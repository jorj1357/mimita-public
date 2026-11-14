# CAP_TRANSPORT server-send pass (2026-11-13)

**Status:** source changes only. B did not claim a load, runtime, or human
result. A owns whole-game verification.

## What changed

- `CAP_TRANSPORT` version is now **3**. `TransportCapabilityV1` gained a
  `send_to_endpoint` function after `peer_count`:
  plain network-order IPv4 address and port, channel, type id, flags, payload.
  No `SOCKET`, `sockaddr_in`, or transport object crosses the ABI.
  `TransportMessageV1` now carries the resident `serverTick` so inbound game
  handlers can respond without a second tick lookup.
- `TransportAdapterV1` version is now **3** and gained the matching host-side
  `send_to_endpoint` and `send_to_peer` functions. `CAP_TRANSPORT::send(peer)`
  now resolves to `send_to_peer` on the server adapter and `send_to_server` on
  the client adapter.
- `HostRuntime` serves the new op through `transportSendToEndpointThunk`.
  `HostServerTransportAdapter` owns a resident endpoint sender; `server.cpp`
  sets it in both the probe and normal server loops (raw UDP send stays in the
  resident host).
- `mimita::game::sendToEndpoint(...)` and `sendToPeer(...)` forward to the
  capability.
- `MimitaNet::serverSendToEndpoint(...)`: resident fallback in
  `server-ice.cpp`, DLL bridge in `network-compat-bridge-module.cpp`.
- `MimitaNet::serverSendToPlayer(...)` in the DLL bridge now derives `typeId`
  from `PacketHeader.type` instead of passing `0`.
- `HostServerTransportAdapter` records the current inbound peer endpoint for
  the duration of a synchronous dispatch (`noteInbound`/`clearInbound`);
  `send_to_peer` first resolves an authenticated player by connection id, then
  falls back to that inbound endpoint. No unbounded peer table is added.

## Raw sendto accounting

| Location | Count | New owner |
|---|---:|---|
| `server-attack.cpp` | 2 | `serverSendToPlayer` |
| `server-npcs.cpp` | 3 | `serverSendToPlayer` |
| `server-packet-chat.cpp` | 5 | `serverSendToPlayer` |
| `server-packet-handlers.cpp` | 4 player + 1 pre-auth | `serverSendToPlayer` / `serverSendToEndpoint` |
| `server-packets.cpp` | 5 player + 1 pre-auth | `serverSendToPlayer` / `serverSendToEndpoint` |
| `server-physical-contact-actors.cpp` | 1 | `serverSendToPlayer` |
| `server-projectiles.cpp` | 1 | `serverSendToPlayer` |
| `server-ice.cpp` | 2 | resident `serverSendToPlayer` / `serverSendToEndpoint` |
| `server.cpp` | 5 | resident probe, player sender, endpoint sender |

Unchanged: packet IDs, packet structs, encode/decode, skip-the-shooter rules,
per-recipient `++totalPacketsOut`, retransmit queues, and disconnect behavior.

## A-owned verification

1. Host build through the root dev loop (the ABI version change requires the
   cold EXE build).
2. Whole-game candidate worker and link; expect 0 undefined / 0 unique.
3. Load the candidate with `MIMITA_GAME_PROBE=1`, `MIMITA_GAME_OWN_FRAME=1`,
   and `MIMITA_GAME_DLL`.
4. Exercise a dedicated server + client: ping (pre-auth endpoint op), chat,
   shots/pellets, NPC projectile spawn, spawn activation, and a disagreement
   retransmit.
5. In the exact `events.jsonl`, prove the sends reach clients and that no
   `sendto` exists in game-owned sources.

## Client outbound follow-up (same pass)

- Added `mimita::game::sendToServer(typeId, data, size)` to
  `transport-bridge-module`; it calls `CAP_TRANSPORT::send` with peer `0`.
- `multiplayer-packets.cpp` now tries that path first under `MIMITA_GAME_DLL`
  in `mpSendPacket`, `mpShutdown` (disconnect packet), and
  `sendReconnectRequest`. The existing resident transport/socket path remains
  as the fallback, so EXE behavior is unchanged.
- Packet bytes, IDs, ordering, bad-connection processing, counters, and the
  `packetsSent` / `lastPacketSentMs` updates are preserved.

## Inbound path (design T1/T2, same pass)

Inbound now uses the generic opaque message path:

- `server.cpp` fills `TransportMessageV1`/`TransportDispatchV1.typeId` from the
  `PacketHeader` type it already inspects, plus `peer`, payload, and
  `serverTick`.
- Both the dedicated and listen-server loops try the command dispatch envelope
  (722) first, then the generic message path (721). Unhandled packets fall
  through to the existing resident `processServerPacket`, so unknown/invalid
  packets keep the resident validation and error behavior.
- `dispatchPingMessage` validates `PACKET_PING` (magic, version, size) and
  answers the same peer through `mimita::game::sendToPeer`, i.e. through
  `CAP_TRANSPORT`, not through a resident socket.
- After a packet is handled by either game path, the host applies the resident
  liveness rule (`lastHeardMs`, non-`PACKET_HELLO`, source-owned player only),
  preserving timeout behavior without trusting unvalidated packets. The game
  logs `game.transport.ping` at most once per 5 seconds with peer, player,
  tick, and send result.
- The resident `HostServerTransportAdapter` owns the peer/endpoint resolution;
  no `SOCKET` or `sockaddr_in` is passed into the game for the migrated path.

## CAP_ENTITY / CAP_WORLD expansion and chat migration (same pass)

### CAP_ENTITY v2

- Added `EntityCapabilityV1::list(hostData, kind, out, capacity)` and the
  matching `EntityAdapterV1::list`. It returns the total count and writes at
  most `capacity` stable handles, so the game can enumerate players/NPCs without
  receiving a C++ container.
- Extended `EntitySnapshotV1` with `yaw`, `onGround`, `transformEpoch`,
  `accountId`, and `pingMs`; `schemaVersion` is now 2. Host read/write maps
  those to/from the resident `ServerPlayer`/`ServerNpc` records.
- Game bridge: `listHostEntities`.

### CAP_WORLD v1 (new)

- `src/host/abi/world-capability.h`: `WorldInfoV1` (loaded, triangle/GLB/spawn
  counts, bounds), `WorldRayHitV1`, `WorldAdapterV1`, `WorldCapabilityV1`.
- `src/host/host-server-world-adapter.*`: resident adapter bound to the
  authoritative `HeadlessWorld`; `info` is O(1) and `raycast` uses a resident
  local copy of the Möller-Trumbore test over the resident triangle list. The
  host does not link against the game-owned `physics/ray-utils.cpp`.
- `HostRuntime::setWorldAdapter`/`clearWorldAdapter` and
  `getCapability(World)`.
- Game bridge: `src/game/world-bridge-module.*` with `readWorldInfo` and
  `castHostWorldRay`. The movement/vehicle handlers are the first intended
  consumers; no handler uses it yet.

### Migrated inbound families

- `dispatchInboundMessage` now routes `PACKET_PING`, `PACKET_CHAT_MESSAGE`, and
  `PACKET_CHAT_TYPING_STATE_REQUEST`.
- Legacy chat reads the sender through `CAP_ENTITY`, sets the server tick, and
  sends to every other enumerated player through `CAP_TRANSPORT::send_to_player`.
- Typing state does the same with the typing event packet.
- Both fall back to the resident handler when validation or sender lookup fails.
  `totalPacketsIn` is incremented for handled packets.
- Chat v2 (`PACKET_CHAT_REQUEST`) still uses the resident reliable-event queue
  and `ServerPlayer` cached-event fields; it is not migrated yet.

## Resident session identity (same pass)

The map id, coordinator code/token, and private-server password now have one
resident owner:

- `src/host/abi/session-identity-capability.h` (`CAP_SESSION_IDENTITY`).
- `src/host/host-session-identity.h/.cpp`: the only storage.
- `src/host/host-runtime.*`: capability thunks.
- `src/game/session-identity-bridge-module.*`: game-DLL bridge.
- `src/network/server-packets.cpp`: the existing getters/setters now delegate
  to the resident owner (EXE directly, DLL through the capability). Direct
  `gServerMapId`/`gServerCoordinatorCode` reads were removed.
- `src/network/server.cpp` and `src/network/server-ice.cpp` write the resident
  values directly instead of calling game-owned setters.
- The password itself never crosses the ABI; the game can only call
  `password_matches`.
- `MatchMode`/`CommunityMode` are part of the same capability.
  `serverGamemodeTick` mirrors them each tick; the join capacity and spawn-NPC
  checks read the resident values instead of the duplicated game static.
- `CAP_TRANSPORT` v4 adds `bind_session(peer, playerId)`; the server adapter
  binds the current inbound endpoint and any claimed transport to an existing
  player.

### CAP_SESSION_EVENTS v1

- `src/host/abi/session-events-capability.h` (new) + `HostCapabilityId::SessionEvents`.
- `src/host/host-server-session-events.*` (new) owns the event-id counter, the
  per-player session-id assignment, and enqueue into the resident
  `ServerPlayer::pendingReliableEvents` queue. Sending uses the registered server
  transport adapter.
- `reliable-gameplay-events.cpp` now takes event ids from the host owner in both
  EXE and DLL; per-player session ids come from the host in DLL builds and from
  the shared resident field in EXE builds.
- The resident `tickReliableGameplayEvents` and ACK handler still own retry/ACK
  behavior; the capability only supplies identity and enqueue.
- Game bridge: `src/game/session-events-bridge-module.*`.

### PACKET_HELLO migration

- `find_connection` detects a duplicate hello; otherwise the host creates a
  player record.
- The game applies unique-name selection, avatar name, host/admin flags,
  reconnect-token rotation, and the resident safe-spawn + transform reset.
- `WelcomePacket` is sent through `CAP_TRANSPORT` to the same peer; a new player
  also gets the reliable "joined the game" chat event.
- Not yet migrated: stored avatar-manifest delivery, shot-serial/spawned resets.
- `PACKET_JOIN_REQUEST` remains resident: it still needs VIP appearance,
  progression ticket, coordinator/password policy state, and avatar manifests.

### Avatar manifests and godball state

- `CAP_ENTITY.send_avatar_manifests(receiver)` sends every other spawned
  player's stored manifest chunks through the resident transport. HELLO and JOIN
  use it, so JOIN no longer has the avatar-manifest debt.
- `PACKET_GODBALL_STATE` now updates the resident godball fields and relays the
  raw packet to every other player through CAP_TRANSPORT.

### Coordinator/VIP JOIN and VIP style

- Snapshot now carries account role, VIP appearance, VIP style detail (bounded
  32 colors), style epoch, and progression ticket.
- `dispatchJoin` performs coordinator validation, VIP ticket verification,
  free-appearance defaults for non-VIP joins, VIP write-back, and a resident-read
  VIP style broadcast to all players.
- Avatar-manifest delivery now runs through
  `CAP_ENTITY.send_avatar_manifests(receiver)`: the host sends every other
  spawned player's stored manifest chunks through the resident transport.
  JOIN no longer has that debt.

### PACKET_CHAT_REQUEST (chat v2)

- Snapshot now carries the resident token-bucket state (`chatTokens`,
  `chatLastUpdateTick`) and the cached retry event fields
  (`lastChatRequestId`, event/message ids, tick, UTC, text, sender name/account).
- `dispatchChatRequest` handles the cached-request retry, empty-message and
  rate-limit rejects, accepted broadcast through `CAP_SESSION_EVENTS`, and writes
  the cache back to the resident record.
- The default token bucket (5 tokens, 1/30 per tick, 300-tick refill cap) matches
  the existing limiter defaults; no `chatconfig.json` loader exists in the repo
  today.

### PACKET_JOIN_REQUEST (local/free path)

- `find_join_token` plus the reconnect-token lookup resolve an existing session;
  otherwise the host creates a player.
- The game applies capacity, password, unique name/avatar, join token, host/admin
  flags, reconnect token, safe spawn/transform reset, `spawned` state, JoinAccept,
  and the reliable join chat.
- Coordinator servers or non-empty VIP tickets fall back to the resident
  handler. Avatar-manifest delivery and VIP style broadcasts are not migrated
  yet (logged as debt).

### CAP_ENTITY v6: reconnect

- Snapshot now carries fixed-width avatar/join/reconnect tokens, previous-token
  validity, spawn generation, kills/deaths, reliable session id, and join-token
  validated flag.
- `find_connection(peer)` and `find_reconnect_token(token, includePrevious)`
  resolve the player without scanning a resident container in the game.
- `reset_transform` gained `awaitAck` so reconnect can reset the epoch while
  clearing the acknowledgement requirement.
- `CAP_SESSION_EVENTS` v2 adds `reset_player` (clear pending reliable events and
  rotate the session id).
- `PACKET_RECONNECT_REQUEST` now runs in `dispatchInboundMessage`: token lookup,
  bind session, token rotation/previous-token window, session reset, transform
  reset, and up to four ReconnectAccept copies through `CAP_TRANSPORT`. Missing
  tokens and failures fall back to the resident handler.

### CAP_WORLD v2: safe-spawn selection

- `WorldSpawnV1` + `pick_spawn(playerId, out)`; authored-index, derived-probe,
  host-override, and unvalidated-authored fallback paths are preserved.
- `src/host/host-server-spawn.*` (new) is a resident copy of the spawn policy
  and capsule-fit/raycast checks. It scans resident triangles directly, so the
  host does not link the game-owned collision helpers.
- `HostServerWorldAdapter` owns the spawn override, refreshed from
  `serverGameOverrides()` each fixed tick in both server loops.
- Game bridge: `pickHostSpawn`.

### CAP_ENTITY v5: disconnect removal

- `remove(handle)` closes the player's transport and erases the resident
  player/NPC record.
- `PACKET_DISCONNECT` now runs in `dispatchInboundMessage`: read sender, remove
  the record, then queue a reliable "left the game" chat event to the remaining
  players through `CAP_SESSION_EVENTS`. Invalid/missing senders fall back.
- Debt: the resident path's actor-preset cleanup and session-removal log are not
  invoked by the migrated path; player ids are not reused.

### CAP_ENTITY v4: transform reset + teleport

- `reset_transform(handle, position, velocity, yaw, onGround,
  invulnerabilityTicks, maxAirJumps)` applies the authoritative transform,
  movement-lifecycle reset, input-buffer reset, and broadcast interpolation
  reset on the resident player record. The host uses a local size-scale clamp;
  no game-owned helper is linked.
- `PACKET_TELEPORT_REQUEST` now runs in `dispatchInboundMessage`: validation,
  dead/finite/sanity checks, then the bounded host transform reset. The resident
  handler is skipped when the game handles it.

### CAP_ENTITY v3 and NPC spawn

- `EntityCapabilityV1` v3 adds `create(kind, outHandle)`. The host entity
  adapter is bound to the server's `nextPlayerId`/`nextEntityId` and creates a
  default player/NPC record.
- `EntitySnapshotV1` now includes `difficulty`; NPC writes also update
  `displayName`.
- `PACKET_SPAWN_NPC_REQUEST` now runs in `dispatchInboundMessage`: sender check,
  melee live-NPC cap via entity enumeration, host-side NPC creation, then a
  bounded snapshot write for position/difficulty/name. The resident handler is
  skipped when the game handles it.

Verified: EXE build `2026-11-13T15:42:37.167598Z`; static gate 0/0. The DLL-side
`dispatchSpawnNpc` needs A's whole-game worker build.

## Join/reconnect remaining blockers (inspected 2026-11-13)

Identity/password state is now single-owner, but `handleHello`,
`handleJoinRequest`, and `handleReconnectRequest` still depend on:

- The full `serverGamemodeState()` struct is still game-owned and duplicated;
  only `matchMode`/`communityMode` are mirrored to the resident session state.
  Anything else the join path reads from that struct is still unsafe.
- Coordinator/VIP validation and the player cached-chat retry fields.
- `serverPickSafeSpawn` / `beginAuthoritativeTransform` and the transform/spawn
  reset path.

Event/session identity and enqueue are now bounded resident operations. The
remaining pieces are the transform/spawn reset helper and the chat-v2 cached
retry fields.

## Remaining B-lane debt (not done here)

- The other inbound handlers (`join`, `reconnect`, `input`, `shot`, chat v2,
  avatar assets, vehicle input/seat, godball/ragdoll, gamemode) still run
  through `processServerPacket` with `SOCKET`/`sockaddr_in`. CAP_ENTITY v2 and
  CAP_WORLD v1 now expose the required read/enumerate/raycast foundation, but
  each family still needs its specific fields/ops:
  - join/reconnect: player create/bind/transport-session and spawn placement;
  - input/movement: full movement state, ground/ray results, NPC writes;
  - shot/attack/melee: combat state, projectile create/tick, damage authority;
  - chat v2: reliable gameplay-event queue and per-player retry cache;
  - avatar/vehicle/godball/ragdoll: per-family resident record fields.
  These are sequential family migrations, not a wire-format change.
- The standalone `client.cpp` path still uses the resident socket capability
  (`sendToSocket`), not `CAP_TRANSPORT`; it is a direct-connect tool path, not
  the active multiplayer session.
- `udp-transport.h` and ICE remain the resident transport mechanism by design.

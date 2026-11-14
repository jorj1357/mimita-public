# Agent B: outbound and inbound packet paths through CAP_TRANSPORT

Session date: 2026-11-13 08:34 local. Scope: Agent B lane on the shared
checkout. Converted game-owned server packet sends to `CAP_TRANSPORT`, added the
pre-auth endpoint send, routed client outbound through the host session, and
moved inbound ping onto the generic `on_event(kEventTransportMessage)` path.
Build and runtime acceptance are Agent A's; B did not perform the ownership
cutover.

## Source changes

- `src/host/abi/transport-capability.h`
  - `kTransportCapabilityVersion` 1 -> 3.
  - Added `send_to_endpoint(hostUserData, addressNetworkOrder,
    portNetworkOrder, channel, typeId, flags, payload, payloadSize)`.
    Address/port are plain integers; no socket or `sockaddr_in` crosses.
  - `TransportMessageV1` now carries `serverTick` (and reserved padding).
- `src/host/transport-adapter.h`
  - `kTransportAdapterVersion` 1 -> 3.
  - Added the host-internal `send_to_endpoint` and `send_to_peer` functions.
- `src/host/host-runtime.h/.cpp`
  - Added `transportSendToEndpointThunk` and exposed it through
    `getCapability(Transport)`.
  - `transportSendThunk` now prefers `send_to_peer` for the server adapter and
    falls back to `send_to_server` for the client adapter.
- `src/host/host-server-transport-adapter.h/.cpp`
  - Added `setEndpointSender`, `noteInbound`, `clearInbound`, and the resident
    endpoint/peer send thunks. `send_to_peer` resolves an authenticated player
    by connection id, then the current inbound endpoint. No unbounded peer
    table is added.
- `src/host/host-transport-adapter.cpp`
  - Client adapter explicitly leaves `send_to_endpoint`/`send_to_peer` unset.
- `src/game/transport-bridge-module.h/.cpp`
  - Added `mimita::game::sendToEndpoint`, `sendToPeer`, and
    `sendToServer(typeId, data, size)`, all through the capability.
- `src/game/network-compat-bridge-module.cpp`
  - `serverSendToPlayer` now passes `PacketHeader.type` instead of `0`.
  - Added the DLL-side `MimitaNet::serverSendToEndpoint`.
- `src/network/server.h`, `src/network/server-ice.cpp`
  - Declared/implemented the resident `serverSendToEndpoint` fallback.
- `src/network/server.cpp`
  - Both server loops install an endpoint sender that owns the raw UDP send.
- `src/network/server-attack.cpp`, `server-npcs.cpp`, `server-packet-chat.cpp`,
  `server-packet-handlers.cpp`, `server-packets.cpp`,
  `server-physical-contact-actors.cpp`, `server-projectiles.cpp`
  - Replaced `if (player.transport) ... else sendto(...)` with
    `serverSendToPlayer(...)`.
  - Replaced the two pre-auth `sendto` replies with
    `serverSendToEndpoint(...)`.

- `src/network/multiplayer-packets.cpp`
  - `mpSendPacket`, `mpShutdown`, and `sendReconnectRequest` now try
    `mimita::game::sendToServer` first under `MIMITA_GAME_DLL`; the resident
    transport/socket path remains the fallback, so EXE behavior is unchanged.
- `src/network/server.cpp`
  - Both server loops build an opaque `TransportMessageV1` with peer, type id,
    payload, and `serverTick`.
  - Probe inbound flow: note the peer endpoint, try the command dispatch
    envelope (722), then the generic message path (721); unhandled packets fall
    through to `processServerPacket`.
  - After either game path handles a packet, the host applies the resident
    liveness rule (`lastHeardMs`, non-`PACKET_HELLO`, source-owned player only).
    Unvalidated packets cannot refresh liveness.
  - Probe ping responses are journaled at most once per 5 seconds, so keepalive
    traffic does not flood `events.jsonl`.
- `src/game/server-command-dispatch-module.cpp`, `.h`, and
  `src/game/game-module.cpp`
  - Added `dispatchPingMessage`: validates `PACKET_PING` magic/version/size and
    answers the same peer through `mimita::game::sendToPeer`.
  - `gameEvent` now handles both `kEventTransportMessage` (generic inbound) and
    `kEventTransportDispatch` (command response). Unhandled or invalid packets
    return `false`, so the resident validation/handler path still runs.

## CAP_ENTITY v2, CAP_WORLD v1, and chat migration

- `src/host/abi/entity-capability.h`, `src/host/abi/game-host-abi.h`
  - `EntityCapabilityV1`/`EntityAdapterV1` v2: added `list(kind, out, capacity)`
    for bounded player/NPC enumeration.
  - `EntitySnapshotV1` gained `yaw`, `onGround`, `transformEpoch`, `accountId`,
    `pingMs`; schema version 2.
- `src/host/host-server-entity-adapter.*`
  - Implemented `listThunk`; read/write the new snapshot fields.
- `src/host/host-runtime.*`, `src/game/entity-bridge-module.*`
  - Added `entityCapabilityListThunk` and `listHostEntities`.
- `src/host/abi/world-capability.h` (new), `src/host/host-server-world-adapter.*`
  (new)
  - Read-only `WorldInfoV1`, `WorldRayHitV1`, `WorldCapabilityV1`.
  - Resident adapter bound to the authoritative `HeadlessWorld`; `raycast` uses
    a resident local copy of the Möller-Trumbore test over the resident triangle
    list, so the host does not depend on the game-owned ray utility.
- `src/host/host-runtime.*`, `src/game/world-bridge-module.*` (new),
  `src/game/game-module.cpp`
  - `setWorldAdapter`/`clearWorldAdapter`, `getCapability(World)`, and
    `readWorldInfo`/`castHostWorldRay`. No handler consumes it yet.
- `src/network/server.cpp`
  - Binds the world adapter in both server loops and clears it on shutdown.
- `src/game/server-command-dispatch-module.cpp`
  - `dispatchInboundMessage` routes ping, legacy chat, and typing.
  - Legacy chat/typing enumerate players through CAP_ENTITY and send through
    CAP_TRANSPORT; sender existence comes from the entity snapshot. Invalid
    packets and missing senders fall back to the resident handler.
  - `totalPacketsIn` is counted for handled packets. Chat v2 (reliable queue)
    remains resident.

## Resident session identity (single owner)

- `src/host/abi/session-identity-capability.h` (new) and
  `HostCapabilityId::SessionIdentity`.
- `src/host/host-session-identity.h/.cpp` (new): the only storage for map id,
  coordinator code, coordinator join token, password-protected flag, and the
  password. `password_matches` is the only password query; the password never
  crosses the ABI.
- `src/host/host-runtime.*`: capability thunks.
- `src/game/session-identity-bridge-module.*` (new) and `game-module.cpp`:
  game-DLL bridge and initialization.
- `src/network/server-packets.cpp`: removed the file-static identity/password
  copies. The existing getters/setters now use the resident owner (EXE direct,
  DLL through the capability), and direct `gServerMapId`/
  `gServerCoordinatorCode` reads were replaced with the getters.
- `src/network/server.cpp`, `src/network/server-ice.cpp`: write the resident
  values directly instead of calling game-owned setters.

This unblocks join/reconnect identity reads. `CAP_SESSION_IDENTITY` also carries
`MatchMode`/`CommunityMode`; `serverGamemodeTick` mirrors them each tick and the
join capacity / spawn-NPC checks read the resident values.

- `CAP_TRANSPORT` v4 adds `bind_session(peer, playerId)`. The server adapter
  binds the current inbound endpoint and any claimed transport to an existing
  player. `game/transport-bridge-module` exposes `bindSession`.
- `CAP_ENTITY` v3 adds `create(kind, outHandle)`; the host adapter is bound to
  `nextPlayerId`/`nextEntityId` and creates default records. `EntitySnapshotV1`
  gained `difficulty`.
- `PACKET_SPAWN_NPC_REQUEST` is migrated: the game checks the sender, applies
  the melee live-NPC cap through entity enumeration, creates the NPC on the
  host, and writes position/difficulty/name through the snapshot. Invalid or
  failed requests fall back to the resident handler.

The remaining join/reconnect blockers are the reliable-event session queue and
the spawn/transform helpers.

## Avatar manifests and godball state

- `CAP_ENTITY.send_avatar_manifests(receiver)` delivers stored manifest chunks
  through the resident transport; HELLO/JOIN use it.
- `PACKET_GODBALL_STATE` is migrated: update resident godball fields, relay the
  raw packet to other players.

## Coordinator/VIP JOIN

- Snapshot carries account role, VIP appearance/style detail (bounded colors),
  style epoch, and progression ticket.
- JOIN now validates the coordinator token, verifies VIP tickets, applies free
  defaults for non-VIP joins, writes VIP state, and broadcasts VIP styles from
  resident snapshots.
- `CAP_ENTITY.send_avatar_manifests(receiver)` delivers stored manifest chunks
  through the resident transport; HELLO and JOIN use it.

## PACKET_CHAT_REQUEST migration

- Snapshot carries resident chat token-bucket + cached retry event fields.
- `dispatchChatRequest` handles retries, empty-message/rate-limit rejects,
  accepted reliable broadcast, and cache write-back.
- Default bucket values match the existing limiter defaults.

## PACKET_JOIN_REQUEST local/free migration

- `find_join_token` added; capacity/password/name/avatar/host/join-token/
  reconnect-token/spawned handling moved into `dispatchJoin`.
- Local (`coordinator == LOCAL`, empty VIP ticket) joins now use safe spawn +
  transform reset, JoinAccept through CAP_TRANSPORT, and reliable join chat.
- Coordinator/VIP joins fall back to the resident handler. Avatar-manifest
  delivery and VIP style broadcasts remain resident debt.

## PACKET_HELLO migration

- Duplicate hellos resolve through `find_connection`; new players are created by
  the host.
- The game applies unique name/avatar, host/admin flags, reconnect-token
  rotation, safe spawn + transform reset, sends `WelcomePacket` through
  CAP_TRANSPORT, and queues the reliable join chat.
- Avatar-manifest delivery and shot-serial/spawned resets remain resident debt.
- JOIN_REQUEST remains resident pending VIP/progression/coordinator/avatar state.

## CAP_ENTITY v6 reconnect

- Snapshot gained avatar/join/reconnect tokens, previous-token validity,
  spawn generation, kills/deaths, reliable session id, and join-token flag.
- `find_connection` and `find_reconnect_token` added.
- `reset_transform` gained `awaitAck`.
- `CAP_SESSION_EVENTS` v2 adds `reset_player`.
- `PACKET_RECONNECT_REQUEST` is migrated: token lookup, session bind, token
  rotation/grace window, session reset, transform reset, and ReconnectAccept
  copies. Failures fall back to the resident handler.

## CAP_WORLD v2 safe spawn

- `WorldSpawnV1` + `pick_spawn(playerId, out)`.
- `src/host/host-server-spawn.*` (new): resident spawn policy with local
  ray-triangle, point-inside, capsule-fit, authored/probe/override/fallback
  paths. No game-owned collision helper is linked.
- `HostServerWorldAdapter` owns the spawn override, refreshed from
  `serverGameOverrides()` each fixed tick in both server loops.
- `game/world-bridge-module` exposes `pickHostSpawn`.

## CAP_ENTITY v5 disconnect removal

- `EntityCapabilityV1`/`EntityAdapterV1` v5 add `remove(handle)`; the host closes
  the player's transport and erases the resident record.
- `PACKET_DISCONNECT` is migrated: read sender, remove, then queue the reliable
  "left the game" chat event to the remaining players through CAP_SESSION_EVENTS.
- Actor-preset cleanup/session-removal logging remain resident-path debt.

## CAP_ENTITY v4 transform reset and teleport

- `EntityCapabilityV1`/`EntityAdapterV1` v4 add `reset_transform`. The host
  adapter applies the authoritative transform, increments the epoch, resets
  accepted-client/input/movement/broadcast-interpolation state, and optionally
  revives with invulnerability. A local size-scale clamp avoids linking a
  game-owned helper.
- `game/entity-bridge-module` exposes `resetHostTransform`.
- `PACKET_TELEPORT_REQUEST` is migrated: validation, dead/finite/sanity checks,
  then the host transform reset. Failures fall back to the resident handler.

## Reliable-event identity/enqueue

- `src/host/abi/session-events-capability.h` (new) + `HostCapabilityId::SessionEvents`.
- `src/host/host-server-session-events.*` (new): resident event-id counter,
  per-player session-id owner, and enqueue into
  `ServerPlayer::pendingReliableEvents`; sends through the registered server
  transport adapter.
- `src/game/session-events-bridge-module.*` (new) + `game-module.cpp`.
- `reliable-gameplay-events.cpp` takes event ids from the host owner in both
  EXE and DLL builds; DLL per-player session ids go through the capability.
- Retry/ACK behavior remains in the resident tick/ACK path.

## Escrow tool (separate from the game build)

Added `tools/escrow/` as a standalone C++17 utility, outside `src/` so the game
build does not compile it:

- `include/escrow/escrow.hpp`, `src/escrow.cpp`, `src/main.cpp`.
- CMake project, `Makefile`, example config, README, and standalone tests.
- JSON store with atomic replace, append-only JSONL event log, integer cents,
  fee-bps release, refund, deadline expiry, admin pause, and `--op-id`
  idempotency.
- Vendored `include/nlohmann/json.hpp`; static MinGW link so no runtime DLLs
  are needed.
- Tests pass (`ctest`), and a CLI smoke run proved create/fund/release,
  idempotent replay, balances, status, list, and events.

Result: 0 raw `sendto` in game-owned network sources; 7 remain in the resident
`server.cpp` / `server-ice.cpp` transport owner. Server outbound, client
outbound, inbound command, inbound ping, legacy chat, and typing now cross
`CAP_TRANSPORT` in the replaceable generation. Packet IDs, schemas, ordering,
skip rules, counters, and retransmit queues are unchanged.

## Build evidence observed (not acceptance)

- Shared dev-loop build published `2026-11-13T13:27:58.826703Z` with the current
  host sources before the final game-module wiring.
- Whole-game worker linked generation 72 at revision
  `9654259f37fc8012b2e4381c957e53a7b2e997560f17d3ce9541a1f4c860e8ea`
  (`build/game/generation-72/game-g72.dll`) after this source pass.
- A later worker run was aborted because another lane changed sources during
  the build. A should rebuild after the tree settles and treat only that build
  as the acceptance build.
- After the client outbound edit, the shared dev-loop published EXE build
  `2026-11-13T13:41:58.706963Z`; `network_multiplayer-packets.o` was rebuilt
  from the edited source.
- After the inbound 721 + `send_to_peer` edit, the shared dev-loop published EXE
  build `2026-11-13T14:01:14.878155Z`; `network_server.o`,
  `host_host-runtime.o`, and `host_host-server-transport-adapter.o` were rebuilt
  from the edited sources.
- After the final liveness-placement edit, the shared dev-loop published EXE
  build `2026-11-13T14:07:20.022935Z`; `network_server.o` was rebuilt.
- After the disconnect/entity-remove pass, the shared dev-loop published EXE
  build `2026-11-14T00:32:47.045318Z` with
  `host_host-server-entity-adapter.o` and `host_host-runtime.o` rebuilt; static
  gate 0/0; escrow tests 10/10.
- After the safe-spawn/reconnect/HELLO prerequisite pass, the shared dev-loop
  published EXE build `2026-11-14T12:51:55.013636Z`; static gate 0/0.
- After the local/free JOIN pass, the shared dev-loop published EXE build
  `2026-11-14T13:09:43.795325Z`; `host_host-server-entity-adapter.o` and
  `host_host-runtime.o` rebuilt; static gate 0/0.
- After the chat-v2 pass, `host-server-entity-adapter.o` and
  `network_server.o` compiled.
- The shared EXE later went green at `2026-11-14T13:54:46.160916Z`. The whole-game
  worker linked generation 127 at revision
  `e26bd83f5de521fc2366e4b2150c566c4afce2c3b203b0fdd380d8de3d5048e8`
  (`build/game/generation-127/game-g127.dll`), including the new dispatch code.
- Runtime load/initialize: dedicated server PID 17028 wrote
  `game.host.probe.started` (ABI 1, generation 1) for generation 127 in
  `logs/2026-11-14/probe-gen127c/events.jsonl`.
- Runtime JOIN attempt: a direct `--client --connect` run did not connect
  (`players=0`); the client journal stops after map/audio load. No joined/chat
  behavior is claimed. A proper dev-loop client run is still required.
- After the avatar-manifest + godball pass, the whole-game worker linked
  generation 131 (`build/game/generation-131/game-g131.dll`). Dedicated server
  PID 22500 wrote `game.host.probe.started` (ABI 1) in
  `logs/2026-11-14/probe-gen131/events.jsonl`.
- Static gate is currently **1 violation** in
  `src/render/render-command-legacy.cpp` (`glGenerateMipmap`), owned by the
  render lane, not this work.
- After the entity/world/chat edits, the shared dev-loop compiled
  `host_host-server-world-adapter.o`, `host_host-server-entity-adapter.o`,
  `host_host-runtime.o`, and `network_server.o`, then failed on an unrelated
  concurrent `src/main-systems.cpp` lambda-capture error owned by another lane.
  No B-owned file reported an error.
- After the other lane fixed `main-systems.cpp`, the shared dev-loop published
  EXE build `2026-11-13T14:36:35.301681Z` with all B-owned sources. This is
  compile evidence only; the `MIMITA_GAME_DLL` side needs A's whole-game worker
  build.

## Not proved

- No probe load, tick/render ownership, generation swap, or rollback was
  claimed for this pass.
- No dedicated server/client run, `events.jsonl` read, or human acceptance.
- No packet-ID, schema, ordering, or retransmit change was observed or claimed.

## Next

1. A: rebuild the host and whole-game candidate after the tree is quiet; confirm
   0 undefined / 0 unique and the `game.host.probe.started` event.
2. A: run a probe server + client and exercise ping, chat, shots, NPC
   projectiles, spawn activation, and a disagreement retransmit; read the exact
   `events.jsonl`.
3. B follow-up: session identity/password, match/community mode, transport
   session bind, and bounded entity creation now exist. Join/reconnect still
   need the reliable-event session queue and a bounded transform-reset/spawn
   helper. Then move `PACKET_HELLO`, `PACKET_JOIN_REQUEST`, and
   `PACKET_RECONNECT_REQUEST`.

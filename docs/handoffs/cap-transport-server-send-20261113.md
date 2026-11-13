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

## Remaining B-lane debt (not done here)

- The other inbound handlers (`join`, `reconnect`, `input`, `shot`, `chat`,
  avatar assets, vehicle input/seat, godball/ragdoll, gamemode) still run
  through `processServerPacket` with `SOCKET`/`sockaddr_in`. Moving them needs
  the resident player/NPC/world records exposed as stable handles
  (`CAP_ENTITY`/`CAP_WORLD` expansion); it is not a wire-format change.
- The standalone `client.cpp` path still uses the resident socket capability
  (`sendToSocket`), not `CAP_TRANSPORT`; it is a direct-connect tool path, not
  the active multiplayer session.
- `udp-transport.h` and ICE remain the resident transport mechanism by design.

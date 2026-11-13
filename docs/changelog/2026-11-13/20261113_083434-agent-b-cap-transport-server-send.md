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

Result: 0 raw `sendto` in game-owned network sources; 7 remain in the resident
`server.cpp` / `server-ice.cpp` transport owner. Server outbound, client
outbound, inbound command, and inbound ping now cross `CAP_TRANSPORT` in the
replaceable generation. Packet IDs, schemas, ordering, skip rules, counters, and
retransmit queues are unchanged.

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
  build `2026-11-13T14:07:20.022935Z`; `network_server.o` was rebuilt. This is
  compile evidence only. The `MIMITA_GAME_DLL` side needs A's whole-game worker
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
3. B follow-up blocked on resident state: the remaining inbound handlers
   (`join`, `reconnect`, `input`, `shot`, `chat`, avatar, vehicle, godball,
   ragdoll) need the resident player/NPC/world records as stable handles.
   Expand `CAP_ENTITY`/`CAP_WORLD`, then move those handlers off
   `SOCKET`/`sockaddr_in` one family at a time.

# Opaque transport design (`CAP_TRANSPORT`, 2026-11-12)

**Status:** design + T0 scaffold + T1 send-side bridge. The host capability, client adapter, and game-side `mpSendServerCommand` host path are implemented and compiled; the server handler is still resident. No runtime evidence.

## Why

The remaining game-DLL link errors are dominated by resident networking:

| Family | References |
|---|---:|
| `MimitaNet` | 120 |
| `IceAgent` | 59 |
| `DuelQueue` | 43 (matchmaking/session, not packet transport) |

The goal is not "make sockets reloadable." It is: **the host keeps the transport mechanism; `game.dll` owns the packet catalog and networking behavior.** Adding or removing a packet should be a `game.dll` change, with no EXE rebuild.

## Boundary

Resident host owns:

- sockets, UDP, ICE, framing, encryption, channel/reliability/ordering machinery;
- peer map, session identity, reconnect token, timeouts, overflow limits;
- `MultiplayerContext`, `ServerPlayer`, `ListenServerState`, `IceAgent` storage;
- transport event delivery and backpressure.

`game.dll` owns:

- packet ids, wire field schema, encode/decode, handlers;
- map-sync, join/reconnect/session policy, snapshot content, spawn authority, combat validation;
- error/outage policy decisions;
- terminal-first control behavior for networking commands.

ABI shape: `src/host/abi/transport-capability.h`.

```text
host -> game:  TransportMessageV1 (opaque typeId + bytes) via on_event(721)
game -> host:  send(peer, channel, typeId, flags, bytes)
               broadcast(channel, typeId, flags, bytes)
               send_to_player(playerId, typeId, flags, bytes)
               session_state / peer_state / peer_count
```

The inbound payload pointer is valid only for the call. The game copies if it retains it. No `SOCKET`, `sockaddr`, `std::string`, STL container, or game-DLL-owned pointer crosses or is retained.

## Packet catalog

- Existing static catalog: 79 packet ids, 81 wire structs, 33 protocol constants.
- Packet ids remain stable and are never reused within a live session. A removed packet leaves a tombstone.
- The host routes `{channel, typeId, bytes}` without parsing. The game generation resolves `typeId` to its codec/handler.
- A candidate announces its schema/generation hash; incompatible peers stay on the prior generation or are disconnected.

## Terminal-first requirement

The dedicated server, client, and command path must be runnable entirely from the terminal. Terminal commands are the primary control surface; the GUI is optional. `PACKET_SERVER_COMMAND` is the first slice because it is the terminal-driven control path.

## Slices

### T0 — capability scaffold (implemented, no call-site move)

1. `TransportCapabilityV1` is exposed by `HostRuntime::getCapabilityThunk` (`transport-capability.h`).
2. `HostTransportAdapter` (resident, `src/host/host-transport-adapter.*`) adapts the active client `MultiplayerContext` transport: `send_to_server`, `session_state`, `peer_count`; server `send_to_player`/`broadcast` are explicit `false` until a server adapter is registered.
3. `main-systems.cpp` binds the adapter after `gpMpContext`; `main.cpp` clears it before host shutdown.
4. No packet code moves yet.
5. Compiled standalone: `host-runtime.cpp`, `host-transport-adapter.cpp`, `main-systems.cpp`, `main.cpp` all exit 0.
6. Remaining T0 runtime evidence: a probe `TransportMessageV1` round-trip through `on_event(721)`; owner-run.

### T1 — server command packet (send-side bridge implemented)

1. `src/game/transport-bridge-module.cpp` builds `ServerCommandPacket` and calls `CAP_TRANSPORT::send`; `multiplayer-packets.cpp` uses it under `MIMITA_GAME_DLL` with the old path as fallback.
2. The host adapter sends the bytes through the active `MultiplayerContext` transport.
3. The server receive/handler path is still resident; moving it to `game.dll` is the remaining T1 work.
4. Terminal command path must work end-to-end without the GUI.
5. Evidence: two processes, a real terminal command, server journal + client journal, same build identity, no EXE relink. Not run yet.

### T2 — ping

Move `PACKET_PING` as the second small packet. Prove latency/keepalive and reconnect-adjacent behavior.

**Status 2026-11-13 (Agent B, source only):** inbound ping now crosses
`on_event(721)` as an opaque `TransportMessageV1`. The game validates the packet
and answers the same peer through `CAP_TRANSPORT::send(peer)`; the server adapter
resolves the peer to an authenticated player transport or the current inbound
endpoint (`send_to_peer`). `lastHeardMs` is preserved. Build/runtime proof is
A-owned.

### T3 — join / reconnect / error paths

Move `PACKET_JOIN_REQUEST` / `JOIN_ACCEPT` / `JOIN_REJECT`, `RECONNECT_*`, timeout, and disconnect policy. These are critical; they require drop/timeout/reconnect journal evidence.

### T4 — snapshot/input/combat

Move snapshot/input/combat packet ownership after the control path is stable.

### T5 — remove resident symbols

Remove `MimitaNet::serverSendToPlayer`, listen-server transport helpers, and `IceAgent` use from `game.dll`; keep the host capability. `DuelQueue` remains a resident matchmaking service or gets its own bounded capability.

## Error and security rules

- Host enforces max payload, rate, peer validity, and backpressure; `TransportResult` reports `Backpressure` / `TransportError` explicitly.
- Host emits `TransportEventV1` for peer join/leave, timeout, reconnect-required, overflow, and transport error.
- Game decides retry/teardown/reconnect policy; host never silently drops without an event.
- Public/untrusted servers never execute unapproved native code; this remains a trusted-dev mechanism.

## Acceptance

- A new packet can be added, removed, or changed in `game.dll` and activated live without an EXE rebuild.
- Terminal command path works entirely from the terminal.
- Join/reconnect/timeout/error paths show real journal evidence.
- The game DLL no longer references resident socket/ICE/server-send symbols.
- The previous generation stays active on any transport/activation failure.

## Current blockers

- The host transport implementation is not wired; `CAP_TRANSPORT` is only an ABI header now.
- `MultiplayerContext` and `ServerPlayer` are still C++ host objects; T1 needs a stable `send_to_player` capability path.
- Packet move requires `server-packets.cpp` / `multiplayer-packets.cpp` call-site changes under the game-DLL cutover, not a blind bridge.

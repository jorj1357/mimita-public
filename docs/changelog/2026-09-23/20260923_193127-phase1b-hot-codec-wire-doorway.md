# Phase 1b: generic hot-codec wire doorway + loopback proof

Date (UTC): 2026-09-23T19:31:27Z
Status: implemented; build and selftests verified; live two-process run pending

## Scope

Phase 1b of the hot server/networking migration: connect the Phase 1 codec
registry to the real wire. One opaque packet type carries every hot schema; the
EXE decodes through the active generation's codec and forwards the opaque bytes
to hot code as a generic event. A new schema is still a hot source edit.

## Changes

1. **One generic packet type** (`src/network/packets.h`): `PACKET_HOT_CODEC = 90`.
   Its payload is `[PacketCodecEnvelopeV1][inner payload]`. No per-schema packet
   type.

2. **Generic receive event** (`src/hot-reload/hot-packet-codec.h`):
   `GAME_EVENT_HOT_PACKET = gameHash("net.packet")`. Payload is the raw
   `[envelope][inner payload]` bytes; hot code casts the first
   `sizeof(PacketCodecEnvelopeV1)` bytes and reads the remainder as its schema.

3. **Cold wire framing** (`src/network/packet-codec-wire.{h,cpp}`, new):
   `buildHotCodecDatagram` (header + envelope + payload, with codec generation
   retention on the send path) and `parseHotCodecDatagram` (validate header,
   envelope, checksum, version; decode).

4. **Server dispatch** (`src/network/server-packets.cpp`): added
   `PACKET_HOT_CODEC` to the known-type gate and a branch that parses the
   datagram, dispatches the hot `net.packet` event, and journals
   `server.packet_received` / `server.packet_rejected` with schema id/version and
   the explicit reject reason. Invalid/incompatible packets are securely dropped.

5. **Client dispatch** (`src/network/multiplayer-tick.cpp`): added the matching
   branch that decodes and dispatches the hot `net.packet` event.

6. **Test**: `--packet-codec-selftest` gained a real loopback UDP test: build a
   datagram, `sendto`/`recvfrom` over an actual OS socket, parse it back,
   confirm the decoded payload and packet sequence, and reject a corrupted
   datagram with `ChecksumMismatch`.

## Evidence

- Source changes: `packets.h`, `hot-packet-codec.h`,
  `packet-codec-wire.{h,cpp}`, `server-packets.cpp`, `multiplayer-tick.cpp`,
  `packet-codec-selftest.cpp`.
- Build (source/build evidence):
  - Hot DLL: `python build_game_dll.py` -> `DLL build success`.
  - Cold EXE: `python build_agent.py` -> `Status: SUCCESS`
    (`mimita-20260923T153050.exe`). A `mimita.exe` (pid 5640) was running; the
    uniquely named agent exe was used and the running process was not touched.
- Automated tests (test evidence):
  - `--packet-codec-selftest` -> PASS (22 checks), including
    "wire: hot-codec packet round-trips over loopback UDP" and
    "wire: corrupted datagram is rejected".
  - `--live-code-selftest` -> PASS.
  - `--generation-bootstrap-selftest`, `--transport-generation-selftest`,
    `--capability-selftest` -> PASS.
- Runtime evidence: none. No two-process live packet exchange was observed.
- Human acceptance: pending.

## Not done (next)

- No hot module consumes `net.packet` yet (the hot codec module registers and
  resolves codecs, but no live gameplay schema is routed end to end).
- Generation retention is exposed by `PacketCodecDispatch` on the send path but
  is not yet connected to the reliable-event queue's retransmit/ACK lifecycle.
- No two-process proof that a new schema round-trips server<->client live.

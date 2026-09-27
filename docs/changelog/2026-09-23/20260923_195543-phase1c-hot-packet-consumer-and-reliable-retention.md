# Phase 1c: hot net.packet consumer, reliable retention, two-leg proof

Date (UTC): 2026-09-23T19:55:44Z
Status: implemented; build and selftests verified; live two-process run pending

## Scope

Phase 1c of the hot server/networking migration: close the hot packet-codec
loop. A hot handler consumes `net.packet`, replies through a kernel capability,
in-flight reliable bytes survive a generation swap, and the full two-leg
client -> server -> client path is proven over real loopback UDP.

## Changes

1. **Hot `net.packet` consumer** (`src/hot-reload/modules/packet-codecs.cpp`):
   `onHotPacket` is registered by the runtime event id. It reads the envelope,
   matches the ping schema, and answers through a new kernel capability. No EXE
   knowledge of the schema.

2. **`net.packet-reply` kernel capability** (`hot-packet-codec.h`,
   `server-event-broadcast.{h,cpp}`, `live-behavior.{h,cpp}`):
   `serverPacketReply(connectionId, bytes, size)` sends caller-built bytes back
   to one connection over its transport or UDP address. Registered as an ordinary
   kernel capability; resolved per call. A headless observation sink
   (`LiveBehavior::setPacketReplySink`) lets the self-test see the reply; production
   always uses the socket path.

3. **Reliable retention bridge** (`packet-codec-dispatch.{h,cpp}`,
   `reliable-gameplay-events.cpp`): `retainForEvent`/`decodeRetainedEvent`/
   `retireEvent` keep the exact bytes of an in-flight reliable hot-coded packet
   keyed by reliable event id. `handleReliableEventAck` and the TTL/attempts
   retirement path both call `retireEvent`, so retention is bounded by the
   reliable queue's own ACK/expiry lifecycle.

4. **Full two-leg proof** (`live-code-selftest.cpp`): over two real loopback UDP
   sockets, the client sends a hot-coded ping datagram; the server receives,
   decodes through the hot codec, dispatches `net.packet`; the hot handler replies
   through `net.packet-reply`; the client receives and decodes the reply. All
   stages asserted.

5. `--packet-codec-selftest` gained the reliable-retention bridge checks.

## Evidence

- Source changes: `hot-packet-codec.h`, `modules/packet-codecs.cpp`,
  `packet-codec-dispatch.{h,cpp}`, `packet-codec-wire.{h,cpp}`,
  `reliable-gameplay-events.cpp`, `server-event-broadcast.{h,cpp}`,
  `live-behavior.{h,cpp}`, `live-code-selftest.cpp`, `packet-codec-selftest.cpp`.
- Build (source/build evidence):
  - Hot DLL: `python build_game_dll.py` -> `DLL build success`.
  - Cold EXE: `python build_agent.py` -> `Status: SUCCESS`
    (`mimita-20260923T155513.exe`). A `mimita.exe` was running; the uniquely
    named agent exe was used and the running process was not touched.
- Automated tests (test evidence):
  - `--live-code-selftest` -> PASS, incl. the two-leg loopback round trip:
    "server received", "server decodes", "net.packet dispatched", "client decodes
    the hot-handled reply".
  - `--packet-codec-selftest` -> PASS, incl. reliable retention
    (retain/decode/retire by event id).
  - `--generation-bootstrap-selftest`, `--transport-generation-selftest`,
    `--capability-selftest` -> PASS.
- Runtime evidence: none. No two-process live session was observed.
- Human acceptance: pending.

## Not done

- No shipping gameplay packet has been migrated onto the hot codec path; the
  proof schema (`packet.hot.ping`) is a live round-trip exercise.
- The server/client `PACKET_HOT_CODEC` branches dispatch `net.packet` but no live
  gameplay handler consumes it yet.
- Live two-process run (client downloads/activates the server generation, then a
  hot schema round-trips) remains.

# Stage D: packet schema declaration layer

Date (UTC): 2026-09-23T19:09:00Z
Status: implemented; cold build and selftests verified; hot schema codec resolved

## Scope

Finish Stage D's packet-schema work by making the packet identity a versioned
schema declaration rather than a raw C++ layout dependency, so a header edit is a
schema version bump that hot-rebuilds the codec instead of an unsafe in-place
cast.

## Changes

1. **New `src/hot-reload/hot-packet-schemas.h`**: stable `(schemaId,
   schemaVersion)` declarations for core gameplay schemas
   (`packet.ping`, `packet.hot.ping`). Editing a layout bumps the version and
   keeps the old one registered during the transition — no unsafe cast.

2. **`src/hot-reload/modules/packet-codecs.cpp`**: added a real `packet.ping`
   codec (inner payload `clientTimeMs`) registered through the existing
   `net.packet-codecs` doorway, alongside the existing `packet.hot.ping` proof.
   The canonical `PacketHeader` stays cold framing; only the schema-owned inner
   payload is codec-controlled.

3. **`src/live-code/live-code-selftest.cpp`**: added a check that
   `packet.ping` resolves through the same generic codec lookup.

4. **`src/hot-reload/hot-modules.json`**: added `hot-packet-schemas.h` to
   `headers`.

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T190819.exe`.
- Automated tests (test evidence): `--live-code-selftest` -> PASS with
  `[CAPABILITY_RESOLVED] provider=net.packet-codecs` and
  `[ok] hot packet-schemas resolves packet.ping`.
- Runtime / human acceptance: pending.

## Stage D status

Server packets are policy-complete: explicit packet-type registry, join policy,
session policy (reconnect grace + map-ready), and reload decision are hot; the
schema identity is now versioned. Remaining packet mechanics (movement decode,
spawn-generation/ack bookkeeping, size/source/rate-limit gates) are mechanism,
not policy, and stay cold.

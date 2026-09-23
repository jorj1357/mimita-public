# Stage D finish + Stage E start: packet schema layer and client snapshot-apply

Date (UTC): 2026-09-23T19:11:00Z
Status: implemented; cold build and selftests verified; hot providers resolved and exercised

## Scope

Finish Stage D's packet-schema work and begin Stage E (client networking) with the
client snapshot-apply lifecycle policy.

## Changes

### Stage D: packet schema declaration layer
- New `src/hot-reload/hot-packet-schemas.h`: stable `(schemaId, schemaVersion)`
  declarations; a layout change is a version bump, not an in-place cast.
- `src/hot-reload/modules/packet-codecs.cpp`: registered a real `packet.ping`
  codec (inner `clientTimeMs`) through the existing `net.packet-codecs` doorway,
  alongside `packet.hot.ping`.
- Selftest check: `packet.ping` resolves through the same generic lookup.

### Stage E start: `net.client-snapshot`
- New `src/hot-reload/hot-client-snapshot.h` (POD `GameSnapshotApplyV1` + shared
  `evaluate`) and `src/hot-reload/modules/client-snapshot-policy.cpp`.
- `src/network/multiplayer-tick.cpp`: the local-sample staleness gate
  (older epoch / same-epoch older tick) and the stale-membership create gate are
  now hot. Replica storage, interpolation buffers, and the world stay cold.

### Manifest + selftests
- Added `hot-packet-schemas.h` and `hot-client-snapshot.h` to `headers`.
- Added `packet.ping` and client-snapshot selftest checks.

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T190953.exe`.
- Automated tests (test evidence): `--live-code-selftest` -> PASS with
  `[CAPABILITY_RESOLVED]` for `net.client-snapshot` and all new checks.
- Runtime / human acceptance: pending.

## Stage D status: complete

Server packets are policy-complete (registry, join, session, reload, schema
identity) with only mechanism (decode, gates, spawn/ack bookkeeping) left cold.

## Stage E remaining

- `multiplayer-projectiles.cpp`: prediction/blast/hit-claim policy.
- `reliable-gameplay-events.cpp`: retry/ACK policy (mostly hot already; verify).
- `multiplayer-reconcile.cpp`: classification (mostly hot already; verify).
- `multiplayer-tick.cpp`: further snapshot-apply/interpolation policy.

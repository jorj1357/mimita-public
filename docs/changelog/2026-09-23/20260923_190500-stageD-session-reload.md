# Stage D: hot session handshake + reload decision

Date (UTC): 2026-09-23T19:05:00Z
Status: implemented; cold build and selftests verified; hot providers resolved and exercised

## Scope

Continue Stage D (server packets). Move the remaining session-handshake and
reload-request policy out of cold code behind generic capabilities.

## Changes

### `net.session-policy` (reconnect grace + map-ready)
- New `src/hot-reload/hot-session-policy.h` (POD `GameReconnectGraceV1` +
  `GameMapReadyV1` + shared implementation) and
  `src/hot-reload/modules/session-policy.cpp`.
- `src/network/server-packets.cpp`: the reconnect previous-token grace/rotation
  window and the map-ready spawn-vs-rearm decision are now hot.

### `net.reload-decision` (reload accept/reject ordering)
- New `src/hot-reload/hot-reload-decision.h` (POD `GameReloadDecisionV1` + shared
  `evaluate`) and `src/hot-reload/modules/reload-decision-policy.cpp`.
- `src/network/server-packets.cpp`: `handleReloadRequest` (both the hot-tool and
  migrated-weapon paths) now uses the hot decision for accept/reason ordering
  (dead / magazine-full / no-reserve / already-reloading / begin). Timer arming,
  ammo adoption, the idempotent cache, and the result packet stay cold.

### Manifest + selftests
- Added `hot-session-policy.h` and `hot-reload-decision.h` to `headers`.
- Added live-code selftest checks: session-policy spawn/rearm, reload-decision
  full-magazine reject and begin.

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T190351.exe`. Package now reports
  `providers=25 requirements=20`.
- Automated tests (test evidence): `--live-code-selftest` -> PASS with
  `[CAPABILITY_RESOLVED]` for both new providers and all checks.
  `--hot-combat-selftest` shows the same 25 pre-existing animation/phase2
  failures as before; none reference reload/session/reload-decision.
- Runtime / human acceptance: pending.

## Stage D remaining

- Reload-cache/known-type done/registry done; the reload decision is hot.
- `packets.h`: become a schema declaration layer with per-schema hot codecs
  registered through `net.packet-codecs`; keep old + new versions accepted.
- Remaining server-packet mechanics (movement decode, spawn-generation/ack
  bookkeeping) are mechanism, not policy.

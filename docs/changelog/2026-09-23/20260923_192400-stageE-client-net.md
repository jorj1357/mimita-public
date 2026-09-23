# Stage E: client networking policies

Date (UTC): 2026-09-23T19:24:00Z
Status: implemented; cold build and selftests verified; hot providers resolved and exercised

## Scope

Stage E (client networking) policy. Move the client snapshot-apply and predicted
projectile correction decisions hot; verify reconciliation and reliable retry/ACK
policies were already hot.

## Changes

### `net.client-snapshot`
- New `src/hot-reload/hot-client-snapshot.h` (POD `GameSnapshotApplyV1` + shared
  `evaluate`) and `src/hot-reload/modules/client-snapshot-policy.cpp`.
- `src/network/multiplayer-tick.cpp`: the local-sample staleness gate (older
  epoch / same-epoch older tick) and the stale-membership create gate are hot.
  Replica storage, interpolation buffers, and the world stay cold.

### `net.projectile-correction`
- New `src/hot-reload/hot-projectile-correction.h` (POD
  `GameProjectileCorrectionV1` + shared `evaluate`) and
  `src/hot-reload/modules/projectile-correction-policy.cpp`.
- `src/network/multiplayer-projectiles.cpp`: the predicted-projectile correction
  decision (server update present, error above threshold) and the blend factors
  are hot. Physics simulation, buffers, and presentation stay cold.

### Verified already hot (Stage E scope)
- `multiplayer-reconcile.cpp`: dispatches `GAME_EVENT_RECONCILE` (hot
  reconciliation policy already present).
- `reliable-gameplay-events.cpp`: dispatches `GAME_EVENT_NET_RELIABLE_POLICY`
  (hot retry/ACK/TTL policy already present).
- `multiplayer-tick.cpp`: `GAME_EVENT_GENERATION_POLICY`, connection-state, and
  hot-packet seams already present.

### Manifest + selftests
- Added `hot-client-snapshot.h` and `hot-projectile-correction.h` to `headers`.
- Added selftest checks for both.

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T192312.exe`.
- Automated tests (test evidence): `--live-code-selftest` -> PASS with
  `[CAPABILITY_RESOLVED]` for both new providers and all checks.
- Runtime / human acceptance: pending.

## Stage E status

Client networking policies are hot: snapshot-apply, projectile correction,
reconciliation, and reliable delivery, alongside the pre-existing
generation/connection/input/hot-packet/interpolation/render-forwarding seams.
Remaining client code is mechanism (buffers, prediction math, transport poll).

# One shared air-acceleration implementation for server + prediction

- EST timestamp: 2026-09-15 11:10:31 EDT (UTC 2026-09-15T15:10:31Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--movement-algorithm-selftest` 9/9 +
  full suite 25/25)

## 1. Concurrent movement-state audit
The movement-state agent owns `movement-system.cpp` (local hot `movement.main`),
`MovementRuntimeStateComponent`, and the ability-state model. This pass did not
change that state model; it only routed the file's airborne acceleration math
through the existing shared policy.

## 2-3. Single implementation + client input mapping
- The air algorithm is defined **once**: `MimitaHotMovement::airAccelerate`
  (`modules/movement-air.cpp`), declared in `hot-movement-policy.h`.
- The `movement.air-accelerate` event handler (server authority path) calls it.
- `movement.main` now maps its generic prediction state into the same
  plain-number inputs (velocity, wishDir, wishSpeed=walkSpeed, wishspd=walkSpeed,
  airAcceleration=airAccel, surfaceFriction=1, airSpeedGainMultiplier=1, dt,
  currentSpeed) and calls the SAME function for the airborne case. The previous
  inline airborne blend formula (`vx += (wishDirX*speed - vx)*blend`) is removed
  for the air case; ground acceleration/friction is unchanged (not yet shared).

## 4. Context-free
- `airAccelerate` takes only `GameAirAccelerateV1` numbers; no `ServerPlayer*`,
  `Player*`, `MultiplayerContext*`, renderer, or packet state.

## 5-6. Tests
- `--movement-algorithm-selftest` PASS 9/9, adding: "one shared air
  implementation: event path == shared function" (the event handler reproduces
  the shared formula exactly), plus the existing function-ownership,
  differs-from-cold, determinism, generic-actor, and multi-tick checks.
- Full suite 25/25.

## 7-8. One-edit-changes-both
- Structurally established: one definition (`airAccelerate`) with two call sites
  (server hook handler + `movement.main`). Editing the function changes both
  paths. **Not live-observed** (no running server+client edit performed).

## Status labels
- SELFTEST PROVEN: single shared air-acceleration implementation; event path
  reproduces the shared function; context-free and deterministic; server hook and
  local prediction both reference the one definition.
- COMPILED INTEGRATION: `movement.main` calls the shared policy; full suite 25/25.
- SERVER/CLIENT MOVEMENT PARITY PROVEN: no. Both paths call the same function
  now, but a real path-parity selftest driving `movement.main` through collision
  (and the remaining unshared ground/friction/gravity/jump differences) was not
  run.
- ONE-EDIT-CHANGES-BOTH PROVEN: structural (one definition, two call sites); not
  live-observed.
- LIVE MOVEMENT HOT-EDIT PROVEN: no.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game air movement feel and prediction/authority
  alignment; the local air math changed from an instant blend to the shared
  diminishing-gains formula.

## Honest limits (success-bar gaps)
- #4 real server/client path parity selftest: not run. `movement.main` uses
  `physics.moveCapsule` + `MovementStateV1` while the server uses
  `movement-step` + generic collision, and ground/friction/gravity/jump are still
  separate; only the air algorithm is shared.
- #5/#6 the one-edit-changes-both and parity-after-edit proofs are structural, not
  live.
- The integrator still uses the typed working copy; generic Transform/Velocity
  direct integration remains pending (documented separately, not mixed in here).
- No other movement function (friction/jump/dash/freeze/ground) was migrated.

## Files changed
`src/hot-reload/hot-movement-policy.h` (shared declaration),
`src/hot-reload/modules/movement-air.cpp` (single definition),
`src/hot-reload/modules/movement-system.cpp` (local path calls shared policy),
`src/network/movement-algorithm-selftest.cpp` (single-implementation check);
docs + this changelog.

## Next (auto-selected)
Drive `movement.main` in a real server/client path parity selftest and observe
the one-edit-changes-both behavior; then share the remaining movement functions;
then distributed hot generation delivery / synchronized activation.

# Hot jump-policy ownership

- EST timestamp: 2026-09-15 12:53:03 EDT (UTC 2026-09-15T16:53:03Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--movement-algorithm-selftest` 17/17 +
  full suite 26/26)

Architecture-first pass: migrate jump behavior into one shared hot implementation.

## Report
- SUBSYSTEM: movement jump.
- COLD OWNER REMOVED: `applyBasicJump` (movement-step.cpp) is no longer the sole
  jump owner (now fallback); the duplicated inline jump logic in
  `movement.main` is removed.
- HOT OWNER ADDED: `MimitaHotMovement::jumpPolicy` (new `modules/movement-jump.cpp`),
  payload `GameJumpPolicyV1` + event `movement.jump`. Owns buffer, coyote,
  grounded/air eligibility, air-jump count/arm/lock, scaled impulse, and state
  transitions.
- COMPATIBILITY FALLBACK: built-in `applyBasicJump` logic when no hot handler is
  active.
- STATE AUTHORITY: generic `Velocity` (z) + the existing generic
  `MovementRuntimeStateComponent` (client) / `MovementJumpState` working copy
  (server). No new jump state store; no `PlayerJumpState`.
- SERVER CALL SITE: `applyBasicJump` (movement-step.cpp) dispatches
  `movement.jump` and applies the returned state (incl. `dashAvailable` reset on
  ground jump and `MovementStepEvents`).
- CLIENT CALL SITE: `movement.main` (movement-system.cpp) calls the same function
  for eligibility/air-jump/impulse.
- SELFTEST PROVEN: `--movement-algorithm-selftest` PASS 17/17, adding grounded
  jump, air jump + count decrement, second-air-jump denial, and determinism.
- PARITY STATUS: air-parity `[warn]` unchanged (maxDev 2.2699, first divergent
  tick 68). Jump did not affect the airborne divergence.
- LIVE HOT-EDIT PROVEN: no.
- KNOWN BUGS DEFERRED: air-parity divergence (feel/parity). Also note: the client
  ground jump now leaves `airJumpArmed=0` (matching the server), so an air jump
  requires a release first — a deliberate unification, not a tuned value.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? No. Jump edits are now hot.
- NEXT COLD OWNER: dash/down-dash, then freeze, then the post-acceleration speed
  clamp, then generic integrator authority.

## Ops note
A transient exe link failure (`ld returned 1`) followed by a skipped relink left
a stale `mimita.exe` that crashed the selftest (access violation). Deleting the
exe and rebuilding resolved it; not a code regression. Recorded so it is not
mistaken for one.

## Files changed
`src/hot-reload/hot-movement-policy.h` (jump payload + declaration),
`src/hot-reload/modules/movement-jump.cpp` (new),
`src/physics/movement/movement-step.cpp` (server jump hook + fallback),
`src/hot-reload/modules/movement-system.cpp` (client uses shared jump policy),
`src/network/movement-algorithm-selftest.cpp` (jump checks); docs + this changelog.

## Next (auto-selected)
Dash/down-dash hot (activation, impulse math, cooldown/state, air/ground
differences), then freeze, then the post-acceleration speed clamp, then finish
the generic movement integrator; then reconciliation/interpolation/rewind.

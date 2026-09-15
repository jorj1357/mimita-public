# Hot dash / down-dash policy ownership

- EST timestamp: 2026-09-15 13:04:34 EDT (UTC 2026-09-15T17:04:34Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--movement-algorithm-selftest` 22/22 +
  full suite 26/26)

Architecture-first pass: migrate dash / down-dash behavior into one shared hot
implementation.

## Report
- SUBSYSTEM: movement dash / down-dash.
- COLD OWNER REMOVED: `tryActivateDash`/`tryActivateDownDash` (movement-step.cpp)
  are no longer the sole owners (now fallbacks); the duplicated dash/down-dash
  math in `movement.main` is removed.
- HOT OWNER ADDED: `MimitaHotMovement::dashPolicy` (new `modules/movement-dash.cpp`),
  payload `GameDashPolicyV1` + event `movement.dash`. Owns direction choice (move
  input with camera-forward fallback), ground/air impulse composition,
  availability consumption, and the down-dash vertical response.
- COMPATIBILITY FALLBACK: built-in `tryActivateDash`/`tryActivateDownDash` when no
  hot handler is active.
- STATE AUTHORITY: generic runtime movement state (dash/down-dash availability)
  + generic `Velocity`; plain-number inputs only.
- SERVER CALL SITE: `tryActivateDash`/`tryActivateDownDash` dispatch
  `movement.dash`; the server wrapper keeps momentum-protection, dash-grace, and
  `airJumpsLeft` reset.
- CLIENT CALL SITE: `movement.main` (movement-system.cpp) calls the same function;
  the dash-effect direction is derived from the returned velocity delta.
- SELFTEST PROVEN: `--movement-algorithm-selftest` PASS 22/22 — grounded dash,
  airborne dash, unavailable denial, camera-fallback direction, down-dash.
- PARITY STATUS: air-parity `[warn]` unchanged (maxDev 2.2699, first divergent
  tick 68). Dash did not affect the airborne divergence.
- LIVE HOT-EDIT PROVEN: no.
- KNOWN BUGS DEFERRED: air-parity divergence (feel/parity). Dash cooldown remains
  in the wrappers (not yet owned by the hot policy) — noted for the clamp/policy
  pass.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? No. Dash edits are now hot.
- NEXT COLD OWNER: freeze, then the post-acceleration speed clamp, then generic
  integrator authority.

## Ops note
Concurrent EXE/DLL relinks caused two spurious failures during this pass: a stale
`mimita.exe` crash after a skipped relink, and a transient tick-0 air-parity
failure while the DLL was being rebuilt. Rebuilding/re-running gave stable
results. Recorded so they are not mistaken for regressions.

## Files changed
`src/hot-reload/hot-movement-policy.h` (dash payload + declaration),
`src/hot-reload/modules/movement-dash.cpp` (new),
`src/physics/movement/movement-step.cpp` (server dash hooks + fallback),
`src/hot-reload/modules/movement-system.cpp` (client uses shared dash policy),
`src/network/movement-algorithm-selftest.cpp` (dash checks); docs + this changelog.

## Next (auto-selected)
Freeze hot (activation, duration/state, movement suppression/modification, exit),
then the post-acceleration speed clamp (highest-value air-divergence suspect),
then finish the generic movement integrator; then reconciliation/interpolation/
rewind.

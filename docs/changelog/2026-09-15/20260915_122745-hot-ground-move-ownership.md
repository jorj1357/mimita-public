# Hot ground-move ownership (friction + acceleration shared)

- EST timestamp: 2026-09-15 12:27:45 EDT (UTC 2026-09-15T16:27:45Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--movement-algorithm-selftest` 11/11 +
  full suite 26/26)

Architecture-first pass: migrate one real movement policy owner into shared hot
code, routed from both server authority and local prediction.

## Report
- COLD OWNER REMOVED: the ground friction + acceleration math in
  `applySourceGround` (movement-step.cpp) is no longer the sole owner — it is now
  a fallback behind the hot policy. The duplicate ground math in
  `movement.main` (movement-system.cpp blend + friction) is removed.
- HOT OWNER ADDED: `MimitaHotMovement::groundMove` (new
  `modules/movement-ground.cpp`), declared in `hot-movement-policy.h`; payload
  `GameGroundMoveV1` + event `movement.ground-move`.
- COMPATIBILITY FALLBACK REMAINING: the built-in Source ground math in
  `applySourceGround` runs when no hot handler handles the event (preserves
  last-good behavior if the hot generation fails).
- STATE AUTHORITY: inputs/outputs are plain numbers (velocity, wishDir,
  wishSpeed, groundAcceleration, frictionAmount, stopspeed, dt, hasInput). No
  ServerPlayer/Player/Npc/mode state.
- SERVER CALL SITE: `applyPreCollisionBasicMovement` -> ... ->
  `applySourceGround` (movement-step.cpp) dispatches `movement.ground-move`.
- CLIENT CALL SITE: `movement.main` (movement-system.cpp) grounded branch calls
  `MimitaHotMovement::groundMove` directly.
- SELFTEST PROVEN: `--movement-algorithm-selftest` PASS 11/11 — adds
  "hot ground-move owns the friction + acceleration function" and "friction-only
  function" checks (exact expected outputs).
- LIVE HOT-EDIT PROVEN: no.
- KNOWN BUGS DEFERRED: the tick-84 late air-parity divergence (maxDev 1.61,
  first divergence tick 84) remains recorded by `--air-movement-parity-selftest`
  as a `[warn]`; suspected areas: wish-speed derivation / speed-cap policy /
  velocity preservation. Not chased per the architecture-first priority.
- NEXT COLD OWNER: speed cap / wish-speed derivation (likely eliminates the
  divergence), then gravity, jump, dash/down-dash, freeze.

## Notes
- The local ground branch now applies friction every grounded tick (Source
  semantics) instead of only when there is no input — a deliberate unification,
  not a tuned constant.
- The air policy from the previous pass is unchanged; the parity harness is kept
  as a regression sensor.
- Generic persistent state (Transform/Velocity) and the typed working-copy
  integrator are unchanged this pass (still a known gap; migrated separately).

## Status labels
- SELFTEST PROVEN: hot ground friction+acceleration function ownership; exact
  output checks; single definition referenced by server hook and prediction.
- COMPILED INTEGRATION: server dispatch + client direct call; full suite 26/26.
- SERVER/CLIENT PARITY PROVEN: no (air parity still diverges late; ground parity
  not yet measured).
- LIVE HOT-EDIT PROVEN: no.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game ground movement feel (friction now always
  applied while grounded on the local path).

## Files changed
`src/hot-reload/hot-movement-policy.h` (payload + declaration),
`src/hot-reload/modules/movement-ground.cpp` (new; single implementation),
`src/physics/movement/movement-step.cpp` (server hook + fallback),
`src/hot-reload/modules/movement-system.cpp` (prediction calls shared policy),
`src/network/movement-algorithm-selftest.cpp` (ground checks); docs + changelog.

## Next (auto-selected)
Migrate the speed-cap / wish-speed derivation policy hot (likely removes the
late air divergence as the duplicated policy disappears), then gravity, jump,
dash/down-dash, freeze — one function per pass.

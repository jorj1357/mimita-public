# Hot speed-policy ownership (max speed + wish-speed derivation)

- EST timestamp: 2026-09-15 12:43:47 EDT (UTC 2026-09-15T16:43:47Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--movement-algorithm-selftest` 15/15 +
  full suite 26/26; air-parity harness `[warn]` unchanged)

Architecture-first pass: migrate the speed / wish-speed derivation to one shared
hot implementation.

## Report
- SUBSYSTEM: movement speed cap / wish-speed derivation.
- COLD OWNER REMOVED: `sourceMaxSpeedValue` and the inline air `wishspd`
  derivation in movement-step.cpp are no longer the sole owners (now fallbacks).
  The local prediction path no longer hardcodes `speed = walkSpeed`.
- HOT OWNER ADDED: `MimitaHotMovement::speedPolicy` (new
  `modules/movement-speed-policy.cpp`), payload `GameSpeedPolicyV1` + event
  `movement.speed-policy`: size-scale factor, effective max speed (with fixed
  speed limit), air wish-speed projection cap.
- COMPATIBILITY FALLBACK: built-in `movementSizeScaleFactor`/`sourceMaxSpeedValue`
  math runs when no hot handler handles the event.
- STATE AUTHORITY: plain numbers (baseMaxSpeed, fallback, sizeScale, sizeExponent,
  speedLimit, airMaxWishspeed, rawWishSpeed); no player/NPC/client state.
- SERVER CALL SITE: `sourceMaxSpeedValue` and the air `wishspd` block in
  `movement-step.cpp` dispatch `movement.speed-policy`.
- CLIENT CALL SITE: `movement.main` (movement-system.cpp) derives its max speed
  through the same hot function.
- SELFTEST PROVEN: `--movement-algorithm-selftest` PASS 15/15, adding
  size-scaled max-speed and air wish-speed-cap checks.
- PARITY STATUS: air-parity `[warn]` unchanged (maxDev 2.2699, first divergent
  tick 68). The speed-policy migration did NOT change it → the divergence is not
  the max-speed/wish-speed derivation.
- LIVE HOT-EDIT PROVEN: no.
- KNOWN BUGS DEFERRED: air-parity divergence (feel/parity, not blocking);
  suspected post-acceleration clamp or the client collision pipeline.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? No. Gravity/ground/air/speed policy
  edits are hot; the divergence is a hot-path feel bug.
- NEXT COLD OWNER: jump, then dash/down-dash, then freeze; then the
  post-acceleration speed clamp; then generic integrator authority.

## Audit
- `hot-cold-audit.md`: speed-cap row updated (derivation hot; post-step clamp
  still cold) and a "Bugs that still require a cold EXE rebuild" list added.
- Note: a transient suite-wide failure occurred once (6 tests) while a
  concurrent agent relinked the EXE/DLL; re-running gave 26/26 — recorded so it
  is not mistaken for a regression.

## Files changed
`src/hot-reload/hot-movement-policy.h` (speed-policy payload + declaration),
`src/hot-reload/modules/movement-speed-policy.cpp` (new),
`src/physics/movement/movement-step.cpp` (server hooks + fallbacks),
`src/hot-reload/modules/movement-system.cpp` (client uses shared derivation),
`src/network/movement-algorithm-selftest.cpp` (speed-policy checks); docs + this
changelog.

## Next (auto-selected)
Jump hot (eligibility, impulse, air-jump state, coyote/buffer) using
`MovementRuntimeStateComponent`, then dash/down-dash, then freeze, then the
post-acceleration clamp, then finish the generic movement integrator; then
reconciliation/interpolation/rewind policy.

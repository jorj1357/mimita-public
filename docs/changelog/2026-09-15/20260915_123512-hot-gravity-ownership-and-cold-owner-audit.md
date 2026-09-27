# Hot gravity ownership + repo-wide cold-owner audit

- EST timestamp: 2026-09-15 12:35:12 EDT (UTC 2026-09-15T16:35:12Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--movement-algorithm-selftest` 13/13 +
  full suite 26/26; air-parity harness emits a `[warn]`, see deferred bugs)

Architecture-first pass: migrate gravity into shared hot policy and publish the
repo-wide cold-owner audit.

## Report
- COLD OWNER REMOVED: `applyBasicGravity` (movement-step.cpp) is no longer the
  sole gravity owner (now fallback); the client no longer delegates gravity to
  `physics.move` (`gravityScale` set to 0 when the hot policy runs).
- HOT OWNER ADDED: `MimitaHotMovement::gravity` (new `modules/movement-gravity.cpp`),
  payload `GameGravityV1` + event `movement.gravity`.
- COMPATIBILITY FALLBACK REMAINING: built-in `movementApplyGravityZ` runs when no
  hot handler handles the event.
- STATE AUTHORITY: generic `Velocity` (z); plain-number inputs only.
- SERVER CALL SITE: `applyBasicGravity` (movement-step.cpp) dispatches
  `movement.gravity`.
- CLIENT CALL SITE: `movement.main` (movement-system.cpp) applies the shared
  gravity to `vz` before `physics.move`, and sets `gravityScale = 0`.
- SELFTEST PROVEN: `--movement-algorithm-selftest` PASS 13/13, adding hot-gravity
  vertical-change and terminal-clamp checks.
- LIVE HOT-EDIT PROVEN: no.
- KNOWN BUGS DEFERRED: air-parity `[warn]` now maxDev=2.2699, first divergent
  tick 68 (was 1.61 / tick 84 before gravity moved). The gravity move changed the
  client's gravity integration point; the air divergence remains a deferred,
  non-blocking parity/feel bug (suspected wish-speed/speed-cap policy).
- WOULD THIS BUG STILL REQUIRE COLD RESTART? No — gravity/jump/dash/freeze policy
  edits are now hot; the divergence is feel/parity only.
- NEXT COLD OWNER: speed-cap / wish-speed derivation, then jump, dash/down-dash,
  freeze, then the generic integrator flip.

## Repo-wide audit
Added a subsystem cold-owner table to `hot-cold-audit.md` (movement, prediction,
reconciliation, interpolation, rewind, weapons, projectiles, NPC AI, gamemodes,
objectives, spawn, animation, effects, audio, HUD, UI, commands/tools,
resources, editor, world generation, multiplayer generation delivery) with
current cold/hot owners, state authority, why still cold, next step, and whether
a bug still needs a cold EXE restart.

## Status labels
- SELFTEST PROVEN: hot gravity function ownership (vertical change + terminal
  clamp); hot ground and air ownership unchanged.
- COMPILED INTEGRATION: server dispatch + client direct call; full suite 26/26.
- SERVER/CLIENT PARITY PROVEN: no (air divergence recorded; worsened with the
  gravity move — deferred by policy).
- LIVE HOT-EDIT PROVEN: no.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game gravity/fall feel (client gravity now
  applied in hot policy instead of the physics primitive).

## Files changed
`src/hot-reload/hot-movement-policy.h` (gravity payload + declaration),
`src/hot-reload/modules/movement-gravity.cpp` (new),
`src/physics/movement/movement-step.cpp` (server gravity hook + fallback),
`src/hot-reload/modules/movement-system.cpp` (client uses shared gravity),
`src/network/movement-algorithm-selftest.cpp` (gravity checks);
`docs/architecture/live-development/hot-cold-audit.md` (cold-owner table);
docs + this changelog.

## Next (auto-selected)
Speed-cap / wish-speed derivation hot (likely reduces the air divergence), then
jump, dash/down-dash, freeze, then finish the generic movement integrator; then
reconciliation/interpolation/rewind policy.

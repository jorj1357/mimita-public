# Generic jump/dash runtime-state authority (player)

- EST timestamp: 2026-09-15 14:40:40 EDT (UTC 2026-09-15T18:40:40Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + full suite 27/27)

Architecture-first pass: move the server player's jump/dash ability runtime state
onto the generic `MovementRuntimeStateComponent`.

## Report
- SUBSYSTEM: movement ability runtime state (jump/dash).
- OLD AUTHORITY: typed `ServerPlayer.movement` (MovementJumpState / dash /
  downDash) was the persistent authority, copied into `MovementState`.
- NEW AUTHORITY: generic `MovementRuntimeStateComponent` (existing fields:
  grounded, airJumpsLeft, jumpIntentSeconds, jumpHeldPreviously, airJumpArmed,
  dashAvailable, downDashAvailable, dashGraceSeconds) is read at the top of the
  tick and written back after the step.
- GENERIC COMPONENTS USED: `TransformComponent`, `VelocityComponent`,
  `MovementRuntimeStateComponent`.
- PERSISTENT STATE MOVED: grounded, airJumpsLeft, jump buffer intent, jump
  held-previous, air-jump armed, dash availability, down-dash availability, dash
  grace seconds.
- EPHEMERAL STATE REMAINING: the per-tick `MovementState` working copy (stack
  local) is unchanged and acceptable.
- COMPATIBILITY PROJECTIONS: `ServerPlayer.movement` and `ServerPlayer.pos/vel/
  onGround` remain mirrors written by `applyMovementStateToServerPlayer`.
- PLAYER PROOF: generic-direct ability read/write (compiler + suite).
- NPC PROOF: not done.
- RUNTIME-GENERIC ACTOR PROOF: valid (typeless actor pipeline unchanged).
- SELFTEST PROVEN: full suite 27/27 (direct headless "typed mutation cannot
  override" test not runnable; proven structurally by the read-from-component at
  the top of `simulatePlayer`).
- LIVE HOT-EDIT PROVEN: no.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Ordinary jump/dash runtime-state
  bugs: no (generic/hot). Collision mechanism: yes (cold by design).
- MOVEMENT CATEGORY COMPLETE? **No** — remaining: freeze runtime state (no generic
  fields yet), coyoteTimerSeconds/airJumpLocked (no generic fields yet), and one
  real NPC path.
- NEXT COLD OWNER: freeze/coyote generic fields → NPC path → reconciliation.

## Notes
- No component ABI change: this pass uses only existing
  `MovementRuntimeStateComponent` fields. Extending it with freeze/coyote fields
  and migrating the NPC physics are intentionally separate slices so each
  authority move stays verifiable.
- Dash cooldown is client-only (the server has no cooldown field); the client
  already stores it in the same component (`dashCooldownSeconds`).

## Files changed
`src/network/server-players.cpp` (generic jump/dash runtime-state read/write);
docs + this changelog.

## Next (auto-selected)
Freeze + coyote/airJumpLocked generic fields; migrate one real NPC path onto the
same substrate; declare movement complete; then reconciliation policy →
interpolation → rewind policy → distributed generation delivery → READY/SWITCH.

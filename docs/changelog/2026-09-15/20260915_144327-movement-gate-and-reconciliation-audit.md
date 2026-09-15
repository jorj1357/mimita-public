# Movement completion gate + reconciliation audit (blocked at concurrency boundary)

- EST timestamp: 2026-09-15 14:43:27 EDT (UTC 2026-09-15T18:43:27Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: no source change this pass (blocked); docs/audit + changelog only

## Why no source change
The remaining movement items require editing files the concurrent movement-state
agent is actively modifying right now:
- `src/hot-reload/game-api.h` (2:26 PM) — `GameMovementRuntimeStateComponentV1`
- `src/live-code/live-behavior.cpp` (2:31 PM) — the component read/write mapping
- `src/hot-reload/modules/movement-system.cpp` (2:10 PM)
- plus effects/render/presentation files.

Per the standing concurrency rule ("if blocked by overlapping edits, document the
exact missing boundary and stop rather than creating duplicate architecture"), I
did **not** extend the shared component/mapping or rewrite the NPC physics in
parallel. No duplicate owner or second state store was created.

## Report
- SUBSYSTEM: movement runtime state (freeze/coyote/air-jump lock) + NPC path.
- OLD AUTHORITY: typed `ServerPlayer.movement` for freeze (`active`/`available`/
  `timerSeconds`), `coyoteTimerSeconds`, `airJumpLocked`.
- NEW AUTHORITY: intended generic `MovementRuntimeStateComponent` — **not done**
  (fields missing; component/mapping owned by the concurrent agent).
- GENERIC FIELDS ADDED: none this pass (deliberately).
- SCHEMA/MIGRATION HANDLING: required when the component is extended (version +
  deterministic defaults for existing entities); documented as a requirement, not
  performed.
- PERSISTENT STATE MOVED: none this pass.
- COMPATIBILITY PROJECTIONS: unchanged.
- PLAYER PROOF: Transform/Velocity/grounded/jump/dash generic (prior rounds).
- NPC PROOF: **not done.**
- RUNTIME-GENERIC ACTOR PROOF: valid.
- REWIND SOURCE: generic Transform. SNAPSHOT SOURCE: typed projection.
- SELFTEST PROVEN: unchanged (full suite 27/27 as of the previous round).
- LIVE HOT-EDIT PROVEN: no.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Freeze/coyote/lock policy edits: no
  (hot). Generic-field migration + NPC: pending.
- MOVEMENT CATEGORY COMPLETE? **No** (gate 6/7/11 unmet; see
  `hot-cold-audit.md`).
- RECONCILIATION AUDIT STARTED? **Yes** — table added to `hot-cold-audit.md`
  (`mpReconcileLocalPlayer`, `classifyMovementCorrection`, snap/smooth/lifecycle
  decisions; interpolation sample/lerp; rewind sample selection).
- NEXT COLD OWNER: finish the component extension + NPC adapter when the
  concurrent files settle, then reconciliation hot policy.

## Next (auto-selected)
1. Extend `MovementRuntimeStateComponent` + the bridge with freeze
   active/available/timer, coyoteTimerSeconds, airJumpLocked (schema-versioned,
   deterministic defaults) — coordinate with the movement-state agent.
2. Migrate one real NPC path onto the same substrate (AI intent → generic
   integrator).
3. Declare movement complete; begin reconciliation hot-policy migration using the
   audit table.

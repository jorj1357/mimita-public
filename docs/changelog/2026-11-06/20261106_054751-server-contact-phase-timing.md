# Server contact phase timing diagnostics

Time created: 2026-11-06T05:47:51Z  
Status: PHASE-LEVEL SERVER CAUSE CONFIRMED; gradual slowdown and reconnect mechanism remain unproven

Branch: `20261011zt`  
HEAD at start: `e3cae756828e55225f85c401a39b9b739995b021` (no commit created)

## Purpose

Split the server's existing over-100-ms tick stall timing across the direct
contact-phase functions, so a fresh runtime journal can identify which phase
dominates.

## Source change

In `src/network/server.cpp`, `runServer()` now times these calls separately:

- `tickServerMeleeActions()` → `melee_actions_ms`
- `tickServerPhysicalContactWeapons()` → `physical_contact_ms`
- `flushServerWorldImpacts()` → `world_impact_flush_ms`

The existing `performance.server-tick-stall` event includes these values only
when the total tick exceeds 100 ms. The prior aggregate `contact_weapon_ms`
remains for comparison. No other source file was edited for this diagnostic.

The unrelated pre-existing changes in the worktree were preserved, including
changes to `src/network/server-physical-contact.cpp`, `src/network/server.h`,
`src/network/server-npcs.cpp`, and other files.

## Reasoning and review

The current aggregate `contact_weapon_ms` combined melee actions, physical
contact weapons (including animated NPC contact handling), and world-impact
flushing. Timing these direct calls independently avoids changing their order
or behavior and emits no additional per-tick events.

Documents read:

- `docs/ROUTER.md`
- `docs/specs/performance/performance.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/specs/dev-loop/multi-instance.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/operations/task-completion/task-completion.md`

Focused skills:

- `docs/skills/logging-checker-v1.md`: PASS by inspection; the existing
  bounded stall event owns these measurements.
- `docs/skills/efficiency-checker-v1.md`: PASS by inspection; three monotonic
  timer reads and JSON fields occur only in the already-measured server tick;
  the new fields are emitted only on the existing >100 ms stall path.

## Validation and runtime evidence

- `git diff --check -- src/network/server.cpp`: PASS (Git reports its existing
  LF-to-CRLF working-copy warning).
- Build: dev-loop published and launched artifact
  `2026-11-06T05:52:47.141763Z`, SHA-256
  `22675766ec9c898b0e734bcf112e2e83c3115533fb24c39a90e248d9f3dcb652`.
  Confirmed the executable contains all three new field names. Dev-loop
  reported `published_current`, source generation 0, and `stale=False`.
- Runtime: dev-loop run `20261106_055615`, mode 45 (Fun Counter Strike), map
  `dust2cyberiav4`; server PID 36156 and client PID 18192. The client journal
  reached `Connected`, but the dev-loop's required `network.mode.start` ACTIVE
  event did not appear within 45 seconds. This is a startup/mode readiness
  failure and limits gameplay conclusions; the server remained running and
  produced its own performance journal.
- Server evidence: `logs/2026-11-06/20261106_055615/server-01/events.jsonl`
  recorded five `performance.server-tick-stall` events from 05:56:25.779Z to
  05:58:09.152Z. Total tick time was 100.704–608.189 ms. The split timings
  show `physical_contact_ms` at 41.355–415.085 ms (median 280.381 ms), while
  `melee_actions_ms` was 0.115–0.196 ms and
  `world_impact_flush_ms` was 0.110–0.430 ms. At the worst 608.189 ms tick,
  physical contact took 415.085 ms. This directly identifies
  `tickServerPhysicalContactWeapons()` as the dominant measured phase in these
  severe stalls. `npc_ms` was also material at 9.904–212.959 ms, so it may
  contribute to some events. The events had 19 NPCs; player count ranged from
  zero to one.
- Interpretation: this proves severe server fixed-tick stalls in the FCS
  runtime and identifies physical-contact processing as the dominant measured
  phase in this sample. It does not prove a steadily increasing slowdown over
  time, that the client reconnect symptom is caused by these stalls, or that
  this mode reached active gameplay. The dev-loop startup check failure is
  separately recorded and should not be treated as a server-tick measurement.

The user explicitly resumed dev-loop work. The initial Bloodtest launch was
not used for FCS conclusions. The subsequent mode-45 run above supplies the
phase-level evidence. `git diff --check -- src/network/server.cpp` passed
before the build; no source changes were made after that build.

Human gameplay review and exact reconnect-path proof: pending.

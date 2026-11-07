# NPC behavior module first proof specification

- Recorded: 2026-11-07 13:58:02 EST (2026-11-07T18:58:02Z)
- Branch: `20261011zt`
- HEAD at start: `64d28fe1`
- Status: PASS_WITH_HUMAN_REVIEW
- Scope: source-checked specification only; no runtime or gameplay code changed

## Pre-existing work

The worktree was already broadly modified across assets, configuration,
devscripts, documentation, shaders, and source files. The existing
`docs/specs/hotreload/hotreload.md` and
`docs/specs/npc-behavior/npc-behavior.md` were also already modified before
this task. Those edits were preserved; only the hot-reload specification was
further edited here. The prior live-modules changelog was present and was not
rewritten. No branch, commit, or unrelated file was changed.

## Specification change

Updated `docs/specs/hotreload/hotreload.md`, section 5 (lines 188–312), which
previously described the first proof generically as utility goal/action
selection. It now names the current source path from
`simulateSharedNpcs` → `NpcSystem::updateOneNpc` → `senseWorld` →
`selectUtilityGoal`/`actionForGoal` → `makeNavGoal` → navigation and the
post-movement diagnostic.

The first live scenario uses a fresh NPC with a visible acquired target beyond
12 m, ready weapon, open line of sight, no objective/hazard/focus/squad
approach override, and no prior utility goal. Expected outputs are
`KillTarget` → `Approach` → host `FollowActor` navigation, followed by measured
world-space progress. The module extraction first ports both utility decision
functions without behavior changes; later tuning must name one decision rule
and keep independent target/team/combat/navigation/collision/recovery owners
fixed. Parity may compare legacy and candidate outputs using copies of the
same snapshot/state, but only one result is applied to gameplay per tick.

The section also specifies fixed-tick activation, host-owned actor state,
last-known-good failure handling, generation-correlated owner journal events,
`NOT_COVERED` reporting, and the contributor scope checklist. The source-backed
inventory row and acceptance gates were updated to use this exact proof.

## Source and review evidence

- Read `docs/ROUTER.md` and the routed live-module, NPC behavior, NPC movement,
  logging, runtime scenario, date/filename, task-completion, and documentation
  review guidance.
- Inspected `src/npc/npc-utility.cpp::selectUtilityGoal` and
  `actionForGoal`, `src/npc/npc.cpp::senseWorld`, `makeNavGoal`,
  `NpcSystem::updateOneNpc`, the `npc.movement-decision` payload, and
  `src/network/server-npcs.cpp::simulateSharedNpcs`.
- Read `docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md`.
  It documents the mixed target-policy/team-lookup/stuck-recovery correction;
  its later wall check also missed the intended geometry because its fixture
  used a stale absolute position.
- `docs/skills/spec-behavior-review-v1.md`: PASS for source-path alignment and
  explicit acceptance boundary. Runtime proof remains future work.
- `docs/skills/documentation-checker-v1.md`: PASS for changed spec purpose,
  audience, scope, and relative link. Existing relevant TODO left untouched:
  `docs/architecture/player-npc-systems/npc-movement.md:22` says
  `9 8 2026 1019 est jorj todo - explain`; suggested expansion is to document
  current NPC goal → navigation → movement-intent → shared-physics ownership
  and distinguish existing paths from unaccepted movement goals.
- `git diff --check -- docs/specs/hotreload/hotreload.md`: PASS.
- No build or runtime scenario was run because this task changes only the
  specification. NPC module implementation, build/reload proof, server
  `events.jsonl`, and human gameplay review remain NOT_APPLICABLE to this
  documentation change and required for the future implementation.

# NPC rolling forward patrol horizon

Date: 2026-10-04
EST timestamp: 2026-10-04 19:58:02 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

The no-target NPC patrol goal now uses a 999-unit rolling forward horizon.
Focused source/build/self-tests passed. A live Counter-Strike match was not
played during this session, so visual movement acceptance remains required.

## Scope

When an NPC has no visible target, no remembered target, and no objective goal,
keep it advancing forward instead of repeatedly treating a short 12-unit point
as the final patrol destination. The goal is recalculated from the NPC's current
position, so it acts as an endless route rather than a fake enemy target.

## Files changed

### `src/npc/npc.cpp`

Owner: `makeNavGoal(const Npc&)`, lines 421-483.

Old behavior:

```cpp
goal.targetPos = npc.body.pos + dir * 12.0f;
```

New behavior:

```cpp
constexpr float kForwardPatrolGoalDistance = 999.0f;
goal.targetPos = npc.body.pos + dir * kForwardPatrolGoalDistance;
```

The change applies only to the no-target `NpcState::Patrol` branch. Visible
targets, last-known pursuit, objective goals, and random-walk goals are
unchanged. The bounded navigator still handles local collision geometry,
turning, repathing, ramps, and stuck recovery.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/player-npc-systems/npc-movement.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

## Validation

### Build

```text
python build_agent.py
BUILD SUCCESS
Compiled: 1
Skipped: 522
```

### Focused runtime self-tests

```text
--npc-search-behavior-selftest       PASS
--npc-navigation-selftest            PASS
--npc-movement-executor-selftest     PASS
--counterstrike-acceptance-selftest  PASS
```

### Human review still needed

Run a live Sandbox and Counter-Strike match and confirm that a no-target NPC:

1. keeps advancing through successive areas;
2. does not stop because it reached a short patrol point;
3. still turns/repaths around walls and follows ramps;
4. switches immediately to a visible or remembered enemy; and
5. does not treat the 999-unit movement goal as a shootable target.

## Pre-existing working-tree changes

Other modified and untracked files were present before this change and were
preserved. This session only added the forward-horizon change in
`src/npc/npc.cpp` and this changelog.

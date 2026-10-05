# JSON-controlled forward patrol distance

Date: 2026-10-04
EST timestamp: 2026-10-04 20:06:08 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

The Counter-Strike no-target forward waypoint is JSON-controlled and defaults
to 12 units. Reaching the waypoint naturally causes the shared goal mapper to
create the next 12-unit waypoint. Focused tests and the build passed. The live
Dust2 CT-spawn wall behavior remains a separate human/runtime investigation.

## Files changed

- `src/npc/npc-movement-policy.h:65` adds `forwardPatrolDistance`, default 12.
- `src/npc/npc-movement-policy.cpp:103-104` parses and clamps
  `forward_patrol_distance` to 1-999.
- `src/npc/npc.cpp:469-483` uses the active actor preset's value for the
  rolling patrol goal, with 12 as the legacy fallback.
- `config/actor-presets/counter_strike.json:71` sets
  `"forward_patrol_distance": 12.0` and documents its meaning.

## Behavior contract

```cpp
if (visibleTarget || rememberedTarget || objective) {
    useHigherPriorityGoal();
} else {
    goal = npc.position + patrolDirection * forwardPatrolDistance;
}
```

The forward goal is movement-only and is not a fake enemy target. The existing
shared navigator still owns wall checks, route planning, ramp traversal, and
stuck recovery.

## Validation

```text
python build_agent.py                         BUILD SUCCESS
--npc-movement-policy-selftest                PASS
--npc-search-behavior-selftest                PASS
--npc-navigation-selftest                     PASS
--counterstrike-acceptance-selftest           PASS
```

## Wall behavior finding

The source contains wall detection and open-direction recovery. The focused
navigation/search tests prove that synthetic walls cause a lateral route, but
they do not prove the Dust2 CT spawn's loaded collision geometry or the live
server target state. If CT NPCs still push the spawn wall, the next diagnostic
must identify whether the actor is in Patrol or Chase, whether
`obstacleInDirection` sees the loaded wall triangle, and whether the navigator
returns `blocked`/a turn before the final movement input is applied.

## Human review still needed

Run Counter-Strike on `dust2cyberiav4` and verify that CT NPCs use successive
12-unit forward goals, turn around the spawn wall, and do not remain pressed
against it. Capture the NPC movement diagnostic for one stuck CT if the wall
remains; no additional wall code was changed in this session because the
map-specific failure point has not yet been proven.

## Pre-existing working-tree changes

Other modified and untracked files were present before this session and were
preserved. This session changed only the four files listed above and added this
changelog.

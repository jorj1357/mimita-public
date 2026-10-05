# NPC shared navigation honors actor preset radius

Date: 2026-10-04
UTC timestamp: 2026-10-05T00:26:02.102Z
Display timezone: America/New_York
Display time: 2026-10-04 20:26:02 EDT
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

The shared NPC movement path now honors an actor preset's optional navigation
settings, including Counter-Strike's existing `search_radius: 20.0`. The code
build and focused navigation/executor tests pass. A live Counter-Strike match
has not been replayed after this change.

## Finding and fix

Counter-Strike selected `movement_executor: "sandbox_shared"`, but
`NpcSystem::updateOneNpc` only supplied `activeNavigationSettings(npc)` to the
`surface_navigation` executor. Counter-Strike therefore silently fell back to
the navigator's hard-coded 12.5-meter local planning window even though its
JSON requested 20 meters. This could make a valid ramp/door detour fall outside
the searched area and leave the NPC repeatedly steering into the direct target
direction.

The shared executor now supplies the actor preset navigation block for both
`sandbox_shared` and `surface_navigation`. The movement implementation remains
one shared path; the preset only changes route-planning data. Actors without a
navigation block retain the existing 12.5-meter shared fallback, and `direct`
still disables surface planning.

## Files changed

- `src/npc/npc.cpp`, `NpcSystem::updateOneNpc`: pass optional preset navigation
  settings to the shared executor.
- `src/npc/npc-movement-context.h`: clarify that the shared executor honors an
  optional preset navigation block.
- `src/npc/npc-movement-policy.h`: document the executor contract.
- `src/npc/npc-navigator.cpp`: clarify that all executors use the same
  navigator and only the supplied settings differ.

These files already contained unrelated/pre-existing NPC and Counter-Strike
work from earlier agents; this session only added the shared-executor settings
boundary described above.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/player-npc-systems/npc-movement.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/regressions/2026-10-04/counterstrike-npc-targeting-movement-REG.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

## Validation

- `python build_agent.py`: `BUILD SUCCESS`, return code 0; build timestamp
  `2026-10-04 20:24:05`.
- `build/npc-movement-executor-test.exe`: `PASS (15 checks)`.
- `build/npc-navigation-test.exe`: `PASS (34 checks)`.
- `git diff --check`: pass after removing one trailing-space warning in the
  touched header.

## Human review still needed

Run Counter-Strike on `dust2cyberiav4` and verify that CT NPCs can leave their
spawn, route around the front wall/ramp, and continue pursuing enemies without
repeating the same wall push. If they still fail, the next evidence needed is
an `[NPC NAV]` log showing whether the 20-meter plan found a route or whether
the map collision geometry has no connected walkable surface at that location.

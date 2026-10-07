# NPC behavior and navigation investigation

- Status: `INVESTIGATION_COMPLETE_IMPLEMENTATION_PENDING`
- Timestamp: `2026-10-07 13:16:56 -04:00` (America/New_York)
- Branch: `afad20a-rebuild`
- Commit at inspection: `53ff267036cb217a8d1139652e99fa5c8b3b5cc4`
- Scope: read-only investigation of NPC wall contact, direct objective travel,
  Counter-Strike plant/defuse flow, aim, binds, empty-hand equip, chat hiding,
  logging volume, and the external navigation-library boundary.

## Pre-existing work preserved

The worktree already contained unrelated modifications in:

`config/accounts/default.json`, `config/analytics.json`,
`config/weaponsets.json`, `docs/jorj-docs/20261007plan.md`,
`docs/regressions/cold-build-required-REG.md`, `src/combat/weapon-system.cpp`,
`src/engine/engine-tick-combat.cpp`, `src/engine/engine-tick-ui-game-hud.cpp`,
and the existing `docs/changelog/2026-10-07/20261007_130200-nothing-inventory-item.md`.
No source or configuration behavior was changed by this investigation.

## Confirmed findings

1. The active NPC execution chain is
   `serverTeamBrainTick -> UtilityContext -> makeNavGoal ->
   NpcNavigator::update -> wall avoidance/stuck recovery -> shared physics`.
   The server remains the correct owner for NPC navigation and objectives;
   shared movement/physics remains the final position owner.

2. Recast/Detour is present and currently compare-only. `src/npc/recast-navigation.cpp`
   converts MiMITA Z-up geometry to Recast Y-up, builds a navmesh, resolves
   nearest polygons, and reports explicit path failures. Existing architecture
   documents correctly prohibit promoting it before real-map route proof.

3. The active `config/npc-difficulty.json` contains contradictory local-search
   tuning: `wallSearchDistance` and `searchLookahead` are `0.01`, while
   `wallAvoidMinProbe` is `10.0`. The wall chooser in
   `src/npc/npc-navigation.cpp`, `NpcNavigation::wallAvoidDirection`, samples
   only left/right, four 45-degree diagonals, and reverse. It does not test
   fine-grained tangent angles or a route corridor, so it can alternate around
   corners instead of preserving forward progress.

4. `src/npc/npc.cpp`, `NpcSystem::updateOneNpc`, applies wall avoidance after
   tactical/navigation decisions. The journal contains a remembered-target
   chase with `net_progress_toward_goal: 0`, repeated direction changes, and
   `npc.nav-plan-failed` with `path_nodes: 0`. This is runtime evidence of a
   steering/route failure, not merely an aim or animation issue.

5. The current journal is not acceptably bounded for this scenario. In
   `logs/10-07-2026/20261007_124602/events.jsonl`, event counts include about
   89,724 `npc.stuck`, 32,854 `npc.wall-avoid`, 9,239 `npc.jump`, and 2,745
   `npc.nav-plan-failed` records. The producer comments intend rising-edge or
   episode logging, but repeated short stuck episodes still generate a large
   stream. The next diagnostic should aggregate per actor/episode and emit a
   bounded summary with first/last tick, count, direction flips, distance
   progress, and the first divergence.

6. Counter-Strike objective state already exists. `serverObjectiveTick` in
   `src/network/server-gamemode.cpp` automatically begins and advances plant
   progress when the carried bomb carrier is inside a configured site, and
   automatically advances defuse progress when a CT is within interaction
   range of a planted bomb. `serverTeamBrainTick` assigns T site travel and CT
   retake/defuse context. The missing live proof is whether the carrier gets a
   valid route and reaches the site; no new bomb system should be added before
   tracing that chain.

7. `rage2` already has `aim_error_deg: 1.0` in
   `config/behavior-profiles.json`. This is source/config evidence only; live
   aim quality still requires a real run with aim decision, target point,
   applied error, shot direction, and hit/miss records.

8. A partial bind system already exists in `src/input/input-commands.*`: JSON
   binds load/save and runtime action-to-key mapping are present, but the
   `InputAction` enum has no canonical fire/aim actions. Mouse fire/aim is still
   read directly in `src/input/input-poll.cpp`. `chatwindow 0|1` already exists
   in `src/terminal/player-commands.cpp`; `chat_show 0|1` would be an alias or
   naming decision, not a new chat renderer.

9. Empty-hand support is already implemented by the existing `nothing` item and
   `WeaponSystem::unequip`; the prior same-number toggle/hidden-hotbar work is
   recorded in `20261007_130200-nothing-inventory-item.md`. Live connected
   acceptance remains separate.

## Required next implementation slices

1. Add bounded owner-level StructuredLogger decision records, rebuild a newly
   named executable, run `--versioninfo`, and reproduce a real Dust2 scenario.
2. Fix the first navigation divergence: prove whether the custom local planner
   returns an empty route, whether Recast nearest-poly/path connectivity fails,
   or whether a valid route is overwritten by wall avoidance.
3. Make local correction forward-preserving: fine angular candidates, collision
   clearance, route/corridor progress, hysteresis, and reverse only for a
   genuinely trapped actor. Keep Recast compare-only until route quality passes.
4. Trace T `carrier -> objectiveTargetPosition -> ReachPosition -> route -> site`
   and CT `planted -> retake/defuse goal -> interaction range -> defuse` in the
   same journal before changing objective logic.
5. Consolidate bind actions for fire, aim, grenade slots, and empty hands behind
   one action surface; preserve the existing shared weapon/equip authority.

## Documents and focused reviews read

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/networking/networking.md`
- `docs/specs/gamemodes/counterstrike.md`
- `docs/specs/ingame-chat/ingame-chat.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/architecture/player-npc-systems/npc-navigation-implementation.md`
- `docs/architecture/player-npc-systems/npc-nav-external-lib-20261006.md`
- `docs/architecture/player-npc-systems/npc-navigation-handoff-prompt-20261006.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/chat-checker-v1.md`
- `docs/regressions/regressions-v1.md`

## Validation and human review

- `git status --short`: inspected; pre-existing edits preserved.
- Repository search and source inspection: completed.
- Existing runtime journal inspection: completed.
- Build: not run; no code or configuration behavior was changed.
- Runtime scenario launched by this session: none.
- Human gameplay/visual/multiplayer acceptance: not performed.


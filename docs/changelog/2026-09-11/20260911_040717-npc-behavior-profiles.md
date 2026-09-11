// 2026-09-11T04:07:17Z
/* purpose
* record reusable NPC behavior profiles resolved from role behavior_profile
* preserve exact source, config, build, and headless runtime evidence
* this file does NOT claim emotion/panic, stealth, or social memory
* this file does NOT replace the append-only regression record
*/

# NPC behavior profiles

## Session

- Branch: `8292026stash`
- HEAD commit: `fdac4e2`
- Timestamp (UTC): `2026-09-11T04:07:17Z`
- Display timezone: America/New_York
- Display time: `2026-09-11 00:07:17 EDT`
- Pre-existing changes: all prior actor/role/health/loadout/movement and
  goal/navigation/traversal slices were preserved. This session only consumed
  the last stored role field.
- Unrelated concurrent edits under `docs/regressions/`, `docs/gold/`, and other
  `docs/changelog/` files were left untouched.

## Files changed

New:
- `src/npc/npc-behavior.h` / `npc-behavior.cpp`: `BehaviorProfileDefinition`,
  `NpcBehaviorTuning`, `BehaviorProfileRegistry`, `resolveNpcBehavior`.
- `config/behavior-profiles.json`: `balanced`, `aggressive`, `nervous`.

Modified:
- `src/npc/npc.h`: `Npc` gains `behaviorProfileId`, `behavior` (resolved
  tuning), and `prevHadTarget`.
- `src/npc/npc-combat.h/.cpp`: `effectiveAimErrorDegrees()`; profile overrides
  aim error, fire cadence, and aggression.
- `src/npc/npc.cpp`: reaction-delay gate on target acquisition; profile
  `preferred_range` feeds `makeNavGoal`.
- `src/npc/npc-state-machine.cpp`: profile `preferred_range` feeds the tactical
  `idealDist` scoring (single range system).
- `src/npc/npc-spawn.cpp`: re-applies profile aggression after difficulty
  refresh.
- `src/network/server-gamemode.h/.cpp`: `ActorSpawnProfile.behaviorProfileId`,
  validation, NPC spawn application, `[NPC BEHAVIOR]` diagnostic.
- `src/network/server-npcs.cpp`: reapplies behavior on respawn.
- `src/main-systems.cpp`, `src/network/server.cpp`, `src/engine/engine-tick-setup.cpp`:
  load + hot-reload `config/behavior-profiles.json`.
- `config/roles.json`: hunter -> balanced, juggernaut -> aggressive, added
  skirmisher -> nervous.

## Behavior-profile schema

`config/behavior-profiles.json`:
```json
{ "id": "aggressive", "aim_error_deg": 3.0, "reaction_delay": 0.08,
  "fire_cadence_multiplier": 1.15, "aggression": 0.9, "preferred_range": 8.0 }
```
Missing numeric fields keep their no-override sentinel (`aim_error_deg`,
`reaction_delay`, `aggression`, `preferred_range` < 0; cadence defaults 1.0), so
callers fall back to existing `NpcDifficultyConfig` behavior.

## role -> profile resolution flow

`MatchRoleDefinition::behaviorProfile` (parsed from `behavior_profile`) ->
`serverResolveActorSpawnProfile` validates it through
`BehaviorProfileRegistry::get` -> `ActorSpawnProfile.behaviorProfileId` ->
NPC spawn (`resetGamemodeActorsAtMapSpawn`) and respawn (`respawnServerNpc`)
call `resolveNpcBehavior(id)` into `npc.behavior`, preserving it through
respawn. JSON is parsed once per file change, never per tick.

## Exact combat fields affected

- **Aim error**: `NpcCombat::effectiveAimErrorDegrees` returns
  `behavior.aimErrorDeg` when active, else the difficulty-derived value; used by
  `applyAimError` and the shot diagnostics.
- **Reaction delay**: on a `hasTarget` rising edge, `npc.reactionTimer` is armed
  to `behavior.reactionDelay`; firing requires `reactionTimer <= 0`. Reset on
  spawn/respawn.
- **Fire cadence**: `min/maxDelay` are divided by
  `behavior.fireCadenceMultiplier` (higher = faster) before the existing
  aggression blend; weapon fire rules/cooldowns are untouched.
- **Aggression**: `computeFireAggression` base uses `behavior.aggression` when
  active (also mirrored into `npc.tuning.aggression` for state scoring).
- **Preferred range**: `behavior.preferredRange` replaces the weapon-derived
  stand-off distance in `makeNavGoal` (MaintainDistance) and `scoreState`'s
  `idealDist` (existing tactical range logic, not a second range system).

## Fallback behavior

- No role / no `behavior_profile` / unknown profile: `behavior.active == false`,
  and every combat field uses the current `NpcDifficultyConfig` path exactly.
  Roleless FFA/TDM NPCs are unchanged.
- Unknown `behavior_profile` id: warned once at role resolution with role
  context; the role keeps NPC defaults.
- Unknown/invalid profile JSON: registry keeps the last valid data.

## Hot-reload behavior

`BehaviorProfileRegistry::pollReload()` detects file changes and reloads with
keep-last-valid semantics; wired next to the role registry polls in the client
hot-reload list and both server loops. Live NPCs pick up new values on their next
spawn/respawn.

## Measurable profile differences

Controlled run: three roles on opposing teams, identical movement
(`retrograd_fast`) and loadout (`standard`), differing only by behavior profile
(TDM, funworld3, 12 NPCs).
- Load: `[BEHAVIOR] Loaded 3 profile(s)`.
- `[NPC BEHAVIOR] hunter profile=balanced aim=5.0 react=0.15 cadence=1.00
  aggr=0.60 range=12.0`; `juggernaut profile=aggressive aim=3.0 react=0.08
  cadence=1.15 aggr=0.90 range=8.0`; `skirmisher profile=nervous aim=8.0
  react=0.25 cadence=0.80 aggr=0.35 range=18.0`.
- Shot means (from the always-on NPC combat log):
  `aggressive maxErr=3.00 dist=6.44 shots=568`,
  `balanced maxErr=5.00 dist=10.68 shots=269`,
  `nervous maxErr=8.00 dist=9.76 shots=377`,
  `default (pre-role) maxErr=0.00 dist=41.81 shots=281`.
- Reaction arming: `aggressive delay=0.08 x39`, `balanced delay=0.15 x26`,
  `nervous delay=0.25 x29`.

Aim error matches the configured values exactly; distances and shot counts
differ by profile, all caused by data only.

## Regressions

- FFA (10 NPCs): 100 kills; runs and scores.
- Elimination (10 NPCs): `[PERSISTENCE] Match result emitted: mode=elimination
  winner=blue` (still ends); role health/loadout/movement logs present
  (`ROLE SPAWN=22`, `ROLE MOVEMENT=24`).
- TDM (30 NPCs): 312 kills; completed 40 s in 42.5 s wall time (no stall).
- Navigation/traversal, role health/loadout/movement, and role/team assignment
  are untouched by this slice.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md`, `docs/specs/weapons/weapons.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS; behavior is data-driven with
  no profile-name branches and reuses existing `NpcDifficultyConfig` concepts.
- `docs/skills/efficiency-checker-v1.md`: PASS; JSON parsed once, profile
  resolved once per life, per-tick work is float comparisons.
- `docs/skills/logging-checker-v1.md`: PASS; `[BEHAVIOR]` load/reload and
  `[NPC BEHAVIOR]` spawn lines, plus reaction lines in the always-on NPC log.

## Smallest logical next phase

Expose per-profile weapon preference and target-switch tendency through the same
registry and feed them into the existing weapon-switch and target-selection
owners, so behavior profiles can shape loadout choice and engagement focus
without new systems.

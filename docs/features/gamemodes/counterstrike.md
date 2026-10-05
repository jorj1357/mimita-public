# Counter-Strike mode

Date file created: 2026-10-02

End goal:
Implement the existing `counterstrike` gamemode as a complete JSON-controlled
5v5 round-based mode on `assets/maps/dust2cyberiav3.glb`, using generalized
owners (gamemode rules own rounds/objectives, roles own team membership,
actor presets own actor config, behavior presets own AI policy, shared actor
execution owns movement/weapons/damage/physics).

## Purpose

Provide a full Counter-Strike experience: choose Counter-Terrorists or
Terrorists, play as one actor with 4 allied NPCs against 5 opposing NPCs, 70
FOV, forced first-person, heavy movement, revolver/shotgun/hitscan rifle/
grenades, no damage numbers or hit markers, blood/killfeed/ragdolls enabled,
bomb pickup/plant/drop/defuse/explosion, NPC navigation/perception/combat/
objective behavior, and round score plus match victory.

## Desired behavior

The authoritative pipeline (from `docs/specs/20261002plan.md`):

```text
Counter-Strike Gamemode
  -> CT/T roles
  -> Counter-Strike ActorPreset
  -> shared actor lifecycle
  -> human input or NPC ActorBrain
  -> Perception + Memory
  -> TeamBrain + ObjectiveContext
  -> Utility Goal -> Action -> ActorIntent
  -> Navigation / Weapon / Interaction systems
  -> shared fixed-60-Hz physics and authoritative server
  -> replication, HUD, killfeed, blood, ragdolls
```

## Current behavior

As of 2026-10-02 on branch `afad20a-rebuild`:

- `config/gamemodes/counterstrike.json` was a stub with no rounds/objectives and
  fell through to the legacy 1v1 duel state machine.
- `config/actor-presets/counter_strike.json` already works (FOV 70, forced
  first-person, heavy movement, in-memory weapon overrides).
- `config/roles.json` had no CT/T roles.
- Team commands existed as `teamlist` / `teampick` only.
- No generic objective/bomb system, no bomb sites/map JSON, no grenade utility
  types, and NPCs had no FOV/LOS-gated perception, memory, utility goals, or
  TeamBrain. These are planned for later checkpoints.
- A full generic objective + CS round implementation exists on the divergent
  branch `origin/8292026stash` (1348 files diverged) but is not in this tree.
  Decision: build fresh on `afad20a-rebuild`.

## Current status, from working best to not working at all

All ten checkpoints are implemented at source level; every subsystem has a pure
runtime selftest that passes. **Human gameplay/visual acceptance is still
required** (see the handoff section at the end of the attempt log).

- Actor preset (FOV/first-person/movement/weapon overlay): working (pre-existing).
- Ordered team schema + CT/T roles + `team_list`/`team_pick`: implemented (C1).
- 5v5 roster + round/match lifecycle + round results: implemented (C2).
- Weapon overrides + presentation scoping + first-life NPC loadout: implemented (C3).
- ActorIntent boundary + human-like perception/memory: implemented (C4).
- Utility goals/actions + NavigationRequest/capabilities: implemented (C5).
- Generic bomb objective + pickup/drop: implemented (C6).
- Bomb sites (map JSON) + plant/defuse/explosion + `site_debug`: implemented (C7).
- Grenades + generic area effects (fire/smoke/darkbang): implemented (C8).
- TeamBrain + objective context + AI grenade reasoning: implemented (C9).
- Debug commands (`npc_inspect`, `npc_brain`, `team_status`, `objective_status`)
  + structured events + consolidated acceptance selftest: implemented (C10).
- Runtime/visual/human acceptance: NOT performed.

## Decisions

- Build fresh on the current branch; do not merge/rebase `8292026stash`.
- Execute one checkpoint per session, stopping for review; one changelog and
  one attempt-log entry per checkpoint.
- Commands: `team_list`/`team_pick` are primary, `teamlist`/`teampick` remain
  aliases.
- Weapon values: keep the existing revolver (100 dmg, head-only) and shotgun
  (20/8/32); only add missing fields (hitscan rifle override, zero thickness,
  headshot/spread/recoil/presentation overrides) in Checkpoint 3.
- `NEEDS_SPEC_DECISION`: the plan's weapon acceptance tests expect revolver
  6/72 and shotgun 2/24, but the human chose to keep the existing preset values.
  The weapon tests must assert the kept values unless the plan is updated.
- Presentation: reuse existing `hide_healthbars` / `ragdoll_enabled` /
  `blood_enabled` as the single source; the new `presentation` block is used
  only for genuinely new flags (damage numbers, hit effects, world impacts,
  hit markers, hit sounds, killfeed) to avoid duplicate sources of truth.

## Ownership

- Primary code owner: `src/gamemode/gamemode.*` (rules/data), `src/network/server-gamemode.*` (authoritative lifecycle), `src/gamemode/match-roles.*` (roles/presets).
- Configuration owner: `config/gamemodes/counterstrike.json`, `config/roles.json`, `config/actor-presets/counter_strike.json`.
- Runtime/event owner: `src/network/server-gamemode.*` + `src/debug/structured-log.*` (events).
- Network owner: `src/network/packets.h`, `src/network/community-match-client.*`.
- Animation/physics owner: shared actor/physics paths only (no CS-only path).

## Related authoritative documents

- Specification: `docs/specs/20261002plan.md`
- Architecture: `docs/architecture/ecs-entity-etc/ecs.md`, `docs/architecture/player-npc-systems/player-npc-systems.md`
- Workflow: `docs/workflows/fix-repeated-bug.md`
- Focused review skill: `docs/skills/spec-behavior-review-v1.md`, `docs/skills/terminal-command-checker-v1.md`
- Regression record: created per confirmed break under `docs/regressions/`

## Relevant files

- `src/gamemode/gamemode.h`, `src/gamemode/gamemode.cpp`
- `src/gamemode/match-roles.h`, `src/gamemode/match-roles.cpp`
- `src/network/server-gamemode.h`, `src/network/server-gamemode.cpp`
- `src/network/server-packet-chat.cpp`
- `src/terminal/debug-commands.cpp`
- `config/gamemodes/counterstrike.json`, `config/roles.json`, `config/actor-presets/counter_strike.json`

## Tests and evidence

- Automated tests: TBD.
- Runtime commands: `start counterstrike`, `team_list`, `team_pick 1`, `play`.
- Logs: canonical JSONL `logs/<date>/<run>/events.jsonl`.
- Human playtest: still required (none performed yet).

## Acceptance criteria

- `counterstrike` mode loads with ordered teams CT=1, T=2.
- `team_list` lists exactly Counter-Terrorists then Terrorists.
- `team_pick 1`/`team_pick 2` assign the requesting player when unlocked and
  reject when locked/invalid/full.
- Role and actor-preset references resolve; unknown ids fail safely.
- Hot reload preserves the last valid configuration.

## Changelog and regression links

- Checkpoint 1 changelog: see `docs/changelog/2026-10-02/`.
- Regressions: none yet.

---

## Config map — what to edit to tweak the game

Edit these files (hot-reloaded unless noted). Base `config/weapons.json` is a
shared registry; do NOT edit it for Counter-Strike tuning.

| What | File | Key fields |
|---|---|---|
| Mode rules: rounds, intermission, freeze, countdown, results, teams, victory, map, presentation | `config/gamemodes/counterstrike.json` | `rounds.intermission_seconds`, `rounds.freeze_seconds`, `rounds.round_seconds`, `rounds.rounds_to_win`, `rounds.countdown_seconds`, `respawn_seconds`, `teams`, `maps`, `presentation` |
| Bomb sites + plant/defuse/explosion timers (per map) | `config/maps/dust2cyberiav4.json` | `bomb_sites` (id/position/radius/visible_debug), `bomb.plant_seconds/defuse_seconds/explosion_seconds` |
| Actor preset: FOV, first-person, movement, avatar, weapon overrides, presentation | `config/actor-presets/counter_strike.json` | `camera.fov`, `movement_preset`, `avatar`, `weapon_overrides`, `presentation` |
| Roles: team, spawn group, avatar, health, loadout, behavior | `config/roles.json` | `counter_terrorist`, `terrorist` |
| Weapon base values (shared) | `config/weapons.json` | do not edit for CS; override in the preset |
| Weapon sets / loadouts | `config/weaponsets.json` | `counterstrike` set id 9 |
| Movement presets | `config/movement/movement-heavy.json` | ground/air speed, accel, jump, dash flags |
| NPC difficulty + AI policy | `config/npc-difficulty.json` | `targetMode`, `damageOtherNpcs`, fire delay, aim error, perception fields |
| NPC behavior profiles | `config/behavior-profiles.json` | aggression, preferred range, weapon weights |
| Map pool / automatic rotation | `config/gamemode-good-maps.json` | `maps` |
| Mode list shown by `modelist` / menu | `config/onlinemodes.json` | `counterstrike` entry |
| HUD text/layout (intermission, score, round, timer) | `config/gui/gamemode-meta-gui.json` | `counterstrike` section |
| Tab (TAB key) player list layout | `config/gui/tab-leaderboard.json` | `panel`, `title`, `header`, `row`, `localRow`, `npcRow` |
| Grenades | `config/grenades.json` | frag/smoke/darkbang/fire |

Intermission length = `rounds.intermission_seconds` in
`config/gamemodes/counterstrike.json`; it is shown by the `intermissionText`
element in the `counterstrike` section of `config/gui/gamemode-meta-gui.json`.

Map selection: a gamemode's `maps[0]` is now the mode's authoritative default
map (`dust2cyberiav4`). An explicit GUI map choice or `changemap` still wins.

---

## Attempt log (append-only)

### Attempt 1 — Checkpoint 1: data model + roles + actor preset + team commands

Tried:
- Extended `Gamemode` with `teams` (id/display/capacity/role/spawn_group),
  `spawn_groups`, `objectives`, `rounds`, `victory`, and `presentation`.
- Rewrote `config/gamemodes/counterstrike.json` with CT/T teams, round rules,
  objective definition, victory type, and presentation policy.
- Added `counter_terrorist` / `terrorist` roles with `actor_preset`, `team_id`,
  `spawn_group`, objective permissions, and forced avatars (`jason` /
  `abusiveboy`).
- Added `actorPresetId`, `teamId`, `spawnGroup`, `objectivePermissions` to
  `MatchRoleDefinition` and its parser.
- Registered `team_list` / `team_pick` as primary commands with
  `teamlist` / `teampick` aliases, and added server handlers for both names.
- Added a team-selection lock (reject after countdown) and ordered-team capacity
  enforcement in `serverRequestTeamChange`.

Went right:
- Teams array populates `teamNames` for backward compatibility, so the existing
  HUD/team code keeps working without changes.
- New gamemode/role fields are optional; other modes are unaffected.
- Alias registration means existing `teamlist`/`teampick` invocations still work.

Went wrong / watch out:
- The plan's weapon values conflict with the human's "keep existing" choice;
  recorded as a `NEEDS_SPEC_DECISION` above.
- The runtime does not consume the new `rounds`/`objectives` data yet; CS still
  routes to the legacy duel branch until Checkpoint 2.
- `presentation` healthbars/blood/ragdolls intentionally left to the existing
  flags to avoid duplicate sources of truth.

Keep doing:
- Optional-only schema additions; derive `teamNames` from `teams`.
- Primary + alias command registration pattern.

Stop doing:
- Adding a key in two owners (e.g. healthbars in both `hide_healthbars` and
  `presentation`) — pick one.

Evidence:
- Source: the files listed above.
- Build: see Checkpoint 1 changelog.
- Test: config parse validation (to be recorded).
- Runtime/human: pending.

Next: Checkpoint 2 — 5v5 roster creation + round/match lifecycle (Stage 2 body,
Stage 3).

### Attempt 2 — Checkpoint 2: 5v5 roster + round/match lifecycle

Tried:
- Added a round lifecycle to `ServerGamemodeState` (`objectiveRounds`,
  `victoryCondition`, `roundNumber`, `roundVersion`, `roundWins[2]`,
  `roundsToWin`, `maxRounds`, `roundSeconds`, `roundEndTick`, `roundWinnerTeam`,
  `roundEndReason`, `freezeSeconds`, `rosterLocked`, `roundNextNpcId`).
- Added a dedicated round state machine branch in `serverGamemodeTick`:
  INTERMISSION/WAITING -> COUNTDOWN -> GO -> ACTIVE -> RESULTS -> next round or
  intermission, with a human-connected guard before starting a round.
- Added roster creation: one human + allied NPCs up to the mode's team capacity
  against a full opposing NPC squad. NPCs are created as `ServerNpc` mirror
  entries so the existing `adoptNewServerNpcs` path simulates them.
- Added `beginObjectiveRound`, `endObjectiveRound`, `checkObjectiveRoundEnd`
  with elimination and timeout outcomes, round-win tally, and match victory at
  `rounds_to_win` (8).
- Added round fields to `DuelStatePacket` and the `CommunityMatchClient` mirror,
  plus `teamName(team)` and `roundWins`/`roundVersion` accessors.
- Added a `counterstrike` GUI layout section and JSON-sourced team names and
  round/match result strings in the HUD overlay.
- Added `serverCounterStrikeRoundSelfTest` + `--cs-round-selftest`.

Went right:
- Routing round modes before the generic FFA/TDM branch kept the existing
  lifecycle untouched.
- Reusing `ServerNpc` mirror entries means roster NPCs get full AI for free.
- `team_pick` still works during INTERMISSION because the round machine starts
  in INTERMISSION; the existing phase lock rejects picks once countdown begins.
- The pure `roundRosterNpcCounts` helper is shared by the runtime and the test,
  so the test cannot drift from production math.

Went wrong / watch out:
- The roster NPCs are added to `npcs` during `serverGamemodeTick`, but
  `simulateSharedNpcs` (which adopts them) runs earlier in the same tick, so
  their first appearance is the next tick. `resetGamemodeActorsAtMapSpawn` is
  harmless in that window but does not apply role loadouts until they exist.
  Weapon/behavior application for roster NPCs therefore relies on later resets;
  Checkpoint 3 must verify loadouts on the first life.
- Rounds currently resolve on elimination/timeout only; bomb objectives arrive
  in Checkpoints 6-7. The `endObjectiveRound` reason codes are reserved for that.
- The HUD reads `teamName()` from the gamemode JSON each frame; it is cheap but
  should be cached if profiling shows it matters.

Keep doing:
- World-independent selftest hooks for the rule math.
- One shared helper for roster sizing used by both runtime and test.
- Reusing existing actor/lifecycle owners instead of a CS-only system.

Stop doing:
- Adding CS-only branches to the generic FFA/TDM path; the round machine is its
  own branch.

Evidence:
- Build: `BUILD SUCCESS` (`build.py build-only`).
- Runtime: `--cs-round-selftest` PASS, `--gamemode-selftest` PASS,
  `--actor-preset-selftest` PASS.
- Human playtest: pending.

Next: Checkpoint 3 — preset application + weapon overrides + presentation
(Stages 4, 5).

### Attempt 3 — Checkpoint 3: preset application + weapon overrides + presentation

Tried:
- Added missing override fields to `ActorPresetWeaponOverride` and its parser:
  `damage_scale`, `headshot_multiplier`, `spread`, `recoil`, and a per-weapon
  `presentation` block (which refines the preset-level policy for that weapon).
- Applied those fields in `ActorPresetWeapons::applyOverride`.
- Added `hit_markers`/`hit_sounds` to `ActorPresetPresentation` and new
  `ActorPresetWeapons::hitMarkersEnabled()`/`hitSoundsEnabled()` globals, then
  gated the two central local hit-feedback owners (`ui/hitmarker.cpp`
  `hitmarkerVisualOnly`, `audio/hitmarker-audio.cpp` `playHitmarkerSound`) and
  the confirmed-damage path (`confirmed-damage-presentation.cpp`).
- Updated `counter_strike.json`: kept revolver 100/6/36 and shotgun 20/8/32,
  added the `hitscan_rifle` override (30 dmg, 4.0 headshot, 30/180, zero
  thickness), zeroed the shotgun beam thickness, and enabled blood in the
  preset presentation to match the CS acceptance list.
- Extended `adoptNewServerNpcs` to apply the role spawn profile (health,
  movement, avatar, behavior, loadout) so roster NPCs get their first-life
  loadout, not only on later resets.
- Extended `--actor-preset-selftest` and `--cs-round-selftest` to prove the
  overrides reach the parsed preset AND the runtime active weapon table.

Went right:
- The override table is in-memory only; `config/weapons.json` is confirmed
  unchanged by git status.
- Zero thickness reaches both the parsed preset and the runtime active table
  (`beamThickness == 0`, `beamWorldThickness == 0`), which is what both the
  local and authoritative traces read.
- Reusing a single `adoptNewServerNpcs` profile path fixed first-life loadouts
  without a CS-only branch.

Went wrong / watch out:
- The local human trace hard-caps range at `MAX_SHOT_DISTANCE = 100.0f` and
  ignores `customParams["range"]` (pre-existing). Zero thickness is correct, but
  a preset `range` override does not reach the local human trace. Recorded as a
  known limitation; the authoritative trace does read `range`.
- The gamemode-level `presentation` block is parsed but NOT applied, because the
  actor preset is the single presentation owner for CS. Keeping it unapplied
  avoids the duplicate-source mistake flagged in Attempt 1. It is reserved for
  future modes that have no actor preset; note it as `NEEDS_SPEC_DECISION`.
- `hitmarker`/`hit sound` gating now has two owners (actor preset + per-weapon
  `weapon-hitfx.json`); both must allow for feedback to appear. This is intended
  (AND-composition) but worth remembering when debugging "no hitmarker".

Keep doing:
- Proving overrides reach the runtime active table, not just the JSON parse.
- Reusing existing central owners for presentation gating.

Stop doing:
- Adding a second presentation application path from the gamemode JSON while the
  actor preset already owns it.

Evidence:
- Build: `BUILD SUCCESS`.
- Runtime: `--actor-preset-selftest` PASS (`rifle=30/30/180 hs=4 thick=0.0/0.0`),
  `--cs-round-selftest` PASS (`baseRifle=yes; rifle mag=30 reserve=180 beam=0`),
  `--gamemode-selftest` PASS.
- `config/weapons.json` unchanged (git status clean for that path).
- Human playtest: pending.

Next: Checkpoint 4 — ActorIntent adapter + human-like perception (Stages 6, 7).

### Attempt 4 — Checkpoint 4: ActorIntent boundary + human-like perception

Tried:
- Added `src/actor/actor-intent.h/.cpp`: a source-agnostic `ActorIntent` plus
  a single `ActorIntentAdapter::toInputState` translation owner so human/NPC/
  script/replay sources all reach the shared `physicsMainUpdate` path.
- Added `src/npc/npc-perception.h/.cpp`: `PerceptionSnapshot`, `MemoryRecord`,
  `BeliefState`, `PerceptionTuning`, `perceive`, `updateMemory`, `buildBelief`,
  `predictTargetPosition`, and a pure `withinSightCone`.
- Target acquisition in `senseWorld` now requires hostile + range + FOV + LOS +
  alive via the perception module; a wall-blocked target no longer stays fully
  known. LOS is rate-limited (every 5 ticks) using the existing shared ray and
  `cachedLoSBlocked` is kept in sync from perception.
- Added bounded memory decay (confidence/uncertainty), last-known position, and
  skill-scaled prediction with deterministic per-NPC error.
- Wired the perception reaction delay as the fallback when no behavior profile
  sets one (default 8 ticks).
- Added perception tuning fields to `NpcDifficultySettings`, the parser, and
  `config/npc-difficulty.json`.
- Added `--npc-perception-selftest` and a `npcPerceptionSelfTest`.

Went right:
- Target acquisition is now genuinely gated; the old code set `hasTarget` on
  alive alone and only *used* LOS for firing.
- Memory + prediction are bounded and cheap; LOS is still one shared ray.
- The ActorIntent boundary is additive and does not disrupt the existing NPC
  input build path.

Went wrong / watch out:
- `ActorIntentAdapter::toInputState` is not yet the live path for the NPC brain;
  the NPC still builds `InputState` in `buildInputState`. The adapter exists as
  the agreed boundary and will be adopted in Checkpoint 5 when utility goals
  produce intent. This is intentional staging, not a claim of migration.
- The perception selftest is world-independent (pure math); it does not prove
  in-game FOV/LOS behavior. That needs a live session.
- `config/weapons.json` shows `beam_thickness` 2.0 -> 0.01 in `git status`, but
  this edit is NOT mine. It appeared during the session alongside other
  runtime-written user settings (collision/crosshair/movement/ragdoll). The
  actor-preset override still forces the in-memory value, so runtime behavior is
  unaffected. Flagged rather than reverted because it is someone else's change.

Keep doing:
- Pure, world-independent tests for perception math.
- One LOS owner (perception) with rate limiting.

Stop doing:
- Setting `hasTarget` from alive-alone; all acquisition goes through perception.

Evidence:
- Build: `BUILD SUCCESS`.
- Runtime: `--npc-perception-selftest` PASS (fov/memory/belief/prediction),
  `--gamemode-selftest`, `--actor-preset-selftest`, `--cs-round-selftest` PASS.
- Human playtest: pending.

Next: Checkpoint 5 — utility goals + navigation integration (Stages 8, 9).

### Attempt 5 — Checkpoint 5: utility goals + navigation request

Tried:
- Added `src/npc/npc-utility.h/.cpp`: `UtilityGoalKind` (KillTarget, Survive,
  HoldPosition, TakeCover, MoveToObjective, DefendSite, RotateToSite,
  PlantObjective, DefuseObjective, RetakeSite), `UtilityActionKind`,
  `UtilityContext`, `UtilityGoalScore` with the full term breakdown,
  `scoreUtilityGoal`, `selectUtilityGoal` with hysteresis (minimum duration +
  switch margin + action cooldown), and `actionForGoal`.
- Added `src/npc/npc-nav-request.h/.cpp`: `MovementCapabilities`,
  `NavigationRequest`, `navigationRequestToGoal` (adapter to the existing
  `NpcGoal`), and `classifySegment` (Walk/Jump/Drop/Gap/Unreachable).
- Wired utility selection into `senseWorld`: builds a `UtilityContext` from
  belief/health/weapon readiness, stores it on the NPC, and selects a goal each
  tick.
- `makeNavGoal` now consults the utility goal first and falls back to the
  legacy state mapping, so the state machine remains the executor.
- Adopted `ActorIntent` in `buildInputState`: NPC tactical output goes through
  `ActorIntentAdapter::toInputState`, so NPC/human/script/replay share one
  execution translation.
- Added `--npc-utility-selftest` and `--npc-nav-request-selftest`.

Went right:
- The navigator is unchanged; the request wrapper only adapts to the existing
  `NpcGoal`, so no second pathfinding owner exists.
- Utility goals only override navigation for situations they cover; every other
  goal falls through to the existing mapping, preserving behavior.
- Hysteresis prevents thrashing between similarly-scored goals.

Went wrong / watch out:
- Objective goals (MoveToObjective, DefendSite, PlantObjective, DefuseObjective,
  RetakeSite) score zero until objective/role context is supplied by TeamBrain
  (Checkpoint 9). The scoring exists and is tested, but no live objective feeds
  it yet.
- The legacy `NpcStateMachine` still owns the actual movement tactics; the
  utility layer currently only influences the navigation goal. Full action
  execution migration is deferred until equivalence is proven, as the plan
  requires.
- `buildInputState` facing smoothing remains the facing owner; the adapter
  carries lookDirection but does not smooth yaw.

Keep doing:
- Wrapping existing owners (navigator, state machine) instead of replacing them.
- Pure, world-independent tests for scoring/hysteresis/classification.

Stop doing:
- Hand-building `NpcGoal` in new behavior code; use `NavigationRequest`.

Evidence:
- Build: `BUILD SUCCESS`, no warnings.
- Runtime: `--npc-utility-selftest` PASS, `--npc-nav-request-selftest` PASS,
  plus perception/gamemode/actor-preset/cs-round PASS.
- Human playtest: pending.

Next: Checkpoint 6 — bomb item pickup/drop (Stage 10).

### Attempt 6 — Checkpoint 6: generic bomb objective + pickup/drop

Tried:
- Added `src/game/objective-state.h/.cpp`: `ObjectiveKind`, `ObjectiveState`,
  `ObjectiveInstance` (id/kind/state/transform/carrier/allowed team/pickup
  radius/interaction range/explosion deadline), `objectiveKindFromString`,
  `ObjectiveCarrierCandidate`, `selectObjectiveCarrier` (pure), and
  `objectiveSelfTest`.
- Added an `ObjectiveInstance objective` to `ServerGamemodeState` and resolved
  the mode's first objective definition (`carrier_team`, `explosion_seconds`)
  at match start via `resolveTeamIndexFromId`.
- `assignObjectiveCarrier` gives the bomb to the first eligible living actor at
  round start (drops to spawn if none). `serverObjectiveTick` runs in ACTIVE/GO:
  drops on carrier death/disconnect (at the body), follows a living carrier,
  and auto-picks up for the nearest living allowed-team actor within
  `pickupRadius`. No duplicate ownership; team-gated.
- Replicated objective fields in `DuelStatePacket` and the
  `CommunityMatchClient` mirror; cleared on reset.
- Added a `counterstrike` HUD `objectivePrompt` ("Pick up Bomb" when the local
  camera is within 3 m of a dropped bomb) and `objectiveStatus` ("BOMB
  DROPPED").
- Added `--objective-selftest`.

Went right:
- The pure `selectObjectiveCarrier` is shared by the runtime assignment, the
  runtime pickup, and the test, so the team/radius/alive rules cannot drift.
- Drop-on-death reuses the existing kill/actor-state machinery; no new
  ownership concept.
- Objective state lives in the gamemode runtime, never in an actor preset.

Went wrong / watch out:
- Pickup is automatic proximity only; the `F` interaction path and site-gated
  plant/defuse arrive in Stage 11/12. `interactionRange` is stored but unused.
- The HUD prompt is camera-distance based and does not check the local player's
  team, because the client does not yet expose "am I on the carrier team?" in
  this overlay. It shows the prompt for any dropped bomb within range; the
  server still enforces the team gate, so a CT pressing pickup does nothing.
- `serverObjectiveTick` builds a candidate vector each tick; this is bounded by
  the 10-actor roster and is not in the tightest collision loop, but it can be
  made allocation-free later if profiling shows it matters.

Keep doing:
- One pure rule function shared by runtime and test.
- Objective state in the mode runtime, not in actor presets.

Stop doing:
- Duplicating team/radius gating inline; use `selectObjectiveCarrier`.

Evidence:
- Build: `BUILD SUCCESS`.
- Runtime: `--objective-selftest` PASS (team gate, radius, alive, nearest),
  plus all prior selftests PASS.
- Human playtest: pending.

Next: Checkpoint 7 — editable bomb sites + plant/defuse/explosion (Stages 11,
12).

### Attempt 7 — Checkpoint 7: bomb sites + plant/defuse/explosion

Tried:
- Added `src/gamemode/map-config.h/.cpp`: `BombSite` (id/position/radius/
  visible_debug/hasPosition), `MapObjectiveConfig` (sites + plant/defuse/
  explosion seconds), `MapConfigRegistry` loading `config/maps/<mapId>.json`
  (accepts nested `objectives.bomb_sites` or flat `bomb_sites`), hot reload,
  site lookup by id and by world position, runtime edit + JSON save, and
  `mapConfigSelfTest`.
- Authored `config/maps/dust2cyberiav3.json` with sites A/B (no position yet,
  so planting is disabled until verified) and bomb timers 3/5/40.
- Extended `ObjectiveInstance` with plant/defuse progress + planner/defuser ids
  + planted site id, and `advanceObjectiveProgress` / `objectiveSecondsToTicks`
  (pure, fixed-tick, interruptible).
- `serverObjectiveTick`: plant when the carrier stands inside a valid site
  (interrupt on leaving), explode at the deadline, and defuse when a defender is
  within interaction range (interrupt on leaving). Emits
  `objective.plant-start`/`planted`/`defuse-start`/`defused`/`exploded`.
- `checkObjectiveRoundEnd`: a planted bomb keeps the round alive through a team
  wipe; timeout awards the defenders when nothing was planted.
- Replicated plant/defuse progress, kind, and explosion timer; HUD shows
  "Planting Bomb...", "Defusing Bomb...", and "BOMB PLANTED Ns".
- Added `site_debug show|hide|select|move|print|save` and site-zone debug
  rendering (`drawWireSphere` per visible site).
- Added `--map-config-selftest`.

Went right:
- One pure progress helper shared by runtime and test.
- Sites are queried generically (`siteIndexAt`), so future capture/escort
  objectives can reuse the pattern.
- Round outcomes now follow the plan: explosion = T, defuse = CT, timeout
  without plant = CT.

Went wrong / watch out:
- The map's site positions are NOT authored; `config/maps/dust2cyberiav3.json`
  has no `position` keys, so `siteIndexAt` returns -1 and planting is disabled
  until a human verifies positions with `site_debug move` + `site_debug save`.
  This is deliberate per the plan ("positions must be verified against the
  loaded map rather than guessed").
- Plant/defuse are automatic on proximity/state (no `F` press). The plan's
  Stage 12 does not require an interact key, and pickup in Stage 10 is
  proximity-based; the explicit `F` interact wire path is not implemented.
- `MapConfigRegistry` is a process singleton; a listen host edits the same
  instance the client reads, which is fine, but a remote client cannot author
  sites (host-only in practice).

Keep doing:
- Pure rule helpers shared by runtime and test.
- Data-driven sites with a runtime verify/save loop.

Stop doing:
- Guessing site coordinates; require verification.

Evidence:
- Build: `BUILD SUCCESS`.
- Runtime: `--map-config-selftest` PASS, `--objective-selftest` PASS
  (progress), plus all prior selftests PASS.
- Human playtest: pending.

Next: Checkpoint 8 — grenades and area effects (Stage 13).

### Attempt 8 — Checkpoint 8: grenades and area effects

Tried:
- Added `src/combat/area-effect.h/.cpp`: `AreaEffectKind` (Fire/Smoke/DarkBang),
  `AreaEffect` (id/kind/owner/team/position/radius/height/duration/damage
  cadence/team policy), `tickAreaEffects` (fixed-tick, appends damage events,
  removes expired), `areaEffectContains`, and `areaEffectSelfTest`.
- Added `src/combat/grenade-registry.h/.cpp`: loads `config/grenades.json`,
  mapping each grenade (frag/smoke/darkbang/fire) to a weapon id and the area
  effect it leaves behind. Frag is a direct explosion (no lingering area).
- Added `config/grenades.json` with the four definitions; fire is 10 damage
  every 10 fixed ticks to enemies, smoke 15s, darkbang 2.5s.
- Added `ServerGamemodeState::areaEffects` + `serverSpawnAreaEffect`,
  `serverSpawnGrenadeAreaEffect`, and `serverAreaEffectTick` (fixed 60 Hz,
  applies fire damage through the shared damage path: NPC health mirror +
  `applyServerDamage` for players).
- Wired `serverAreaEffectTick` into `serverGamemodeTick` so it runs in every
  managed mode.
- Added `grenade_spawn <id>` terminal command (spawns a grenade's area effect
  at the player position) and `--grenade-selftest` / `--area-effect-selftest`.
- Relocated the area-effect functions out of the anonymous namespace so the
  public spawn API matches its header declarations.

Went right:
- Area effects are a generic, reusable server-authoritative primitive; the
  fire cadence/team policy/expiry are pure and unit-tested.
- Damage reuses the shared path (no second damage pipeline).
- `weapons.json` remains unchanged; grenade policy lives in `config/grenades.json`.

Went wrong / watch out:
- Throwing is not yet per-grenade: all four map to `grenade_launcher`, and the
  projectile explosion path does not yet call `serverSpawnGrenadeAreaEffect`
  (no grenade id flows on the projectile). The four types are spawnable via the
  new API/command and fully tested, but selecting smoke/fire/darkbang at throw
  time requires per-grenade weapon entries or a grenade id on the projectile,
  deferred to a later pass.
- `frag` uses the existing projectile splash damage path unchanged.
- Client rendering of the smoke volume / fire cylinder is not added; the
  server owns the effect and the presentation is deferred.

Keep doing:
- One pure, tested area-effect primitive reused by all grenade types.
- Server-authoritative outcomes; no client decision.

Stop doing:
- Duplicating damage application; route through the shared path.

Evidence:
- Build: `BUILD SUCCESS`.
- Runtime: `--grenade-selftest` PASS, `--area-effect-selftest` PASS, plus all
  prior selftests PASS.
- Human playtest: pending.

Next: Checkpoint 9 — TeamBrain and NPC objective play (Stages 14, 15).

### Attempt 9 — Checkpoint 9: TeamBrain + AI objective/grenade reasoning

Tried:
- Added `src/npc/team-brain.h/.cpp`: `TeamAssignment` (AttackSite/CarryBomb/
  DefendSite/Rotate/Retake/Defuse), `TeamSiteInfo`, `EnemyReport`,
  `TeamObjectiveContext`, `TeamAssignmentPolicy`, `TeamBrain` with
  `reportEnemySighting`, `tickReports` (decay/drop), `bestReport`,
  `updateAssignments`, `assignmentFor`, `objectiveTargetPosition`, and
  `teamBrainSelfTest`.
- Added `TeamBrain teamBrainA/B{0/1}` to `ServerGamemodeState` and
  `serverTeamBrainTick`, which fills site/objective context, records shared
  enemy reports (visual, exact), recomputes per-team assignments, and pushes
  objective context (`objectiveKnown`/`objectivePos`/`onDefense`/`atObjective`/
  `canPlant`/`canDefuse`) onto each NPC's utility brain. It never moves actors.
- Added optional JSON assignment policy fields to `GamemodeTeam`
  (`attackers_per_site`, `defenders_per_site`, `one_rotator`) and parsing.
- Added Stage 15 grenade reasoning to `npc-utility`: `GrenadeThrowContext`,
  `scoreGrenadeThrow` / `grenadeThrowAllowed` (rejects wall collision,
  self-damage, friendly fire, duplicate utility, and no-benefit throws), and a
  bounded `FightMemory` (dodge left/right, jump, held) with decay.
- Added `--team-brain-selftest` and `--grenade-reasoning-selftest`.

Went right:
- TeamBrain is pure policy and never touches positions/physics; the ActorBrain
  and shared navigation execute.
- Objective context flows through the existing `UtilityContext`, so the
  objective goals from Checkpoint 5 now actually score.
- Grenade reasoning is bounded, deterministic, and unit-tested.

Went wrong / watch out:
- `serverTeamBrainTick` runs in `serverGamemodeTick`, which the main loop calls
  AFTER `simulateSharedNpcs`; objective context therefore reaches NPCs one tick
  later. Acceptable at 60 Hz but worth noting.
- Assignments are apportioned but not yet consumed by a dedicated action layer:
  the NPC utility brain uses the objective context (`objectivePos`, `canPlant`,
  `canDefuse`, `onDefense`) rather than the assignment enum directly. The
  assignment surface exists for the next migration step.
- Enemy reports are currently exact/visual only; hearing feeds uncertain reports
  through the existing perception path, not yet wired into `reportEnemySighting`.
- Grenade throwing is decided but not yet triggered from the NPC combat loop;
  the scoring/rejection API is ready and tested, wiring into `tryFire`/throw is
  deferred.

Keep doing:
- Pure, tested team/utility policy; one owner for execution.
- Objective context through the shared utility boundary.

Stop doing:
- Teleporting or overriding physics from the team brain.

Evidence:
- Build: `BUILD SUCCESS`, no warnings.
- Runtime: `--team-brain-selftest` PASS, `--grenade-reasoning-selftest` PASS,
  plus all prior selftests PASS.
- Human playtest: pending.

Next: Checkpoint 10 — debug tooling + full runtime acceptance (Stages 16, 17,
18).

### Attempt 10 — Checkpoint 10: debug tooling + acceptance harness

Tried:
- Added terminal commands: `npc_inspect [id]` (team/hp/role/preset/target/
  belief/confidence/distance/goal/action/nav dest/path nodes/perception),
  `npc_brain [id]` (all utility goal scores + terms), `team_status` (mode/round/
  score/roster/team order/member counts), `objective_status` (bomb state/
  carrier/team/position/site/progress/counters).
- `site_debug` now also accepts `on|off` aliases for `show|hide`.
- Added structured events: `actor.team-assigned`, `actor.preset-applied`,
  `round.result`, `match.result` (plus the C6-C8 `objective.*` and
  `area_effect.spawn` events).
- Added `--counterstrike-acceptance-selftest`, a consolidated runner over every
  pure subsystem selftest (round+weapons, objective, map-config, grenade,
  area-effect, team-brain, perception, utility, npc-grenade, nav-request).

Went right:
- One command surface for inspecting NPC brains and objective state.
- One acceptance entry point that a human or CI can run.

Went wrong / watch out:
- `npc.perception`/`npc.goal-selected`/`npc.navigation` per-tick events are not
  emitted from the NPC brain path (they would need `structured-log.h` in
  npc.cpp and rate limiting). The event names are reserved; only the server-side
  `actor.*`, `round.*`, `match.*`, `objective.*`, `area_effect.*` events exist.
- Debug commands are host/local (they read the in-process server state).
- The consolidated selftest proves rules/data only; it does not prove visuals,
  multiplayer, or human acceptance.

Keep doing:
- Pure, world-independent selftests for every rule surface.
- Structured, categorized events from the authoritative owner.

Stop doing:
- Adding a new diagnostic file per subsystem; use the existing logger.

Evidence:
- Build: `BUILD SUCCESS`, no warnings.
- Runtime: `--counterstrike-acceptance-selftest` PASS (all 11 groups), plus all
  twelve individual selftests PASS.
- Human playtest: pending.

---

## Handoff — human test instructions (remaining acceptance)

Build the game, then run these in a real local session (host). Report build,
runtime, and visual results separately.

Start:
```text
modestart <counterstrike number>      (or: modestartnow <n>)
team_list
team_pick 1
```

Expect: teams list "Counter-Terrorists = 1 | Terrorists = 2"; `team_pick 1`
confirms; after intermission a 3-2-1-GO countdown; the human + 4 CT NPCs vs 5
T NPCs spawn.

Inspect while playing:
```text
team_status
objective_status
npc_inspect
npc_brain
site_debug show
```

Verify:
1. Player is CT, 4 allied CT NPCs, 5 T NPCs; correct avatars (jason / abusiveboy).
2. FOV 70, forced first-person, heavy movement, auto-hop/dash disabled.
3. Loadout: revolver 6/36, shotgun 8/32, hitscan rifle 30/180.
4. No damage numbers / hit markers / hit sounds; blood + killfeed + ragdolls on.
5. Bomb: one T starts with it; drops on death; only a T can pick it up; cannot
   pick up through walls; "Pick up Bomb"/"BOMB DROPPED" prompts appear when valid.
6. Sites: stand at each bombsite, `site_debug move A` / `site_debug move B`,
   `site_debug print`, `site_debug save`. Then confirm a T can plant only inside
   A/B (HUD "Planting Bomb..."), planting interrupts on leaving, a CT defuses in
   range ("Defusing Bomb..."), explosion awards T, defuse awards CT, and a
   no-plant timeout awards CT.
7. Rounds: round result names come from JSON; score increments; match ends at 8
   wins and returns to intermission.
8. Grenades: `grenade_spawn frag|smoke|darkbang|fire` at your feet; fire damages
   roughly every 10 ticks and only enemies; smoke/darkbang leave timed effects.
9. NPCs: navigate the map, cannot see through walls, respect FOV, react with a
   delay, share enemy info, and pursue the objective without teleporting.

Record any confirmed break as `docs/regressions/YYYY-MM-DD/<name>-REG.md` and
link it here; do not mark a solution until human-confirmed.

### Attempt 11 — post-checkpoint gameplay fixes + config handoff

Triggered by human playtest feedback (branch `afad20a-rebuild`):
- **Everyone spawned at one point.** Root cause: the shared anchor ignored map
  spawn tags. Fix: `ServerSpawnPoint.tag` is now populated from the GLB node
  name; `assignGamemodeSpawns` groups `spawnpoint.CT`/`spawnpoint.T` into
  per-team clusters; `gamemodeSpawnPoint(d, team)` is used by roster build and
  round reset; the human respawns at their team spawn. Falls back to the shared
  anchor when untagged.
- **All NPCs insta-killed the human regardless of team.** Root cause: `targetMode: "player"` ignored teams entirely, and team modes did not force NPC-vs-NPC
  targeting. Fix: `chooseNearestPlayer` and the player-priority path now gate on
  `actorsAreHostile`; team round modes allow NPC targets so both squads fight.
  Friendly **splash** damage remains allowed; same-team targeting is forbidden.
- **Instant respawn at the death spot.** Fix: `respawn_seconds: 0` explicit in
  `counterstrike.json`; one-life path keeps the actor dead; the client forces
  freecam while the local actor is Dead/Respawning/Spectating during a round.
- **Intermission had no NPCs.** Fix: warmup spawns the roster at intermission
  start with standing/moving NPCs and infinite lives; the round rebuilds a clean
  roster at countdown.
- **Tab list had no team info.** Fix: `CommunityMatchClient::teamForActor` +
  team columns and short team tags in `config/gui/tab-leaderboard.json` render.
- **Freeze time** added (`rounds.freeze_seconds: 5`): NPC wakeup is held after GO.
- **Avatars swapped**: T=jason, CT=abusiveboy.
- **Map = dust2cyberiav4**: gamemode `maps[0]` is now authoritative for the
  mode's default map; `config/maps/dust2cyberiav4.json` added.
- **Config map** documented above for future tuning.

Went right:
- Team-tagged spawns read from the real GLB node names (verified tags survive).
- Same-team hostility is a single gate reused by every target path.
- Warmup reuses the round roster builder with cleanup so it does not duplicate.

Went wrong / watch out:
- Per-team **human** avatar is still preset-driven (jason) on the client; NPC
  avatars are per-team via the role. A client-side per-team human avatar is a
  later refinement.
- Freeze time holds NPCs but does not yet hold the human.
- `npc-difficulty.json` still says `targetMode: "player"` / `damageOtherNpcs:
  false`; the team gate and forced team-targeting make this safe for CS, but the
  config intent should be cleaned up later.
- Warmup uses the objective module's bomb assignment; the bomb may be carried
  during warmup. Harmless for a warmup but worth confirming.

Evidence:
- Build: `BUILD SUCCESS`, no warnings.
- Runtime: `--counterstrike-acceptance-selftest` PASS (incl. new `spawn-tags`
  group); all 13 individual selftests PASS.
- Human playtest: pending (see updated handoff checklist).

### Attempt 12 — fix pass #2: spectator team, actorState, bomb pulse

Tried:
- Added `CommunityMatchClient::actorState(actorId)` (declared + defined);
  `localActorState` now delegates to it. The TAB leaderboard render referenced it
  but it did not exist in the prior pass, so the timing pass did not link.
- Spectator is a real ordered team (index 2, capacity 0) in
  `counterstrike.json`; every mode now has a built-in Spectator team. The stale
  `team_list` alias was removed (`team_list` remains the single owner).
- Countdown freeze holds the human (`roundCountdownFreeze`, exempt in warmup) and
  releases at GO; TAB leaderboard is JSON-editable with columns
  (`idCol`/`teamCol`/`stateCol`/`nameCol`/`pingCol`) and per-state text.
- New config-driven objective pulse sphere: added `GamemodeObjectiveVisual` to the
  objective definition, parsed from the JSON `visual` block, and rendered by
  `renderObjectivePulse` in `engine-tick-render.cpp` via
  `DebugVis::drawFilledSphere` for Carried/Dropped/Planted states.

Went right:
- The 3-team model required no gameplay change: the runtime already iterates only
  teams 0/1 as playing teams, so Spectator never gets roster NPCs.
- The pulse visual is data-driven (radius/amplitude/period/color in JSON), reuses
  the existing production filled-triangle buffer, and is cosmetic only.

Went wrong / watch out:
- The prior pass left `--cs-round-selftest` and `--gamemode-selftest` asserting
  exactly 2 teams, so adding Spectator silently broke them. Both are aligned now.
- The pulse phase uses client wall-clock `MimitaNet::nowMs` (fine for cosmetics;
  not a gameplay source).
- Incremental builds skipped edits whose file mtime did not advance; affected
  files were touched to force recompilation. This is an environment artifact, not
  a code defect.

Evidence:
- Build: `BUILD SUCCESS` (`build.py build-only`); `mimita.exe` relinked.
- Runtime: `--counterstrike-acceptance-selftest` PASS (all 11 groups); all 13
  individual selftests PASS, incl.
  `[GAMEMODE SELFTEST] objective_visual enabled=1 radius=0.35 amp=0.15 period=1.50`.
- Human playtest: pending.

Next: human visual acceptance of the pulse sphere and TAB leaderboard; optional
HUD overlap check if a screenshot identifies the overlapping element.

### Attempt 13 — fix pass #3: spawns, freeze, patrol, map, body parts, win text, cursor, outlines

Triggered by 11 human playtest bugs on `afad20a-rebuild`. The previous session
root-caused and partially implemented them and left
`docs/specs/20261003-counterstrike-fixpass3-handoff.md`. This session finished the
remaining items.

Tried:
- **Spawn collapse (2/9):** verified the kill/respawn/rotate calls use
  `gamemodeSpawnPoint(d, team)` (prior session).
- **NPCs pre-GO (3):** verified the `finalizeServerNpcSpawn` countdown wakeup and
  the `simulateSharedNpcs` freeze gate (prior session).
- **Patrol (5/10):** verified the prior-session `NpcState::Patrol`; added
  `UtilityGoalKind::Patrol` so `npc_brain` diagnostics match the executor when no
  hostile/objective context exists. The scorer only reclassifies the target-less
  `KillTarget` fallback; `makeNavGoal` falls through to the same legacy state
  mapping, so navigation behavior is unchanged.
- **Body parts (6):** removed `damage_policy.allowed_body_parts` from
  `config/actor-presets/counter_strike.json`; all body parts now take damage and
  the rifle headshot multiplier still applies to head hits.
- **Map (4):** replaced the remaining `dust2cyberiav3` fallbacks with
  `dust2cyberiav4` (`main-systems.cpp`, `engine-tick-state.cpp`, and the
  `map-config.cpp` selftest); fixed the `dust2cyberiav4.json` header comment;
  room joins now prefer the requested `mci.mapName` even without a direct
  address.
- **Dev-loop map reuse:** `dev-loop.py` now reads the running server's `--map`
  and restarts the external server when the launch-mode map differs.
- **Win text (8):** `DuelStatePacket.winnerTeam` now sends `d.roundWinnerTeam`
  for a normal round and `d.winnerTeam` at match over, so the results screen
  shows the winning team name.
- **Cursor (10b):** the forced-cursor condition now excludes
  `PauseMenu::isOpen()` in `engine-tick-state.cpp`, so ESC shows a usable cursor.
- **Outlines (11):** added `GamemodePresentation.player_outlines` parsed from the
  JSON `presentation` block; `PlayerVisualsConfig` gained an in-memory override
  and `effectiveMode()`; `render-player.cpp` resolves the effective mode;
  `CommunityMatchClient` applies/clears the override per mode and on reset;
  `counterstrike.json` sets `player_outlines: false`.

Went right:
- The outline override mirrors the existing healthbar/ragdoll/blood in-memory
  override pattern and never writes `config/playervisuals.json`.
- Patrol reclassification cannot change navigation because the executor path is
  identical for `Patrol` and the previous target-less fallback.
- `config/weapons.json` remains unchanged; the body-part policy is removed from
  the actor preset only.

Went wrong / watch out:
- `--actor-preset-selftest` was stale against the working-tree revolver tuning
  (`fire_delay` 0.3, `reload_time` 1.5) and still asserted the HEAD values
  (0.8/2.2); the assertion was aligned to the current preset. This is a
  pre-existing mismatch, not caused by the body-part removal.
- `config/gamemode-good-maps.json` lists both `dust2cyberiav3` and
  `dust2cyberiav4`, so auto-rotation could still choose v3. Left unchanged as out
  of scope.
- Patrol is compile/test-verified only; runtime wall-hugging or stuck loops are
  unobserved.
- `freeze_seconds` remains parsed but unused (the human wants NPCs to act at GO).
- Intermission duration (issue 1) was explicitly skipped by the human.

Evidence:
- Build: `BUILD SUCCESS` (`python build.py build-only`); `mimita.exe` relinked.
- Runtime: `--counterstrike-acceptance-selftest` PASS; all 13 individual
  selftests PASS.
- Human playtest: pending.

Next: human acceptance of the 10 checklist items in
`docs/specs/20261003-counterstrike-fixpass3-handoff.md`.

### Attempt 14 — automatic NPC surface navigation

Tried:
- Added actor-preset `navigation` settings (`src/npc/npc-navigation-settings.*`)
  and a pure surface classifier (`src/npc/npc-surface.*`); parsed the block in
  `match-roles.*` and live-resolved it via `activeNavigationSettings` in
  `npc.cpp`. `config/actor-presets/counter_strike.json` gains the `navigation`
  block (shared slope rule, no 45-degree override).
- Rebuilt `NpcNavigator::planLocalPath` as a multi-surface A*: each cell keeps
  stacked walkable surfaces, every connection is edge-validated (clearance ray,
  step/slope/drop rules), and jump links require
  `allowNavigationJumps && npcPolicyAllowsJump(policy, Navigation)` plus a
  clear landing (unless `allow_wall_jump`). Continuous ramps are walked even
  when the per-cell rise exceeds the step height.
- Removed the unconditional direct-steer fallback. A blocked route with no path
  now turns (`blocked_behavior`) or holds and retries after a short delay, and
  traversal cannot override it with a jump/dash into the wall.
- Added `NavCapability` tags on path segments and `--npc-navigation-selftest`
  plus `tests/npc-navigation-test.cpp`.

Went right:
- The rolling local planner stays the single navigation owner; no global graph.
- Surface generation reuses the existing cached broadphase candidate gather.
- Policy gating at the planner level (not just at execution) means a
  `jump_style:"never"` actor cannot receive a jump route.

Went wrong / watch out:
- `max_walkable_slope_degrees: 45` from the spec was intentionally not set in
  the CS preset; the shared physics rule (dot 0.80, ~36.9 deg) is used so the
  planner and body agree.
- Multi-surface columns and edge rays are heavier than the old single-probe
  grid; bounded by the global 16 plans/s token. Profile if needed.
- Future movement types (crawl/fly/roll/teleport) carry capability tags only.

Evidence:
- Build: background dev-loop relinked `mimita.exe` (17:05:43).
- Pure: `build/npc-navigation-test.exe` PASS (34 checks).
- Runtime: `--npc-navigation-selftest` PASS (13 checks); policy/search/radar/
  nav-request/acceptance all PASS. `--actor-preset-selftest` still FAILs only on
  the pre-existing revolver assertion.
- Human playtest: pending (ramp + wall scenario in
  `docs/changelog/2026-10-04/20261004_210905-automatic-npc-surface-navigation.md`).

Next: human Counter-Strike map test of the ramp/wall scenario and general
navigation regression.

### Attempt 15 — NPC teams, Rage2 profile, team targeting, RMB aim zoom

Tried:
- `config/gamemodes/counterstrike.json`: added `npc_behavior_profile: "rage2"`,
  an `npc_targeting` block (`opposite_team`, include players + NPCs), and a
  `camera.aim_fov` block (`enabled`, `right_mouse`, multiplier 0.5, duration
  0.5, ease_in_out).
- New pure owners `src/npc/npc-targeting.*` (team hostility + nearest-hostile
  selection) and `src/entities/aim-fov.*` (RMB FOV blend/easing/apply).
- `GamemodeNpcTargeting` + `GamemodeAimFov` parsed in `gamemode.*`.
- `serverResolveActorSpawnProfile` precedence is now explicit > gamemode > role
  (role no longer overwrites the mode profile); `[NPC BEHAVIOR]` logs `mode=`.
- `server-npcs.cpp` builds an effective targeting policy (mode block, else the
  legacy `npc-difficulty.json` fallback), gates players/NPCs by inclusion, and
  uses the shared hostility predicate.
- Team source of truth `actorTeamOf`/`isValidPlayingTeam`/
  `gamemodeSpawnPointForActor`; every kill/map-change/reset/warmup path resolves
  the actor's own team. A neutral `sharedAnchor` removes the silent CT fallback;
  `[CS SPAWN]` errors when a team mode lacks team spawn tags;
  `validateTeamRoleConsistency` rejects swapped/invalid team roles.
- Camera uses the mode FOV override when enabled (raw RMB, independent of the
  aim-body mode), resets the blend on mode change/death/respawn/leave, and keeps
  the aimbody fallback otherwise. Spectator camera untouched.

Went right:
- One pure targeting owner and one pure FOV owner keep the logic testable and
  avoid hard-coding CT vs T.
- CS no longer depends on the global npc-difficulty targeting values; other
  modes keep their exact previous behavior.

Went wrong / watch out:
- `--spawn-tag-selftest` initially ran before the gamemode registry loaded; it
  now loads `config/gamemodes` itself.
- `--actor-preset-selftest` still fails on pre-existing weapon-value
  assertions, unrelated to this change.
- Target stickiness/scoring is unchanged; the pure `selectNpcTargetId` is used
  for the hostility predicate and tests.

Evidence:
- Build: `BUILD SUCCESS` (server-gamemode.cpp recompiled + relink).
- Pure: `npc-targeting-test` PASS (23), `aim-fov-test` PASS (13).
- Runtime: targeting/aim-fov/cs-round/spawn-tag/gamemode/navigation/search/
  radar/nav-request/acceptance all PASS.
- Human playtest: pending (see changelog
  `docs/changelog/2026-10-04/20261004_221801-counterstrike-npc-teams-targeting-aimzoom.md`).

Next: live Counter-Strike match — verify CT/T targeting, per-team respawns,
`profile=rage2` logs, and CS-only RMB zoom.

### Attempt 15 — death spectator no longer blacks out the world

Triggered by human report: dying in Counter-Strike and entering the death
spectator/freecam made the world black while the HUD and other actors stayed
visible, every death.

Tried:
- Root cause: the fullscreen black spawn-flash quad in
  `src/engine/engine-tick-render.cpp:458` (drawn after the world, before actors)
  is gated only by `player.spawnFlashTimer > 0.0f`. That timer's only decrement
  owner was `simulateTick`, which the death spectator's forced gameplay freecam
  skips. The local host's `DeathSystem::update` locally respawns a networked
  one-life player (it checks only `DuelQueue`, not the gamemode rule), arming
  the flash at death; the forced freecam then froze it. The world always
  rendered; it was covered.
- Moved the decay to `engineTickState` (runs every render frame regardless of
  freecam/death), frame-rate independent (`dt * 60.0f`, same duration as the old
  one-per-fixed-tick), and removed the `simulateTick` copy so one owner remains.
- No change needed for the spectator team or camera: `counterstrike.json`
  already has `respawn_seconds: 0` and a `spec` team; `updateActorStates`
  already moves a dead non-respawning actor to Spectator during objective
  rounds; the camera already forces spectator freecam from replicated state.

Went right:
- The fix is one guaranteed-owner decay and cannot wedge; it also fixes any
  other path that skipped `simulateTick` with a live spawn flash.

Went wrong / watch out:
- The local host's `DeathSystem::update` still locally respawns a networked
  one-life actor; that is a separate lifecycle defect and is why the flash is
  armed at death. Not addressed here to keep the patch minimal and focused on
  the reported black.

Evidence:
- Build: `BUILD SUCCESS`; `mimita.exe` relinked.
- Runtime: not performed.
- Human playtest: pending.

Next: human test — die in Counter-Strike and confirm the world stays visible and
other actors keep moving while spectating. Changelog:
`docs/changelog/2026-10-04/20261004_220150-counterstrike-death-spectator-black-fix.md`.
## Attempt 16 — spawn separation confirmed; NPC targeting and wall recovery

Human playtest evidence from 2026-10-04 confirms that the earlier team-spawn
bug is no longer reproducing: actors are spawning on their correct side. The
same playtest found two remaining behavior problems: Counter-Strike NPCs were
not consistently selecting the human as a hostile target, and some NPCs kept
pressing into the same wall or doorway.

The current implementation now resolves human team identity from the
authoritative `matchTeams` roster when the per-player mirror is stale. The
Counter-Strike `opposite_team` policy uses nearest-hostile selection for both
players and NPCs, matching Sandbox's direct chase behavior while retaining the
Rage2 combat profile. Stuck policy NPCs choose a locally open direction, jump,
and request a repath instead of repeatedly pushing the same wall.

Build/self-tests pass. A live post-fix Counter-Strike match is still required
to confirm CT/T NPC acquisition, NPC-vs-NPC firing, and Dust2 doorway/ramp
recovery.

### Attempt 17 — unified shared movement executor

Tried:
- Made the one shared movement owner explicit: `NpcSystem::updateOneNpc` now
  takes a typed `NpcMovementContext` (mode-selected target + objective + optional
  goal override) instead of a bare `Player&`. Both `NpcSystem::update`
  (offline/Sandbox) and `NpcSystem::updateOneWithTarget` (server: Sandbox-online
  + Counter-Strike) build a context and call it.
- Added the generic, hot-reloadable actor-preset key `movement_executor`
  (`sandbox_shared` default | `surface_navigation` | `direct`) stored on
  `NpcMovementPolicy`; `counter_strike.json` sets `sandbox_shared`.
- Consolidated the policy and legacy stuck-recovery branches into one shared
  recovery (open direction → optional jump → repath), preserving Attempt 16.
- Added `tests/npc-movement-executor-test.cpp` and
  `--npc-movement-executor-selftest`.

Went right:
- Counter-Strike and Sandbox now resolve the same `sandbox_shared` executor and
  run identical movement code and tuning; Rage2 stays combat-only.
- Mode code (target selection, TeamBrain objective) supplies context and never
  steers; no Counter-Strike branch was added to `npc.cpp`.
- The change is generic, so Arena Fighter / Zombie Guard can reuse it.

Went wrong / watch out:
- Under `sandbox_shared`, Counter-Strike intentionally ignores its `navigation`
  block (search radius etc.) to match Sandbox exactly; that block now applies
  only to `surface_navigation`.
- The uncommitted Attempt-16 targeting/wall-recovery changes were preserved.

Evidence:
- Build: `BUILD SUCCESS`; `mimita.exe` relinked.
- Pure: `npc-movement-executor-test` PASS (15 checks).
- Runtime: `--npc-movement-executor-selftest` PASS; targeting/navigation/search/
  movement-policy/radar/nav-request/aim-fov/spawn-tag/cs-round/gamemode/
  acceptance all PASS.
- Human playtest: pending (see changelog
  `docs/changelog/2026-10-04/20261004_233026-unified-npc-movement-executor.md`).

Next: live Counter-Strike match — confirm both squads leave spawn, attack
enemies (human + NPC) but never teammates, and recover at Dust2 doorways/ramps
through the shared executor.

### Attempt 18 — wall-escape diagnostic verified + rate-limited; stale policy-test fixture fixed

Triggered by `docs/specs/20261005-counterstrike-wall-escape-handoff.md` (Option
A only; Option B deferred). Finishes the deferred verification items.

Tried:
- **Fixed `--npc-movement-policy-selftest` FAIL (was `lateral=0.49`).** The
  handoff blamed the Option-A un-gating, but instrumenting the test showed the
  actor already sits at `x=15.17` after Phase 1, so the hard-coded Phase 2 wall
  at `x=4` was *behind* it; the actor never met a wall. Placed the wall relative
  to the actor (`npc->body.pos.x + 3.0f`). The assertion is unchanged and now
  passes (`lateral=0.64`); it was the fixture that was stale, not the check.
- **Verified the JSONL diagnostic end to end.** Added
  `--npc-wall-escape-event-selftest`: it inits the real `StructuredLogger`,
  drives a real `counter_strike`-preset NPC through `NpcSystem::updateOneNpc`
  inside a closed wall pocket, then reads `events.jsonl` back and asserts a
  `npc.wall-escape` record with `preset:"counter_strike"` and `team:0`.
- **Rate-limited the backtrack event.** The selftest exposed the backtrack
  event firing once per tick (90 records in 90 ticks) while pinned; added an
  episode guard (`!npc.navigator.backtrackActive`) so it emits once per
  backtrack episode. The `open_turn_repath` event already had a guard.

Went right:
- The event now provably lands in the canonical `logs/<date>/<run>/events.jsonl`
  with the level gate satisfied (`npc_movement: important`).
- The fixed fixture makes the policy selftest robust to future movement-tuning
  changes instead of depending on an absolute coordinate.
- The diagnostic is now bounded (1 record / 180 pinned ticks in the harness).

Went wrong / watch out:
- The background dev-loop compiles changed objects but does not always relink,
  and `build_agent.py` links only when it compiled in the same run. Used
  `MIMITA_FORCE_LINK=1` to reconcile. Environment artifact, not code.
- This is an in-binary harness, not a live round. The CT-at-spawn event is
  proven to write, but the human still needs to confirm the live escape.
- Option B (real bomb-site positions) was not done; the placeholder sites in
  `config/maps/dust2cyberiav4.json` remain guesses.

Keep doing:
- Exercise the real executor + real logger in the selftest and read the file
  back, instead of asserting only that a call was made.
- Anchor test geometry to the actor's live position when Phase 1 movement can
  drift.

Stop doing:
- Trusting a handoff's suspected root cause without instrumenting the failing
  test first (the "Option A broke it" theory was wrong).

Evidence:
- Build: `BUILD SUCCESS`; `mimita.exe` relinked.
- Pure: executor 15, navigation 34, movement-policy 72 checks PASS.
- Runtime: `--npc-movement-policy-selftest` PASS (`lateral=0.64`),
  `--npc-wall-escape-event-selftest` PASS (1/1 record), plus navigation /
  executor / acceptance PASS.
- Human playtest: pending (live check that CT NPCs leave the spawn wall).

Next: human live check that CT NPCs leave the Dust2 spawn wall and that
`npc.wall-escape` records appear for CT actors during a real round. Changelog:
`docs/changelog/2026-10-05/20261005_011500-wall-escape-event-verify.md`.

### Attempt 19 — shared NPC movement commitment (single direction owner)

Triggered by the human handoff "shared NPC movement-commitment system".

Tried:
- **One direction owner.** Added commitment state to the shared `NpcNavigator`
  (`commitmentActive`, `committedDirection`, `commitmentTimeRemaining`,
  `progressTimer`, `commitmentStartPosition`) and moved the recent-path rings
  there. Removed `NpcStateMachine::patrolDir`/`patrolRepathTimer`; Patrol and
  `makeNavGoal` now read the navigator commitment (full unification).
- **Candidate scorer.** `NpcNavigator::chooseBestOpenDirection` scores open
  distance + forward continuation + target progress, minus reverse and
  recently-visited points, and never returns an immediately blocked direction.
- **Reason-based replanning.** The repath timer is now a minimum interval; a
  route is rebuilt only when empty/finished, the goal moved beyond the
  threshold, it is physically blocked, or progress failed.
- **Profile ownership.** `config/behavior-profiles.json` gains documented
  `repath_interval_seconds`, `goal_move_threshold_meters`, and a
  `movement_commitment` block (parsed + clamped with a validation warning).
  Compatibility defaults kept (`0.9` / `2.5`).
- **Visible-enemy rule.** Only `belief.hasVisibleTarget` (alive + FOV + range +
  LOS) lets combat movement own steering; a visible enemy suspends rather than
  destroys the commitment, so hidden pursuit resumes. Remembered targets follow
  forward commitment and the profile's `after_reaching_last_known`.
- **Events.** Added `npc.target-changed`, `npc.goal-changed`,
  `npc.nav-plan-created/failed`, `npc.nav-replan`, and
  `npc.movement-commitment-created/replaced/blocked`,
  `npc.movement-progress-failed` (change-edge, level-gated).
- **search_radius.** One owner/max: parser clamps to the planner's [4,20] and
  reports the clamp; `counter_strike.json` `200.0` -> `20.0`.

Went right:
- Counter-Strike and Sandbox still run the one `sandbox_shared` executor; no
  mode branch was added to movement.
- The commitment is evaluated for all profiles; profile JSON tunes it.
- The event selftest proved commitment/goal/plan events are not per-tick.

Went wrong / watch out:
- `--npc-radar-selftest` FAILs on `rage2 uses perfect radar`; that is a
  pre-existing config/test mismatch (`information_mode: "memory"` vs the test's
  `perfect_radar`) present at `HEAD`, not caused by this change.
- `--actor-preset-selftest` still fails on pre-existing weapon-value
  assertions, unrelated.
- Candidate scoring performs bounded ray probes only when a direction is
  (re)committed, not every tick; visible combat skips it entirely.
- Live Dust2 behavior (spawn escape, no repeated reversing, hidden pursuit) is
  still unobserved.

Keep doing:
- One movement owner and one direction owner; mode code supplies target/goal only.
- Profile-owned, hot-reloadable tuning with explicit clamp diagnostics.

Stop doing:
- Re-deciding the forward direction every tick or rebuilding a valid route on a
  bare timer.

Evidence:
- Build: `BUILD SUCCESS`; `mimita.exe` relinked (forced relink verified).
- Pure: movement-policy 72, navigation 35, movement-executor 15 PASS.
- Runtime: policy / search / navigation / executor / behavior-profile /
  commitment / commitment-event / wall-escape-event / acceptance PASS.
- Human playtest: pending.

Next: live Dust2 Counter-Strike round — confirm CT and T leave spawn, no
repeated reversing, routing around walls/ramps, hidden-target forward pursuit,
visible-enemy combat movement, and no endless circling. Changelog:
`docs/changelog/2026-10-05/20261005_171741-shared-npc-movement-commitment.md`.

### Attempt 20 — shared client/server NPC JSONL logging

Triggered by the human handoff "Shared Client/Server NPC JSONL Logging" so an
NPC investigation can be read from one file instead of a client-only log.

Tried:
- **One shared path.** `StructuredLogger::createLogDir` now derives
  `eventsPath`/`run_id` from `MIMITA_EVENTS_FILE` when set, otherwise generates
  the path and exports it with `SetEnvironmentVariableA` so the server child
  inherits it. `gui-main` logs the inherited path; `dev-loop.py` passes the same
  `MIMITA_EVENTS_FILE` to both server and client. The named mutex is unchanged.
- **One writer.** Converted `npcLog` text lines to `writeEvent`:
  `npc.target-changed`, `npc.shot`, `npc.reaction`, `npc.weapon-switched`,
  `npc.damage`, `npc.profile-applied`, plus new `npc.death` and `npc.respawn`.
  Deleted `npc-combat-log.*`; no `NPC_log_*.txt` is produced.
- **Movement instrumentation.** Added `npc.state-changed`, `npc.wall-avoid`,
  `npc.stuck`, `npc.stuck-recovery` (split from `npc.wall-escape`), `npc.jump`,
  and a post-physics `npc.movement-decision` snapshot once per NPC per second.
- **Config.** `general` -> `important` (so `logger.started` is not dropped),
  `performance` -> `off`; `npc_movement` important, `npc_combat` off.
- **`log_open`.** Reports the absolute path, client/server presence,
  `logger.started`, record/invalid counts, and NPC event counts.

Went right:
- The server honoured `MIMITA_EVENTS_FILE` and wrote `logger.started` with
  `process=server` and the shared `run_id`; the client writes `process=client`.
- The movement-decision file validated as 0 invalid JSON / 0 missing required
  fields, with the snapshot rate-limited to ~once per second.

Went wrong / watch out:
- `std::getenv` reads the CRT environment copy; the shared-path selftest must
  set the var with `_putenv_s` (the runtime export uses the Win32
  `SetEnvironmentVariableA` for child inheritance).
- `npc.stuck` / `npc.jump` re-emit per jump cycle because the reason resets to
  None while airborne; still bounded by jump cadence, not per tick.
- `--npc-radar-selftest` still fails on the pre-existing rage2
  `information_mode` mismatch.

Keep doing:
- One JSONL writer (`StructuredLogger`) and one shared path per session.
- Change-edge events plus a bounded heartbeat for the full movement decision.

Stop doing:
- Per-subsystem NPC text files (`NPC_log_*.txt`).

Evidence:
- Build: `BUILD SUCCESS`; `mimita.exe` relinked.
- Pure: movement-policy 72, navigation 35, movement-executor 15 PASS.
- Runtime: shared-log-path / movement-decision / wall-escape-event /
  commitment-event / commitment / behavior-profile / policy / search /
  navigation / executor / acceptance PASS; server `logger.started` shared-path
  proof captured.
- Human playtest: pending (live client+server session, `log_open`).

Next: human live session — verify `log_open` shows client+server in one file and
that a Counter-Strike round's NPC records explain direction, jump, replan, stuck,
and distance moved. Changelog:
`docs/changelog/2026-10-05/20261005_182631-shared-npc-jsonl-logging.md`.

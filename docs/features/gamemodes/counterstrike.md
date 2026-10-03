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

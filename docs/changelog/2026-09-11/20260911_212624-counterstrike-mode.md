// 2026-09-12T01:26:24Z
/* purpose
* record the JSON-driven Counter-Strike objective mode (teams, rounds, bomb)
* preserve exact source, config, build, and headless runtime evidence
* this file does NOT claim visual, HUD, or live-human acceptance
* this file does NOT replace the append-only regression record
*/

# Counter-Strike mode: teams, objective rounds, bomb, forced actor rules

## Session

- Branch: `8292026stash`
- HEAD commit: `afad20a` (working tree; no commit created this session)
- Timestamp (UTC): `2026-09-12T01:26:24Z`
- Display timezone: America/New_York
- Display time: `2026-09-11 21:26:24 EDT`
- Pre-existing changes: the working tree already contained many unrelated
  concurrent edits from other sessions (grenade weapons, NPC mind/behavior,
  `src/combat/area-effect.*`, `src/npc/npc-mind.*`, `config/debuglogger.json`
  untouched by me, etc.). Those were preserved and are not claimed here.

## What was requested

Make the `counterstrike` mode actually work: two teams (T/CT), one bomb carrier
that drops the bomb on death, plant/defuse bomb sites defined per map, dead
players spectate (not respawn) for the round, first to 8 rounds, forced FOV 70,
forced `aim_mode` physical, one slow/realistic movement preset for every actor
(players and NPCs), mode-driven `healthbar.json` override, and new AK/sniper
weapons with per-team loadouts. All six phases were implemented together.

## Root cause found

`counterstrike` was not wired to any lifecycle. `serverGamemodeTick` routed only
`ffa`, `tdm`, `win_condition == last_team_standing`, and bomb-feature modes, so
`counterstrike` fell through to the legacy 1v1 duel machine
(`server-gamemode.cpp`, `serverGamemodeTick`). `counterstrike.json` was a stub
and `dust2cyberiav3` was not in its map pool.

## Changed files

### New

- `src/gamemode/map-config.h/.cpp`: per-map JSON registry for team spawns,
  bomb sites, and bomb timings. Loader = `MapConfigRegistry::get(mapId)` reading
  `config/maps/<mapId>.json`; cached, hot-reloadable via `pollReload()`.
- `config/maps/dust2cyberiav3.json`: provisional T/CT spawns and one bomb site
  derived from the map's single `spawnpoint.011` glb node.

### Gamemode schema

- `src/gamemode/gamemode.h/.cpp`: `Gamemode` gains `aimMode`, `movementPreset`,
  `GamemodeHealthbarOverride healthbar`, objective-bomb timings, and
  `useModeMaps`. `GamemodeFeatures` gains `objectiveBomb`. `GamemodeRegistry`
  gains `revision()` (incremented on every load/reload) for live rule re-apply.

### Forced actor rules + hot reload

- `src/config/gameplay-config.h/.cpp`: added `gameplayAimModeFromString(...)`
  and a process-local aim-mode override (`setAimModeOverride`,
  `clearAimModeOverride`, `hasAimModeOverride`); `aimMode()` returns the
  override while active.
- `src/gui/hud/healthbar-config.h/.cpp`: match override layer
  (`setMatchOverride`, `clearMatchOverride`); `data()` returns the override
  while active, preserving the player's colors/timings.
- `src/network/packets.h`: `DuelStatePacket` carries `aimMode[16]` + compact
  healthbar override; `BombTagStatePacket` carries `objectiveState`,
  `plantPercent`, `defusePercent`; added `BombObjectiveState`.
- `src/network/community-match-client.h/.cpp`: replicates the above, applies
  aim/healthbar overrides, clears them on `reset()`, reads objective-bomb state,
  and adds `localActorSpectating()`.
- `src/network/server-gamemode.h/.cpp`: mode overrides copied at match start and
  re-applied live when `GamemodeRegistry::revision()` changes (safe tick
  boundary; phase/scores/spawns preserved).

### Round lifecycle + objective bomb

- `src/network/server-gamemode.h/.cpp`: `objectiveRounds`, `roundWins[2]`,
  `roundNumber`, `roundOver`, `roundEndReason`, per-team spawn arrays, and
  objective-bomb state. New helpers `bombSiteContains`, `objectiveTeamAlive`,
  `objectiveActorPos`, `objectivePickCarrier`, `updateObjectiveBomb`,
  `checkObjectiveRoundEnd`, `beginObjectiveRound`.
  - Carrier dies -> bomb drops at the death position (kill path hook).
  - Carrier in a bomb site -> plants after `bomb_plant_seconds`; timer starts.
  - CT in radius -> defuses after `bomb_defuse_seconds`; timer expiry -> explosion
    damage in radius.
  - Round winner recorded; first to `goal_value` ends the match; RESULTS between
    rounds starts the next round directly; match over -> intermission and a
    fresh match.
  - `emitGamemodeMatchPersistence` reports round wins for objective modes.
  - `assignMatchParticipants` keeps dead NPCs on the roster in round modes so
    they revive instead of being dropped.
  - `gamemodeSpawnPoint(d, team)` picks per-team spawns with anchor fallback.
  - `useModeMaps` opts a mode into its own `maps` pool (Counter-Strike only).
- `src/game/gamemode-manager.cpp`: `objective_bomb` feature renders the
  dropped/planted C4 sphere and world timer from replicated state.

### Spectator freecam

- `src/terminal/terminal-state.h`, `src/main-systems.cpp`: new
  `gSpectatorFreecamLocked` global.
- `src/engine/engine-tick-camera.cpp`: while `localActorSpectating()` is true,
  force `FREECAM_ENABLED` on and lock it; release when the round resets.
- `src/camera/camera-commands.cpp`: `freecam 0` is refused while locked.

### Configs

- `config/gamemodes/counterstrike.json`: full rules (`goal_value 8`,
  `respawn_seconds 0`, `countdown_seconds 3`, `camera_fov 70`,
  `aim_mode physical`, `movement_preset counterstrike`, `win_condition
  objective_rounds`, `features.objective_bomb`, `use_mode_maps true`,
  `roles {terrorist:10, counter_terrorist:10}`, `maps ["dust2cyberiav3"]`,
  bomb timings, and a healthbar override block).
- `config/roles.json`: added `terrorist` (team 0, `weapon_set cs_t`,
  `starting_weapon ak`) and `counter_terrorist` (team 1, `weapon_set cs_ct`,
  `starting_weapon sniper`); both leave `movement_preset` empty so the mode
  preset applies.
- `config/weaponsets.json`: added role-only sets 9 `cs_t` (ak, spyknife) and 10
  `cs_ct` (sniper, revolver, spyknife). A concurrently added set 8 `Grenades`
  was preserved.
- `config/weapons.json`: added `ak` (slot 16) and `sniper` (slot 17) using
  placeholder existing models/sounds, per the agreed first pass. Slots 16/17
  avoid the concurrently added grenades at 13-15.

## Exact behavior notes

- Forced movement is data-driven: `serverResolveActorSpawnProfile` seeds
  `movementPreset` from the gamemode and a role's own preset overrides it.
- Forced aim mode is applied on the server (`computeAim` reads the effective
  `GameplayConfig`) and replicated so clients agree.
- Rounds keep participants stable; dead actors pass through the existing
  one-life `ActorState::Spectating` path (`respawn_seconds: 0`), so no new
  no-fire/no-move branches were added.

## Spec disagreement (recorded, needs decision)

`docs/specs/gamemodes/gamemodes.md` says every mode uses
`config/gamemode-good-maps.json` for automatic selection. To honor the explicit
request that Counter-Strike use `dust2cyberiav3`, I added an opt-in
`use_mode_maps` flag instead of changing all modes. Existing modes keep the
shared pool. The spec should be updated to document `use_mode_maps`.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md`, `docs/specs/gamemodes/meta.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW; one
  intentional spec/code disagreement recorded above.
- `docs/skills/logging-checker-v1.md`: PASS; new `[BOMB]`, `[CS ROUND]`,
  `[MAP CONFIG]`, `[GAMEMODE] live rules reloaded` diagnostics are event/change
  based and Duel-category (off by default).

## Validation

- JSON parse check: `weapons.json`, `weaponsets.json`, `roles.json`,
  `gamemodes/counterstrike.json`, `maps/dust2cyberiav3.json` all parse.
- Build: `python build_agent.py` => `Status: SUCCESS` (return code 0), three
  times (after the main implementation, and after each runtime fix).
- Headless runtime (dedicated `mimita.exe --server --direct --mode
  counterstrike --gamemode counterstrike --map dust2cyberiav3 --npcs 8`), Duel
  log temporarily at verbose:
  - `[GAMEMODE] Loaded counterstrike.json ... goal=8 ... fov=70 ... win=objective_rounds`
  - `[MATCH RULES] mode=counterstrike respawn=0.00s respawns=0 kill_heals=0 win=objective_rounds roles=2`
  - `[MAP CONFIG] loaded map=dust2cyberiav3 T=3 CT=3 sites=1`
  - `[DUEL SERVER] anchor=(-604.9 28.3 2366.2) spawns=1 teamSpawnsT=3 teamSpawnsCT=3`
  - `[MATCH ACTORS] assigned=8 mode=counterstrike roleCounts=2`
  - `[ROLE MOVEMENT] resolved preset 'counterstrike' from config/movement\movement-cs.json`
  - `[ROLE SPAWN] ... role=terrorist ... weaponSet=9 ...` and
    `role=counter_terrorist ... weaponSet=10 ...`
  - `[CS ROUND] starting round=1 carrier=1001 score=0-0`
  - `[CS ROUND] round=1 winnerTeam=1 reason=elimination score=0-1 matchOver=0`
  - Repeated rounds to `score=0-8 matchOver=1` then
    `[CS ROUND] match over; intermission 15s` and a fresh `round=1 score=0-0`.
- `config/debuglogger.json` was temporarily set to Duel verbose for capture and
  restored exactly (`git diff` clean).

## Human review still needed

- Visual: two GUI clients on `dust2cyberiav3` confirming forced FOV 70, physical
  aim, the slow movement feel, healthbar override, T/CT spawns, HUD, and the
  spectator freecam lock.
- Bomb plant/defuse with a human carrier (headless NPCs never plant). The
  headless run only exercised carrier assignment, drop, and elimination wins.
- The `config/maps/dust2cyberiav3.json` coordinates are provisional offsets
  around the map's single spawn node. They need real map data (or authored
  `spawn T`/`spawn CT` glb nodes, which the loader already prefers) and real
  bombsite volumes.
- Role caps are 10 per team; teamless overflow when more than 20 participants
  needs a decision.

## Follow-up in the same session (HUD + score replication)

After the initial build was verified, these visible gaps were closed:

- `config/gui/gamemode-meta-gui.json`: added a `counterstrike` layout section
  (`modeTitle`, `scoreText`, `matchTime`, `intermissionText`, `countdownText`,
  `bombAlert`, `bombPlanted`). Without it the generic community HUD had no
  layout for the mode and rendered nothing.
- `src/network/server-gamemode.cpp` (`broadcastDuelState`): objective-round
  modes now publish `roundWins[0/1]` through `redTeamKills/blueTeamKills`, so the
  generic `{red_score}`/`{blue_score}` HUD shows the current round score.
- `src/game/gamemode-manager.h/.cpp`: added `renderObjectiveBombHud()` under the
  `objective_bomb` feature, showing carrier instruction, plant progress percent,
  dropped-bomb notice, planted timer, and defuse progress percent.
- `config/onlinemodes.json`: Counter Strike menu `score_limit` updated 10 -> 8
  and the description corrected (no ragdolls, first to 8).

Build re-verified after the follow-up: `python build_agent.py` => `Status:
SUCCESS`. The HUD is client-side presentation and is not proven by the headless
server; it remains part of the human review list above.

## Smallest logical next phase

Author `spawn T`/`spawn CT` nodes and bombsite volumes in Blender for
`dust2cyberiav3`, then replace the provisional map JSON; add a manual plant key
and defuse-kit decision if required.

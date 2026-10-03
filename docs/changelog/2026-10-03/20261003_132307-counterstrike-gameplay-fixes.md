# Counter-Strike gameplay fixes + config handoff

Date: 2026-10-03
EST timestamp: 2026-10-03 13:23:07 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and pure-rule evidence are proven (including a new spawn-tag
selftest). Live gameplay/visual acceptance is NOT performed and remains
required.

## Scope

Human playtest feedback after the 10-checkpoint implementation:
team spawn separation, same-team hostility, death/spectator, intermission
warmup, tab team display, avatars, and map = dust2cyberiav4. One changelog for
the whole pass.

## Files changed

### Team-tagged spawn separation
- `src/network/server.h`: `ServerSpawnPoint` gained `std::string tag`.
- `src/network/server-world.cpp`: `loadHeadlessWorld` stores the GLB node name
  in `sp.tag`.
- `src/network/server-gamemode.h`: `ServerGamemodeState` gained
  `teamSpawnPoints[2]`, `teamSpawnsResolved`, and `warmup`.
- `src/network/server-gamemode.cpp`: added `spawnTagTeamIndex`; `assignGamemodeSpawns`
  groups tagged CT/T spawns; added `gamemodeSpawnPoint(d, team)` and kept the
  1-arg wrapper; `buildObjectiveRoster` and `resetGamemodeActorsAtMapSpawn` use
  the team spawn. Added `serverSpawnTagSelfTest`.

### Same-team hostility
- `src/network/server-npcs.cpp`: `chooseNearestPlayer` now gates on
  `actorsAreHostile`; team round modes allow NPC targets
  (`allowNpcTargets = damageOtherNpcs || objectiveRounds`); player-priority mode
  falls back to hostile NPCs so both squads fight.

### Death -> spectator / one-life
- `config/gamemodes/counterstrike.json`: explicit `respawn_seconds: 0`,
  `rounds.freeze_seconds: 5`, `maps: ["dust2cyberiav4"]`.
- `src/network/community-match-client.h/.cpp`: added `localActorState`,
  `localTeam`, `teamForActor`.
- `src/engine/engine-tick-camera.cpp`: forces gameplay freecam while the local
  actor is Dead/Respawning/Spectating during a round; restores after.

### Intermission warmup
- `src/network/server-gamemode.cpp`: round INTERMISSION now builds the roster at
  intermission start, spawns the human at their team spawn, assigns the
  objective, and sets `warmup`. `buildObjectiveRoster` clears prior roster NPCs
  (ids >= 100000) before rebuilding. `serverPlayerRespawnsEnabled`/
  `serverMatchRespawnsEnabled` return true during warmup (infinite lives).

### Freeze after GO
- `src/network/server-gamemode.cpp`: on GO -> ACTIVE, NPC `wakeupTimer` is set
  to `freezeSeconds` so NPCs hold after GO.

### Tab leaderboard team columns
- `src/engine/engine-tick-ui-overlays.cpp`: header and rows now show a short
  team tag (CT/T) from the replicated roster; local row marked "(you)".
- `config/gui/tab-leaderboard.json`: unchanged structure (teams rendered
  in-code); header default text updated in code.

### Avatars
- `config/roles.json`: T = `jason`, CT = `abusiveboy`.

### Map = v4
- `config/gamemodes/counterstrike.json`: `maps: ["dust2cyberiav4"]`.
- `config/maps/dust2cyberiav4.json`: new (copy of the v3 objective config).
- `src/network/server.cpp`: `gamemodeDefaultMap()` makes a mode's `maps[0]` the
  authoritative default map in both dedicated and listen start paths (explicit
  GUI/`changemap` still wins).

### Config handoff
- `docs/features/gamemodes/counterstrike.md`: added a "Config map — what to edit
  to tweak the game" table and an Attempt 11 entry.

### Tests
- `src/game/game-cli.cpp`: added `--spawn-tag-selftest` and included it in
  `--counterstrike-acceptance-selftest`.

## Validation

Build: `BUILD SUCCESS`, no warnings.

Runtime:

```text
[ACCEPTANCE] round+weapons: PASS
[ACCEPTANCE] spawn-tags   : PASS
[ACCEPTANCE] objective .. npc-nav-request: PASS
[ACCEPTANCE] PASS
```

All 13 individual selftests PASS.

## Known limitations / follow-ups

- Per-team **human** avatar is still preset-driven on the client (NPC avatars
  are per-team).
- Freeze time holds NPCs, not the human.
- `config/npc-difficulty.json` still declares `targetMode: "player"` /
  `damageOtherNpcs: false`; the team gate makes this safe, but the config intent
  should be cleaned up.
- Bomb may be carried during warmup (harmless).
- Bomb sites in `config/maps/dust2cyberiav4.json` are unauthored; verify with
  `site_debug` before expecting planting.

## Human review still needed

- Confirm CT/T spawn apart using `spawnpoint.CT`/`spawnpoint.T` in v4.
- Confirm same-team NPCs never shoot the player but enemy NPCs still do.
- Confirm death -> freecam until round end, then revive at the team spawn.
- Confirm intermission shows moving NPCs and infinite lives.
- Confirm TAB shows your team (CT/T) and the roster.
- Confirm T=jason, CT=abusiveboy avatars.
- Confirm the map is `dust2cyberiav4`.

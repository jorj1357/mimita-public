# Map entity run runtime (checkpoint / boss_trigger / spawnpoint) + editor hardening

- EST timestamp: 2026-10-09 09:25:00 -04:00
- UTC timestamp: 2026-10-09T13:25:00Z
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Why

The Zombie Tower spec (`docs/specs/gamemodes/zombie-tower.md`) requires authored
map entities that actually affect the run: checkpoints with run attempts
(§20-24, §208-209), boss triggers (§115-120, §183), and player spawning from
authored `spawnpoint` entities (§19, §139). The Map Entity Editor v0 already
authored and saved the six entity types, but only `monster_zone` had a runtime
consumer. This slice adds the checkpoint, boss_trigger, and spawnpoint
consumers, and hardens the editor (look-to-place on geometry, full property
editing, save round-trip safety). A follow-up requirement is that editing
`config/maps/<map>.json` by hand must update the running game live; the client
previously loaded the map config once and never polled it, so the client now
polls the active map file like the server does.

## Changes made

- `src/gamemode/map-config.h/.cpp`
  - New shared owner `mapEntityContainsPoint(entity, point)` for authored
    trigger geometry (radius sphere or size box), so the volume has one owner.
  - `save()` now starts from the existing on-disk JSON and rewrites only
    `bomb`, `bomb_sites`, and `entities`, preserving unknown top-level keys.
  - New `mapEntityConfigSelfTest()` covering trigger geometry, save
    round-trip of an unknown top-level key, and live reload of an external
    JSON edit through `pollReload()`.
- `src/engine/engine-tick-setup.cpp`
  - The client now calls `MapConfigRegistry::pollReload()` every hot-reload
    polling cycle for the active map, instead of only loading the file when the
    map id changes. Editing `config/maps/<map>.json` now updates the live client
    registry (debug markers, authored entity state) without a restart, matching
    the already-existing server poll.
- `src/terminal/map-entity-commands.cpp`
  - `placement()` is now collision-aware: a bounded world raycast from the
    camera (`sweptSphereTraverseGridCells`) places the entity on the first
    surface hit, falling back to camera-forward when nothing is hit.
  - `entity_set` now edits `pickupId`, `bossId`, `tag`, `damageType`,
    `damage`, `damageIntervalTicks`, `checkpointRequirement`, and `visible`,
    so every entity type is settable in-engine. Help text updated.
- `src/gamemode/gamemode.h/.cpp`
  - `Gamemode::runAttempts` (`run_attempts`, default 3), parsed from gamemode
    JSON.
- `src/network/server-gamemode.h`
  - `ServerGamemodeState` run state: attempts total/remaining, current
    checkpoint id/position, `hasCheckpointSpawn`, per-run activation latches
    for checkpoints and boss triggers, boss encounter/lock state.
- `src/network/server-gamemode.cpp`
  - `mapEntityRuntimeTick()` runs every `DUEL_PHASE_ACTIVE` frame:
    - `checkpoint`: latches once per run when a live player enters, stores the
      checkpoint respawn, emits `zombie_tower.checkpoint-reached`.
    - `boss_trigger`: latches, sets the progression lock, spawns the configured
      `bossId` actor through the shared NPC path, emits `boss.triggered` /
      `boss.spawned`, and clears the lock with `boss.died` when the boss actor
      dies.
  - `zombieTowerPartyWipeTick()` (persistent-NPC modes): when all active humans
    are dead, spends a run attempt and respawns the party at the latest
    checkpoint (`zombie_tower.attempt-started`); when no attempts remain, resets
    the run and restarts (`zombie_tower.run-failed`).
  - `gamemodeSpawnPoint()` prefers authored `spawnpoint` entities, filtered by
    the team's `spawn_group` tag; `gamemodeSpawnPointForActor()` respawns players
    at the current checkpoint during a run.
  - Run state initialized from the gamemode rules at mode start.
- `src/game/game-cli.cpp`
  - New `--map-entity-selftest` CLI entry point.

## Behaviour boundary

- `damage_volume` and `pickup` remain authoring-only in this slice (deferred).
- The boss trigger spawns an authoritative actor and locks progression; it does
  not yet provide a dedicated boss health bar/music presentation.

## Validation evidence

- BUILD: `python build_agent.py` with `MIMITA_EXE_NAME=mimita-20261010-mapentity-live-final.exe`
  returned 0; final named executable linked (148,155,118 bytes).
- BUILD (content proof): the final executable contains the new self-test string
  `"pollReload did not detect an external JSON edit"` (binary search, 1 match),
  so the live-reload assertion is compiled in.
- COMPONENT: `mimita-20261010-mapentity-live-final.exe --map-entity-selftest`
  printed `[MAP ENTITY SELFTEST] PASS` (exit 0): trigger sphere/box geometry,
  save round-trip preserving an unknown top-level key, and `pollReload()`
  detecting an external edit and applying the moved checkpoint position.
- COMPONENT (no regression): `--map-config-selftest` printed `PASS`
  (`sites_checked=2`).
- RUNTIME: `mimita-20261010-mapentity-live-final.exe --versioninfo` completed and
  emitted `EVENTS_JSONL_PATH=logs/10-10-2026/20261010_175319/events.jsonl`.
  That journal contains only `run.started`, `logger.started`,
  `versioninfo.executed`, and `logger.stopped`; no live match was driven, so no
  checkpoint/boss/spawn events were exercised here.
- HUMAN ACCEPTANCE: still required. Launch the final executable, load Zombie
  Tower 4, run `entity_visibility on`, edit `config/maps/zombietower4.json`
  (for example move the `test1` spawnpoint position), save it, and confirm the
  on-screen marker moves within ~1/6 second without a restart. Also confirm a
  live `zombie_tower.checkpoint-reached`, respawn-at-checkpoint after a party
  wipe while attempts remain, `zombie_tower.run-failed` on exhaustion,
  `boss.triggered`/`boss.spawned` on entering a boss trigger, and authored
  `spawnpoint` selection.

## Spec and doc TODOs observed (not edited)

- `docs/specs/gamemodes/zombie-tower.md:2862`, `:3381` (music/MIDI),
  `:3774` (NPC hostility pointer), `:4015` (drops/monetization), `:4104`
  (replay), `:1172`, `:352`, `:5293` (narrative).

## Repository hygiene

Pre-existing working-tree modifications in configuration and other source files
were preserved and are not claimed by this change. No regression record was
created because no human-confirmed regression was established.

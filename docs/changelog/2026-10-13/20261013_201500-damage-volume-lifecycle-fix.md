# Fix: damage volume stopped damaging (persistent lifecycle gated on spawn cap)

- EST timestamp: 2026-10-13 16:15:00 -04:00
- UTC timestamp: 2026-10-13T20:15:00Z
- Branch: `2026-10-10-Z-Tower`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Routed documents read

`AGENTS.md`, `docs/ROUTER.md`, `docs/specs/debug-logging/AI-fix-live.md`,
`docs/specs/gamemodes/zombie-tower.md`, `docs/specs/entity-editor/entity-editor.md`,
`docs/workflows/runtime-scenario-validation.md`.

## Observable failure

A player enters the authored `damage_volume` in Zombie Tower but health does not
decrease; the expected `damage-volume.damage` event is absent from the fresh
shared journal. Runtime identity: `--mode zombie_tower --gamemode zombie_tower
--map zombietower4`.

## Root cause (first divergence)

`config/gamemodes/zombie_tower.json` had `npc_spawn.max_count` set to `0`
(commit `7a1ab10f`, "not workign damage volume and menu not good"). The server
derived the lifecycle flag from the cap:

```cpp
d.persistentNpcSpawns = gm.npcSpawnPolicy == "persistent" && gm.npcSpawnMax > 0;
```

so `persistentNpcSpawns` became false. The single shared lifecycle branch that
advances `WAITING -> COUNTDOWN -> GO -> ACTIVE` (`serverGamemodeTick`,
`server-gamemode.cpp:4556`) is gated on `npcWaves || persistentNpcSpawns ||
ffa || tdm || last_team_standing || last_actor_alive`. Zombie Tower matches none
of those when `persistentNpcSpawns` is false, so the phase never reached
`ACTIVE`, and `mapEntityRuntimeTick` — which owns `damage_volume`, `checkpoint`,
and `boss_trigger`, and is only called from `case DUEL_PHASE_ACTIVE:`
(`:4672`) — never ran. The same applies to sandbox/`mapOnly`, which returns
before the shared lifecycle.

Evidence: journal `logs/2026-10-13/20261013_152157/events.jsonl` (before the
commit) has `damage-volume.damage` x116; post-commit runs `_193457`, `_195053`,
`_195456` have map entities loaded but `damage-volume.damage = 0`.

## Changes made (owner: gamemode runtime)

`src/network/server-gamemode.cpp`:
1. `serverCommunityStartMatch` (`:679`, `:689`): `persistentNpcSpawns` is now
   enabled by `policy == "persistent"` alone; `npcSpawnMax = max(0, gm.npcSpawnMax)`.
   A cap of 0 means "run the mode, spawn no NPCs" instead of disabling the mode.
   `spawnPersistentNpcBatchIfDue` already no-ops when `npcSpawnMax <= 0`.
2. Hot-reload block (`:3910-3913`): same decoupling, so editing the JSON live does
   not silently disable the lifecycle.
3. Sandbox/`mapOnly` (`:4178`): `mapEntityRuntimeTick` is now called every tick in
   the `mapOnly` branch, so authored entities (damage volumes, checkpoints, boss
   triggers) tick in sandbox play too, not only the managed match lifecycle.
4. Owner-level journal event `gamemode.npc-spawn.policy` emitted at mode start,
   carrying `policy`, `persistent_npc_spawns`, `npc_spawn_max`, `npc_waves`, so the
   ACTIVE-phase gate is traceable in `events.jsonl` without guessing.
5. `serverSpawnTagSelfTest` (`:5020`): the zombie_tower assertion now requires
   `npcSpawnMax >= 1` (the cap is no longer `50`).

Pre-existing user-owned edits to `config/gamemodes/zombie_tower.json`
(`max_count` restored to `1`) and `config/maps/zombietower4.json` were preserved
and are not claimed by this change.

## Validation evidence

- BUILD: `MIMITA_EXE_NAME=mimita-20261013-damagevolume-fix.exe python build.py
  build-only` -> `BUILD SUCCESS`, `server-gamemode.cpp` recompiled and linked.
- COMPONENT: `mimita-20261013-damagevolume-fix.exe --spawn-tag-selftest` ->
  `PASS` (proves the new object is linked; the old `== 50` assertion would fail
  with the current `max_count: 1`).
- RUNTIME (journal, server): `--server --mode zombie_tower --gamemode
  zombie_tower --map zombietower4 --no-npcs` started cleanly and wrote journal
  `logs/10-13-2026/20261013_161111/events.jsonl` containing
  `gamemode.npc-spawn.policy {policy:"persistent", persistent_npc_spawns:true,
  npc_spawn_max:1, npc_waves:false}` and `map-entity.loaded {entity_count:2,
  result:success}`. This proves the lifecycle flag is now independent of the cap.
- RUNTIME (sandbox, Option C): `--server --map zombietower4 --no-npcs` ran ~280
  ticks through the `mapOnly` branch with `map-entity.loaded` and no errors/crash.
- HUMAN ACCEPTANCE: still required. Through `python devscripts/dev-loop.py`
  (mode 8/10, Zombie Tower on `zombietower4`), teleport into
  `damage_volume_live_test` (`-1263.48, -820.26, 379.64`) and confirm the fresh
  journal shows `damage-volume.damage {players_hit:1}` and `server.damage.applied`
  every ~5 ticks, with HP dropping in-game.

## Limits

- The live player-damage confirmation needs the dev-loop client; it was not
  driven in this shell (the dev loop owns the runtime).
- `packets.h` `PROTOCOL_VERSION` is currently `41` in this working tree (an
  earlier banner-protocol change is not present); no wire change was made here.
- Some earlier authored-entity work (`MapEntity::shape` box handling,
  `monsterRole` wiring, the checkpoint banner) is not present in the current
  working tree; this pass only restored damage-volume runtime reachability and
  sandbox ticking.

## Repository hygiene

Pre-existing working-tree changes (`devscripts/dev-loop.py`, dev-loop/docs, gui,
config, other source) were preserved and are not claimed. No regression record
was created because no human-confirmed regression was established beyond the
reported symptom, which is recorded here.

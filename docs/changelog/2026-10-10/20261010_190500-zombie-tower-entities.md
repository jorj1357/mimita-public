# Zombie Tower: damage volumes, monster roles, basic zombie, checkpoint banner

- EST timestamp: 2026-10-10 19:05:00 -04:00
- UTC timestamp: 2026-10-10T23:05:00Z
- Branch: `2026-10-10-Z-Tower`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Why

Close the gameplay loop for the authored map entities added earlier so an editor
session produces playable behavior:

1. `damage_volume` entities must damage actors that walk into them, with a
   selectable box or sphere shape (they only authored/visualized before).
2. `monster_zone` should spawn a chosen monster role (it always spawned the
   global-default NPC before).
3. A basic data-driven **zombie** monster: walk to the nearest player, slash at
   contact range, with a placeholder hook for a future voice line / effect.
4. `checkpoint` must be monotonic (no going from checkpoint 3 back to 2) and show
   a fading "CHECKPOINT N REACHED" banner reusing the gamemode center HUD.

## Changes made

### A. Damage volume runtime + shape
- `src/gamemode/map-config.h/.cpp`: `MapEntity::shape` ("sphere"/"box"); parser,
  saver, and `mapEntityContainsPoint` now honor the explicit shape (empty keeps
  the legacy inference). `monsterRole` and `checkpointIndex` fields added too.
- `src/debug/debug-visuals-scene.cpp`: entity markers draw only the matching
  shape (a box **or** a sphere), not both.
- `src/network/server.h` + `src/network/packets.h` + `src/network/server-damage.cpp`:
  new `ServerDamageSource::Environment` and `DAMAGE_CONFIRMED_ENVIRONMENT`.
- `src/network/server-gamemode.h/.cpp`: `damageVolumeNextTick` cadence state and a
  `damage_volume` branch in `mapEntityRuntimeTick`. Enabled volumes with
  `damage > 0` apply `applyServerDamage(..., Environment)` to players and direct
  damage to NPCs every `damageIntervalTicks`, emitting `damage-volume.damage`.

### B. Monster zone → role
- `addNpcWaveParticipant` takes a role id; `spawnPersistentNpcBatchIfDue` passes
  `zone.monsterRole` (or a `monsterPool` that names a known role). The shared
  `serverResolveActorSpawnProfile` + `adoptNewServerNpcs` apply the role's
  health / movement / behavior / loadout.

### C. Basic zombie
- `config/weapons.json`: `zombie_claw` (SpyKnife physical contact, damage 12,
  `npcContact` gate, `serverContactRadius`/`contactForwardOffset`/`contactCenterZ`/
  `damageTickInterval`).
- `src/combat/weapon-data.cpp`: built-in `createZombieClawDefinition()` (JSON can
  only override built-ins, so a stub is required) registered in the builtin set.
- `config/weaponsets.json`: role set `zombie` (`zombie_claw`, `nothing`).
- `config/roles.json`: role `zombie` (team 1, health 100, `source` movement,
  `zombie` set, `zombie_claw`, `zombie` behavior).
- `config/behavior-profiles.json`: profile `zombie` (aggression 1.0,
  `preferred_range` 1.0, persistent pursuit).
- `src/npc/npc.h`: per-victim `contactLastTick`.
- `src/network/server-npcs.cpp`: NPC contact tick in `simulateSharedNpcs` reusing
  `WeaponExecution::testPhysicalContact` (the same geometry spyknife uses),
  applying damage per `damageTickInterval` while a hostile player intersects the
  claw, with the confirmed-damage/kill path. Emits `npc.attack` as the
  placeholder hook for the future voice line / slash animation.

### D. Checkpoint ordering + banner
- `mapEntityRuntimeTick`: a checkpoint activates only when its `checkpointIndex`
  exceeds the current index; on activation it bumps a monotonic `bannerSerial`
  and sets `bannerNumber`.
- `src/network/packets.h`: `DuelStatePacket::bannerSerial`/`bannerNumber`;
  `PROTOCOL_VERSION` 41 -> 42. `broadcastDuelState` copies them.
- `src/network/community-match-client.h/.cpp`: store + expose the banner.
- `config/gui/gamemode-meta-gui.json`: new `zombie_tower` section
  (intermission, countdown/GO, `checkpointText`).
- `src/engine/engine-tick-ui-overlays.cpp`: `drawCentered` takes an alpha; the
  checkpoint banner restarts a local 5 s (300 tick) fade on serial change,
  alpha 0.5 -> 0.0, matching the "you died to X" popup fade.

### Death-cause attribution
- `src/network/multiplayer-projectiles.cpp`: on a confirmed local death, the
  "you died to X" popup now says `the environment` (environment source) or
  `a monster` (NPC physical contact) instead of `unknown`.

### Data
- `config/maps/zombietower4.json`: `monster_zone_2` gets `monsterRole: "zombie"`;
  sample `checkpoint_1` (index 1) and `lava_1` (`damage_volume`, sphere, lava).

## Validation evidence

- BUILD: `MIMITA_EXE_NAME=mimita-20261010-zombie-claw-v2.exe` built (final exe
  linked). Binary-string proof the new code is linked: `contact_claw_hit`,
  `damage-volume.damage`, `zombie_tower.checkpoint-reached`, `a monster` each
  present once.
- COMPONENT: `--map-entity-selftest` PASS (trigger geometry + save round-trip +
  live reload); `--map-config-selftest` PASS.
- RUNTIME (server): `mimita-20261010-zombie-claw-v2.exe --server --map zombietower4
  --gamemode zombie_tower` started cleanly: `[WEAPON] Registered: zombie_claw
  (slot 20)`, `protocol version=42`, and journal
  `logs/10-10-2026/20261010_190256/events-000001.jsonl` contains
  `map-entity.loaded {map:zombietower4, entity_count:3, result:success}`.
  `--versioninfo` emitted
  `logs/10-10-2026/20261010_190323/events-000001.jsonl`.
- HUMAN ACCEPTANCE: still required. In-game confirm: (1) walking into `lava_1`
  drains HP every ~30 ticks and shows a purple sphere (not a box); (2) entering
  `monster_zone_2` spawns a zombie that walks in and claws for periodic contact
  damage (`npc.attack` in the journal); (3) entering `checkpoint_1` shows the
  fading "CHECKPOINT 1 REACHED" banner and does not re-fire from a lower index;
  (4) the death popup reads "the environment" / "a monster".

## Limits and notes

- The claw swing is server-authoritative contact damage. Its client-side swing
  sound/effect and monster animation are the deferred hook marked in
  `server-npcs.cpp` (`npc.attack` placeholder).
- `damage_volume` uses the shared `applyServerDamage` path; environment kills
  are attributed to the popup class, not yet to a per-hazard text (e.g. "lava").
- A concurrent `build_agent.py` (another session) held the shared build lock for
  most of this session; `build.py build-only` was used and the final named exe
  was verified by binary search.

## Repository hygiene

Pre-existing working-tree changes (config and other source files not listed
above) were preserved and are not claimed by this change. No regression record
was created because no human-confirmed regression was established.

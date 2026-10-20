# Zombie Tower first playable slice

- Status: `IMPLEMENTED_FIRST_SLICE_PASS_WITH_CONTENT_LIMITS`.
- Timestamp: 2026-10-20T00:50:00Z (2026-10-19 20:50 EDT).
- Branch: current working tree (no commit made).
- Request: implement the agreed Zombie Tower first-playable slice in the existing
  mode/actor/map owners, then validate through the dev loop.
- Skills: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`.
- Build: dev-loop build `2678`, status `SUCCESS`.
- Runtime journal: `logs/2026-10-20/20261020_004352/events.jsonl`
  (earlier successful validation runs: `logs/2026-10-20/20261020_000454/events.jsonl`).
- Pre-existing/unrelated work preserved: `config/analytics.json` launch count was
  incremented by runtime; `config/melee.json` was changed concurrently by work
  outside this session and was not edited or reverted.

## What now works

- Zombie Tower enters a 30-second lobby intermission, waits for a human before
  starting, runs the 3/2/1/GO countdown, moves the team to the authored
  `tower_intro` spawn, and freezes team-0 actors until GO.
- The human team target is the configured capacity (4 by default). One human is
  filled with three allied NPCs; `zt_team humans` takes a slot from a companion.
- `zt_team humans|zombie|spectator` is server-authoritative and works during an
  active run. The zombie choice assigns the Normal Zombie role and loadout.
- Joining humans spawn at the last activated checkpoint during an active run.
  Zombie players spawn at the next checkpoint the humans have not reached; when
  every checkpoint is reached they use the highest reached checkpoint and emit
  `zombie_tower.zombie-spawn-no-ahead-checkpoint`.
- Human-team starting loadout is Revolver 6/120, Shotgun 2/80, Rocket Launcher
  4/16, and Force Punch, applied through role `weapon_ammo`.
- Authored `checkpoint`, `monster_zone`, `boss_trigger`, `tower_end`, and
  `spawnpoint` entities are consumed. Monster zones activate only for human-team
  players, spawn a finite configured pool, and are one-shot by default.
- The Chainsaw Boss placeholder uses the defined base-5000 preset and the
  10000-health placeholder role; boss name/current/max health are replicated and
  drawn only while the encounter is active.
- Team wipe with attempts remaining shows a 5-second attempt result and restarts
  at the last checkpoint with the countdown. Final loss/win shows for 5 seconds
  and returns to the lobby intermission.
- Results carry elapsed time, most damage, most healing, top speed, and names.
- Human downed state and hold-F revive are wired; monsters ignore downed players.
- Mode music is selected by the server (`on_go_track`, then a random pool on
  client track-end reports) and replicated through the match state.
- Fog, rain, and map-light torch effects are mode-gated; `zt_effects on|off`
  toggles them together without affecting forced first person.
- Boomer, Stabber, Crawler, Brute, and Charger have data-ready presets/roles using
  their existing avatars. They are unarmed/not behaviorally distinct until their
  stats, attacks, and animation clips are authored.

## Exact changes by file

### Config and data

- `config/behavior-profiles.json` (`655-663`): new `zombie_tower_companion`
  profile. Before: no companion profile. After:
  `"id": "zombie_tower_companion", "focus_enabled": true, "focus_mode": "hostile_team", "cohesion": 0.6`.
- `config/gamemodes/zombie_tower.json` (`4-38` and following): before:
  `"intermission_seconds": 5` and two teams with no roles. After: 30-second
  intermission, `"run_mode": true`, three teams with roles/`npc_role`, capacity 4
  humans, `"friendly_fire": false`, `"force_first_person": true`,
  `"allow_midrun_team_change": true`, fog/rain/torch features, the exact 11-track
  `music.random_pool`, and `"npc_spawn": {"policy":"persistent","max_count":0,...}`.
- `config/gui/gamemode-meta-gui.json` (`160-181`): new `"zombie_tower"` layout
  with `ztAttemptsText`, `ztCheckpointText`, `ztAttemptResultText`,
  `ztResult*Text`, `ztBossNameText`, `ztBossHpText`, `ztBossBarBg`,
  `ztBossBarFill`, and `ztDownedText`. Before: no Zombie Tower layout.
- `config/roles.json` (`140-302`): new `zombie_tower_human`,
  `zombie_tower_companion`, `zombie_tower_zombie`, boss base/placeholder, six
  monster roles, and `weapon_ammo` overrides. Before: no Zombie Tower roles.
- `config/weapons.json` (`1949-1997`): new `zombie_claw` using the existing
  spyknife contact behavior and existing contact values. Before: no zombie claw.
- `config/weaponsets.json` (`114-137`): new `zombie_tower_humans`,
  `zombie_tower_zombie`, and `zombie_unarmed` sets. Before: sets ended at
  `captain_falcon`.
- `config/monster-pools.json` (new): `"default"` pool with
  `{"role":"zombie_tower_normal_zombie_npc","count":1}`. No prior text.
- `config/actor-presets/zombie_tower_human.json` (new): movement `source`,
  weapon set `zombie_tower_humans`, revolver start, health 100.
- `config/actor-presets/zombie_tower_companion.json` (new): same loadout plus
  `behavior_profile: "zombie_tower_companion"`.
- `config/actor-presets/zombie_tower_normal_zombie.json` (new): forced
  `monsternormal` avatar, `zombie_tower_zombie` set, `aggressive` placeholder.
- `config/actor-presets/zombie_tower_chainsaw_boss.json` (new): forced
  `monsterchainsawboss` avatar, health 5000.
- `config/actor-presets/zombie_tower_{boomer,stabber,crawler,brute,charger}.json`
  (new): forced existing monster avatars, `zombie_unarmed` set, aggressive
  placeholder. No stats/attacks were invented.

### Shader, audio, effects

- `shaders/basic.frag` (`101-106`, `268-273`): added `uFogEnabled`,
  `uFogColor`, `uFogDensity`, `uCameraPos`, and distance fog mixing. Before: no
  fog uniforms or fog math.
- `src/audio/music-manager.h` (`21-26`, `103-104`): added `playModeTrack`,
  `stopModeTrack`, `trackEnded`, `consumeTrackEnded`, `mTrackEnded`,
  `mModeTrackMode`. Before: only menu/ingame playlists.
- `src/audio/music-manager.cpp` (`298-300`, `338-339`, `354-355`, `373-398`,
  `438-439`): mode tracks no longer auto-advance locally; added `playModeTrack`
  and `stopModeTrack`. Before: every ingame track end called
  `playNextIngame()`.
- `src/effects/zombie-tower-effects.{h,cpp}` (new): client-only `zt_effects`
  state and fog/rain/torch flags. No prior text.
- `src/effects/rain-system.{h,cpp}` (new): 420 camera-local rain drops rendered
  with existing debug lines. No prior text.
- `src/render/render-world-mesh.cpp` (`19`, `171-179`): uploads fog uniforms when
  `ZombieTowerEffects` fog is enabled. Before: only lighting/shadow uniforms.
- `src/engine/engine-tick-render.cpp` (`41-44`, `797-820`): updates rain and
  map-light torch dynamic lights. Before: only hit bursts/objective pulse/debug.
- `src/engine/engine-tick-camera.cpp` (`13`, `236-237`): continuously forces
  first person while the replicated mode forces it. Before: edge-triggered only.

### Gamemode and map schema

- `src/gamemode/gamemode.h` (`38-42`, `63-71`, `256-263`): added fog/rain/torch/
  freeze features, team `npc_role`/`players`/`npcs`/`prefer_humans`, and mode
  `allowMidrunTeamChange`/`runMode`/music fields. Before: no such fields.
- `src/gamemode/gamemode.cpp` (`191-194`, `333-337`, `421-428`): parses those
  fields. Before: only `run_attempts` and the earlier feature flags existed.
- `src/gamemode/map-config.h` (`48-60`, `115-116`): added `order`, `required`,
  `activationRadius`, `healthMultiplier`, runtime counters, and
  `resetRuntimeState()`. Before: MapEntity ended at `checkpointRequirement`.
- `src/gamemode/map-config.cpp` (`178-186`, `233-242`, `394-397`): parses/saves
  the new fields, defaults monster zones to one-shot, and resets runtime state.
  Before: those fields were absent.
- `src/gamemode/match-roles.h` (`78-85`, `115-116`): new `RoleWeaponAmmo` and
  `weaponAmmo`. Before: roles had no starting-ammo data.
- `src/gamemode/match-roles.cpp` (`108-118`): parses `weapon_ammo`. Before: no
  ammo override parsing.
- `src/gamemode/monster-pools.{h,cpp}` (new): loads/hot-reloads
  `config/monster-pools.json`. No prior text.

### Input, network, runtime

- `src/input/input-state.h` (`27`): added `reviveHeld`. Before: no revive input.
- `src/input/input-poll.cpp` (`177`): maps F to `reviveHeld`. Before: F was not
  a gameplay input.
- `src/network/client.cpp` (`373`): sends `NET_STATE_REVIVING`. Before: only
  jump/dash/down-dash/freeze flags were sent.
- `src/network/packets.h` (`42-43`, `1263-1286`): added the revive state bit and
  Zombie Tower fields to `DuelStatePacket` (attempts, results, stats, boss,
  checkpoint, music, effects flags). Before: the packet ended at
  `ProceduralWorldNetworkState procedural`.
- `src/network/actor-match.h` (`28-29`): added `ActorState::Downed`. Before:
  Alive/Dead/Respawning/Spectating only.
- `src/network/server.h` (`180`, `301-306`): added `reviveHeld`, `downed`,
  `downedHealth`, `reviveProgress`, `ztSpawnResendTick`. Before: no downed state.
- `src/network/server-packets.cpp` (`84-85`, `1059`): accepts the revive flag and
  feeds `p.input.reviveHeld`. Before: the flag was filtered out.
- `src/network/server-players.cpp` (`542-547`, `835-848`, `1705`): applies
  role ammo, freezes downed players, and replicates the revive flag. Before:
  downed players did not exist and role ammo was ignored.
- `src/network/server-damage.cpp` (`83`, `177-187`, `206-234`): rejects damage to
  downed actors, credits run damage, and converts lethal team-0 damage into
  Downed with a 30-second drain. Before: lethal damage immediately set `dead`.
- `src/network/server-gamemode.h` (`284-286`, `291-319`, `420`): added run state,
  results/music/boss/stats fields, and `ActorSpawnProfile::weaponAmmo`. Before:
  only the partial checkpoint/attempt fields existed.
- `src/network/server-gamemode.cpp`: the main runtime additions:
  - `14`, `36`: includes monster pools/ctype.
  - `283-310`: initial-join role fallback so the first spawn already has the
    configured Zombie Tower loadout.
  - `418-420`: copies role ammo into spawn profiles.
  - `814-830`: resets run state, music, effects, and stats on mode start.
  - `~1430-1550`: authored lobby/intro/checkpoint/zombie spawn helpers.
  - `~2770-2800`: ZT roster diagnostic and data-driven team assignment.
  - `~3640-3730`: late-join admission and party reconcile.
  - `~3750-3830`: downed tick, attempt-result wipe, return-to-lobby.
  - `~3900-4300`: monster-zone finite group spawning, checkpoint/boss/tower-end
    run entities, and boss win/defeat handling.
  - `~5600-5700`: spawn-resend recovery, GO music selection, lobby empty-run wait.
  - `~6000-6120`: `zt_team`/mid-run team change, checkpoint spawn, companion
    reconciliation.
  - `~6280-6360`: `zt_music_ended` server selection.
  Before these changes, Zombie Tower was a persistent NPC spawner with no team
  roles, no run phases, no lobby/checkpoint transitions, and no boss state.
- `src/network/server-npcs.cpp` (`240-241`, `308-331`, `400`, `496`, `1113`):
  applies role ammo, holds team-0 companions during countdown, ignores downed
  players for targeting, and uses ZT down/freeze state. Before: no companion or
  downed filtering.
- `src/network/server-packet-chat.cpp` (`24`, `30`, `35`, `416-485`): added
  `zt_team` and `zt_music_ended` before the host gate. Before: unknown commands.
- `src/network/community-match-client.h` (`129-153`, `208-233`): exposes and
  stores ZT run/boss/music/effects state. Before: no ZT state.
- `src/network/community-match-client.cpp` (`181-203`, `464-487`): resets and
  parses that state. Before: no ZT state.
- `src/main-systems.cpp` (`110`, `508`) and `src/network/server.cpp` (`17`,
  `434`): load `config/monster-pools.json`. Before: no pool registry load.
- `src/terminal/debug-commands.cpp` (`23`, `220-250`): added `zt_effects` and
  `zt_team` terminal commands. Before: no ZT commands.
- `src/terminal/map-entity-commands.cpp` (`191-194`): editor supports `order`,
  `required`, `activationRadius`, and `healthMultiplier`. Before: not editable.
- `src/engine/engine-tick-ui-overlays.cpp` (`40-41`, `795-876`): draws the ZT
  attempts/checkpoint/attempt/final/boss/downed HUD and drives shared music.
  Before: only generic mode HUD elements.

### Documentation

- `docs/specs/gamemodes/zombie-tower/implementing/zombie-tower-20261019.md`
  (new): full implementation plan and data contract. No prior text.

## Validation evidence

- Build: `python devscripts/dev-loop.py --request build --wait-for-build`
  produced build `2678`, status `SUCCESS`.
- Runtime: launch mode 10 (`zombietower4`, `zombie_tower`, no startup NPCs),
  executable `.dev/builds/2676/mimita.exe` for the validated run and `2678` for
  the final build.
- Runtime journal evidence:
  - `zombie_tower.roster-assigned` and `zombie_tower.roster-actor` show human
    role `zombie_tower_human`, team 0, weapon set `zombie_tower_humans`.
  - `weapon.spawn.equipped_published` shows `weapon_set: 13`, `revolver` on the
    human and `weapon_set: 14`, `zombie_claw` after `zt_team zombie`.
  - `actor.team-assigned` shows accepted `zt_team` changes to Zombies and Humans.
  - `zombie_tower.checkpoint-reached` fired for `checkpoint_01`.
  - `monster.spawn-zone-activated` and two `monster.spawned` events fired for the
    configured finite group.
  - `boss.spawned`/`boss.died` fired for the Chainsaw Boss placeholder.
  - `zombie_tower.completed` fired with `tower_end: true`, then the phase entered
    RESULTS and returned to INTERMISSION with a fresh ~30-second timer.
  - `zombie_tower.spawn-resend` proved the spawn-sync recovery path is reachable.
- Temporary map entities used only for runtime validation were restored from
  `%TEMP%\zombietower4.json.bak`; `git diff -- config/maps/zombietower4.json` is
  clean. No map coordinates are part of this change.
- Not verified: audible music, visual fog/rain/torch appearance, human-play feel,
  multiplayer with more than one human, and the exact attempt-result/revive
  overlay visuals. These remain human review.

## Remaining content and TODOs

- The player must author the real lobby/tower spawn points, ordered checkpoints,
  monster pools/volumes, boss trigger, and optional tower end in
  `config/maps/zombietower4.json`. See the plan document for the exact schema.
- Boomer, Stabber, Crawler, Brute, and Charger now have placeholder health,
  movement, contact attacks, and Captain Falcon animation clips. Their numeric
  values are intentionally provisional and should be replaced with authored data.
- The Chainsaw Boss uses the existing avatar and shared contact attack; its unique
  movement/attack animation and low-friction tuning are not invented.
- Allied NPC revive is deferred; human down/revive is implemented.
- Reactivating zones use `one_shot:false` and cooldown; the remaining-delay world
  text above the volume is not implemented.
- `healthMultiplier` is parsed but the placeholder role already carries 10000
  health; connect the multiplier to a per-spawn health override when a second
  boss preset needs it.
- The all-checkpoints-reached zombie spawn fallback logs
  `zombie_tower.zombie-spawn-no-ahead-checkpoint`; the spec decision remains open.

## Documentation TODOs found

- `docs/specs/gamemodes/zombie-tower/zombie-tower.md:1453`:
  `todo explain better  enshittification ?`
- `docs/specs/gamemodes/zombie-tower/zombie-tower.md:5591`:
  `2026 10 06 1603 jorj todo - explain zombie tower`
- `docs/specs/actor-presets/actor-presets.md:176`:
  `2026 10 19 1834 jroj todo explain this stuff is so cool like`
- `docs/specs/graphics/graphics-lighting.md:1` and
  `docs/specs/music/music.md:1` are informal notes to expand later.
- `docs/doc-review-09-03-2026.md` is absent in this checkout.

## Follow-up 2026-10-20: placeholder monster loadouts

Scope decision: use the existing Captain Falcon clips temporarily and invent
placeholder numeric stats/attacks for Boomer, Stabber, Crawler, Brute, and
Charger. These values are explicitly provisional; replace them with authored
monster data later. No new attack behavior type was added.

### Changed files

- `config/weapons.json` (`1999`, `2026`, `2053`, `2080`, `2107`): added
  `zombie_boomer_claw`, `zombie_stabber_claw`, `zombie_crawler_claw`,
  `zombie_brute_claw`, and `zombie_charger_claw`, each with placeholder
  spyknife damage/knockback/cadence values. Before: only `zombie_claw` existed.
  Example: before `"damageTickInterval": 0.166 } } }`; after
  `"zombie_boomer_claw": { ... "damageTickInterval": 0.2 }`.
- `config/weaponsets.json` (`140-179`): added role-only weapon sets 16-20.
  Before: `"id": 15, "key": "zombie_unarmed"` was the final set. After:
  `"id": 20, "key": "zombie_tower_charger"`.
- `config/roles.json` (`221-289`): assigned placeholder health, movement,
  weapon set, and starting weapon to each `_npc` role. Before:
  `"weapon_set": "zombie_unarmed", "starting_weapon": "nothing"`. After:
  e.g. Boomer `"health": 120`, `"movement_preset": "heavy"`,
  `"weapon_set": "zombie_tower_boomer"`, `"starting_weapon": "zombie_boomer_claw"`.
- `config/actor-presets/zombie_tower_boomer.json` (`5-14`), `..._stabber.json`
  (`5-14`), `..._crawler.json` (`7-14`), `..._brute.json` (`5-14`), and
  `..._charger.json` (`5-14`): same placeholder stats/loadouts. Before:
  `"movement_preset": "source", "weapon_set": "zombie_unarmed",
  "starting_weapon": "nothing"`. After: e.g. Brute `"max": 300`,
  `"movement_preset": "heavy"`, `"starting_weapon": "zombie_brute_claw"`.
- `config/monster-pools.json` (`6`, `14`, `19`): added `ground_mixed`,
  `floor_01_main`, and `floor_01_heavy` pools. Before: only `default`. After:
  `"floor_01_heavy": [{ "role": "zombie_tower_brute_npc", "count": 1 }, ...]`.
- `assets/avatars/monstertest1/animations.json`,
  `assets/avatars/monsterstabber/animations.json`,
  `assets/avatars/monstercrawler/animations.json`,
  `assets/avatars/monsterbrute/animations.json`, and
  `assets/avatars/monstercharger/animations.json` (new): verbatim copies of
  `assets/avatars/captainfalcon/animations.json`. Before: no `animations.json`
  in these avatar directories, so authored melee clips were unavailable.

### Validation evidence

- Build: dev-loop build `2680`, status `SUCCESS`.
- Runtime: mode 10, `zombietower4`, runs `20261020_202019` and `20261020_202400`.
- `monster.spawn-zone-activated` reported `"monster_pool":"ground_mixed",
  "spawn_count":6` and `"spawn_count":8` in the two runs.
- `monster.spawned` reported every new role across the runs, including
  `zombie_tower_boomer_npc`, `zombie_tower_stabber_npc`,
  `zombie_tower_crawler_npc`, `zombie_tower_brute_npc`, and
  `zombie_tower_charger_npc`.
- `npc.grenade-availability` reported each new weapon as
  `"definition_found":true`, `"runtime_exists":true`, `behavior` 10, and team 1;
  for example `"equipped":"zombie_charger_claw"` on actor `100009`.
- `anim.state.changed` reported `"avatar":"monstertest1"`,
  `"avatar":"monsterstabber"`, `"avatar":"monstercrawler"`,
  `"avatar":"monsterbrute"`, and `"avatar":"monstercharger"` entering
  `"to":"captainfalcon_walk"`.
- `anim.melee.pose_probe` reported `"set_valid":true` and
  `"authored_only":true` for the monster avatars.
- NPC state logs showed the new roles entering `ATTACK` against human-team NPCs.
- Temporary test map changes were restored; the map file hash matches the backup
  and `git diff -- config/maps/zombietower4.json` is clean.
- Not verified: actual damage numbers/feel, unique attacks, death/ragdoll tuning,
  and visual acceptance. Placeholder values are for testing only.

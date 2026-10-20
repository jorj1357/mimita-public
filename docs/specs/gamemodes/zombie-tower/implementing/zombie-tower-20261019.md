# Zombie Tower — first playable slice implementation plan

Status: implementation plan and working record.
Date: 2026-10-19.
Owners/specs: `docs/specs/gamemodes/zombie-tower/zombie-tower.md`,
`docs/specs/actor-presets/actor-presets.md`, `docs/ROUTER.md`,
`docs/specs/gamemodes/gamemodes.md`.

## Locked scope decisions

- Human downed + hold-F revive now; allied NPC revive deferred.
- Normal Zombie and Chainsaw Boss placeholder wired; Boomer, Stabber,
  Crawler, Brute and Charger are data-ready presets/roles only.
- Music is server-authoritative and replicated; clients hear the same current
  track.
- Fog, rain and dynamic torch lighting are implemented and toggled together by
  `zt_effects <on|off>`. Forced first person is not changed by `zt_effects`.

## Current repository facts (before this slice)

| Area | Current state |
|---|---|
| Mode runtime | Zombie Tower is a persistent-NPC-spawn mode, not a run mode. |
| Phases | Generic intermission/countdown/GO/active/results exist, but no ZT transitions. |
| Teams | Teams parse id/capacity/role/spawn group, but ZT teams have no roles and humans remain team -1. |
| Map entities | `spawnpoint`, `checkpoint`, `monster_zone`, `damage_volume`, `boss_trigger`, `pickup` exist; only some are consumed. |
| Attempts | Partial decrement/checkpoint respawn; no 5s overlay, final results, or lobby return. |
| Boss | Generic NPC only; no boss role, 2x health, boss bar, or progression lock enforcement. |
| Actors/roles | No ZT human/companion/zombie/monster roles or presets. |
| Monsters | Avatar/model folders exist; no role, stats, attacks, most animation clips. |
| Down/revive | None. |
| Music | Client-local random ingame playlist only; no mode JSON fields or shared selection. |
| Effects | Fog parsed but not rendered; rain absent; dynamic lights are muzzle-flash only. |
| HUD | Generic mode HUD only; no `zombie_tower` layout, boss bar, attempt overlay, or results rows. |
| Map data | `zombietower4.json` has only two damage volumes and one monster zone. |

## Data contracts

### Mode JSON

`config/gamemodes/zombie_tower.json` will gain:

```json
{
  "maps": ["zombietower4"],
  "intermission_seconds": 30,
  "results_seconds": 5,
  "countdown_seconds": 3,
  "go_seconds": 1.0,
  "run_attempts": 3,
  "friendly_fire": false,
  "force_first_person": true,
  "allow_midrun_team_change": true,
  "teams": [
    {
      "id": "humans", "display_name": "Humans", "capacity": 4,
      "role": "zombie_tower_human",
      "npc_role": "zombie_tower_companion",
      "spawn_group": "human_spawn",
      "players": true,
      "prefer_humans": true
    },
    {
      "id": "zombies", "display_name": "Zombies", "capacity": 64,
      "role": "zombie_tower_zombie",
      "spawn_group": "zombie_spawn",
      "npcs": true
    },
    {
      "id": "spec", "display_name": "Spectator", "capacity": 0,
      "role": "spectator"
    }
  ],
  "npc_targeting": {
    "mode": "opposite_team",
    "include_players": true,
    "include_npcs": true
  },
  "features": {
    "fog": true,
    "rain": true,
    "torch_lighting": true,
    "freeze_during_countdown": true
  },
  "music": {
    "on_go_track": "assets/music/ingame/Roblox Music - Gothic - Zero Project.opus",
    "random_pool": [ "...exact 11 paths from the spec..." ]
  },
  "npc_spawn": {
    "policy": "persistent",
    "max_count": 0,
    "interval_ticks": 60,
    "per_interval": 1
  }
}
```

`max_count: 0` keeps the run lifecycle enabled while disabling the old
persistent fallback spawner.

### Roles, presets, weapons and ammo

`config/roles.json` gains:

- `zombie_tower_human` — team 0; `zombie_tower_humans` weapon set;
  revolver starting weapon; `weapon_ammo` overrides:
  revolver 6/120, shotgun 2/80, rocket launcher 4/16, force punch unlimited.
- `zombie_tower_companion` — same loadout, companion behavior profile.
- `zombie_tower_zombie` — team 1, Normal Zombie avatar/preset,
  `zombie_tower_zombie` weapon set.
- `zombie_tower_chainsaw_boss` — base health 5000.
- `zombie_tower_chainsaw_boss_placeholder` — health 10000 (2x base).
- Data-ready roles for Boomer, Stabber, Crawler, Brute and Charger: team 1,
  known avatar, `zombie_unarmed` weapon set, no invented stats or attacks.

New actor presets use the existing avatars: `monsternormal`, `monstertest1`
(Boomer), `monsterstabber`, `monstercrawler`, `monsterbrute`,
`monsterchainsawboss`, `monstercharger`.

Weapon sets:

- `zombie_tower_humans`: revolver, shotgun, rocket_launcher, force_punch.
- `zombie_tower_zombie`: `zombie_claw`.
- `zombie_unarmed`: nothing.

`zombie_claw` reuses the existing spyknife contact behavior and existing
spyknife contact values as a documented placeholder. No new damage number is
invented.

### Map entity schema

`MapEntity` gains:

- `order` (checkpoint progression order)
- `required` (required vs optional encounter)
- `activation_radius` (0 = use `radius`)
- `health_multiplier` (boss placeholder uses 2.0)
- runtime-only activation counters

Entity behavior:

- `spawnpoint` tags `lobby`, `tower_intro`, `human_spawn`, `zombie_spawn`.
- `checkpoint` with `order`.
- `monster_zone` with `monster_pool`, finite `spawn_count`, `max_alive`,
  `spawn_cooldown_ticks`, `one_shot` (default true), `required`,
  `checkpoint_requirement`; activate only for human-team players.
- `boss_trigger` with `boss_id`, `health_multiplier`,
  `checkpoint_requirement`.
- optional `tower_end`.
- existing `damage_volume` unchanged.

Monster pools live in `config/monster-pools.json`:

```json
{
  "pools": {
    "floor_01_main": [
      { "role": "zombie_tower_normal_zombie_npc", "count": 4 }
    ]
  }
}
```

### Map data to author

In `config/maps/zombietower4.json` (or the map GLB for geometry), place:

1. `spawnpoint` tag `lobby`.
2. `spawnpoint` tag `tower_intro`.
3. Ordered `checkpoint` entities (`checkpoint_01`, `checkpoint_02`, ...).
4. `monster_zone` entities with activation radius, spawn box size,
   `monster_pool`, finite `spawn_count`, `max_alive`, `one_shot`, `required`,
   `checkpoint_requirement`.
5. `boss_trigger` with `boss_id:
   "zombie_tower_chainsaw_boss_placeholder"` and `health_multiplier: 2.0`.
6. Optional `tower_end` after the boss. If omitted, the placeholder boss death
   completes the run.
7. Torch positions as map GLB `KHR_lights_punctual` lights, or a simple
   `torch` entity if JSON authoring is preferred.

No map coordinates are invented by this slice.

## Implementation workstreams

### A. Config/schema loaders

- `src/gamemode/gamemode.h/.cpp`: `allowMidrunTeamChange`,
  `musicOnGoTrack`, `musicRandomPool`,
  `features.fog/rain/torchLighting/freezeDuringCountdown`,
  `team.players/npcs/preferHumans/npcRole`.
- `src/gamemode/map-config.h/.cpp`: new entity fields, parser/save/editor,
  runtime-reset helper.
- `src/gamemode/match-roles.h/.cpp`: `weapon_ammo` parsing and propagation
  into `ActorSpawnProfile`.
- New `src/gamemode/monster-pools.h/.cpp`: load/hot-reload
  `config/monster-pools.json`.
- Register loaders in `src/main-systems.cpp` and `src/network/server.cpp`.

### B. Teams, roster and spawn

- Data-driven team auto-assignment using `players`/`npcs` flags.
- `npc_role` selection for allied NPCs.
- `prefer_humans` capacity eviction.
- `allow_midrun_team_change`.
- `reconcileZombieTowerParty`: target = configured capacity (4 default),
  humans first, fill/remove allies.
- `zombieTowerSpawnForActor`: lobby during intermission/results, tower intro
  during countdown, last checkpoint during active, next unreached checkpoint
  for zombies.
- Late-join admission: humans when room remains, otherwise spectator.
- `zt_team <humans|zombie|spectator>` client command and server handler.
- Initial join and team change use the ZT spawn helper.
- Allied NPC spawning on team 0 with the companion role; monster NPCs on
  team 1 with the pool role; skip downed players when targeting; apply
  per-role ammo.

### C. Phase loop, lobby, attempts, checkpoints, results

- 30s lobby intermission with lobby spawn; wait for map config readiness.
- Countdown moves humans/allies to `tower_intro` and freezes them; 3/2/1/GO.
- Checkpoint and zone activation only by human-team players.
- Wipe counts all team-0 actors (humans + allies).
- Retry: 5s attempt overlay, then checkpoint countdown (3/2/1/GO).
- Final: 5s win/loss + stats, then lobby intermission.
- Reset one-shot zones and run state at new-run/results, not retries.
- Track run start tick, damage, healing and top speed.
- Win: boss placeholder defeated + required encounters cleared + optional
  `tower_end` reached.
- All-checkpoints-reached zombie fallback: highest reached checkpoint with a
  reasoned log event.

### D. Human downed/revive

- `ActorState::Downed`.
- `ServerPlayer` downed health/timer/revive progress.
- Lethal damage enters Downed; 30s drain; dead at zero.
- Held revive input (F) through input/network input state.
- Nearest downed teammate in range; heal while held; interrupt on release,
  range loss, invalid state; revive at 100 HP.
- Downed actors cannot move/attack; monsters ignore downed actors.
- Replicate downed state; show a small downed/revive prompt.
- Allied NPC revive deferred.

### E. Encounters and boss

- Replace ZT persistent fallback spawning with `monster_zone` group spawning.
- Activation by human-team players only; finite group; `max_alive` throttle.
- One-shot default; cooldown reactivation when `one_shot: false`.
- Honor `checkpoint_requirement`; track `required` and cleared state.
- Boss resolves `boss_id` to a role, spawns health x multiplier, sets boss
  state, clears on death.
- No world-space remaining-delay text in this slice.

### F. Replication and HUD

- New `PACKET_ZOMBIE_TOWER_STATE` with attempts, result flags, stats, boss
  name/health, checkpoint serial/name, music serial/track, visual flags.
- Client mirror state.
- `zombie_tower` layout in `config/gui/gamemode-meta-gui.json`.
- HUD branches for intermission, countdown, attempts, attempt result, final
  results, checkpoint message, downed/revive prompt, boss bar.
- Boss bar only while the encounter is active.

### G. Music

- Parse mode `music`.
- `MusicManager::playModeTrack`, `stopModeTrack`, end-of-track notification.
- Server selects `on_go_track` at GO, advances to random pool on client
  end-of-track report, replicates serial/track.
- Client plays current track and reports end; stops on final results/lobby.
- Late joiners start the current track from the beginning.

### H. Effects and forced first person

- `zt_effects <on|off>` toggles fog, rain and torch lighting together.
- Fog uniforms in `shaders/basic.frag` and `render-world-mesh.cpp`.
- Rain particle system.
- Persistent/flickering torch lights from map GLB lights in
  `DynamicLightManager`.
- Forced first person enforced every camera tick; unaffected by `zt_effects`.

### I. Documentation and changelog

- Add the first-slice map data contract to the Zombie Tower spec.
- Update actor-preset current-behavior facts where materially changed.
- Write exactly one changelog with exact paths, line numbers, and verbatim
  before/after quotes for every changed file.

## Validation plan

1. Inspect `git status`/`git diff` before every overlapping edit.
2. Build through `python devscripts/dev-loop.py --request build
   --wait-for-build`.
3. Launch ZT through the existing dev loop (launch mode 10 or a ZT profile).
4. Record the exact executable and `EVENTS_JSONL_PATH`.
5. Exercise lobby intermission, countdown/freeze, roster, `zt_team`, checkpoint
   spawn, zombie respawn, encounter activation, boss, wipe/retry, final
   results, effects toggle and music.
6. Inspect the journal for the expected ZT, roster, spawn, boss and music
   events; identify the first divergence.
7. Report build evidence, runtime evidence and human visual/multiplayer
   acceptance separately.
8. No new tests are added or run unless requested.

## Known limitations to report

- Map coordinates are authored by the user; end-to-end playability depends on
  that content.
- Boomer, Stabber, Crawler, Brute and Charger are data-ready but unarmed/not
  behaviorally distinct; stats, attacks and animation clips remain
  unspecified.
- Chainsaw Boss uses the existing avatar and shared contact attack placeholder;
  no chainsaw movement/attack animation or tuning is invented.
- Human down/revive is implemented; allied NPC revive is deferred.
- Reactivating volumes work by cooldown; the remaining-delay world text is not
  implemented.
- The all-checkpoints-reached zombie spawn uses a reasoned fallback.
- Lobby “tower loading” is a readiness guard, not asynchronous streaming.
- Music is shared by track selection, not sample-synced for late joiners.
- Results and boss-bar styling reuse the existing simple HUD conventions.

## Spec TODOs found (not edited)

- `docs/specs/gamemodes/zombie-tower/zombie-tower.md:1453`:
  `todo explain better  enshittification ?`
- `docs/specs/gamemodes/zombie-tower/zombie-tower.md:5591`:
  `2026 10 06 1603 jorj todo - explain zombie tower`
- `docs/specs/actor-presets/actor-presets.md:176`:
  `2026 10 19 1834 jroj todo explain this stuff is so cool like`
- `docs/specs/graphics/graphics-lighting.md:1` and
  `docs/specs/music/music.md:1` are informal notes to expand later.
- `docs/doc-review-09-03-2026.md` is absent in this checkout.

# Fun Counter Strike

**Status:** Desired behavior contract for the `funcounterstrike` gamemode.

## Purpose

Fun Counter Strike is the respawning, bomb-focused sibling of
`counterstrike`. It reuses the same teams, bomb objective, weapons, grenades,
NPC brains, damage, networking, and map authoring. It does not fork movement,
damage, weapon, or projectile systems.

Keep the rules distinct:

- `counterstrike` is the one-life tactical mode. A round can end by elimination.
- `funcounterstrike` is the respawning bomb mode. Elimination never wins a
  round; the bomb objective alone decides it.

## Round lifecycle

- Unplanted round timer: **90 seconds** by default (`rounds.round_seconds`).
- Countdown: **3 seconds**, synchronized for every actor on the authoritative
  tick. The shared countdown freeze applies.
- Round result break: **3 seconds** (`rounds.results_seconds`) before the next
  synchronized countdown.
- Rounds are **endless** by default (`rounds.endless_rounds: true`).
- Match length is configurable with `rounds.rounds_to_win` or
  `rounds.max_rounds`. A positive value ends the match after that many wins or
  rounds; `0` means endless.
- At round start, dead/spectating state clears, health and loadout reset, the
  bomb resets, and every participant respawns at the correct team spawn.

## Respawn and loadout

- `respawn_seconds` is non-zero during active rounds, so the shared respawn
  loop runs. Death is a short respawn interval, not a round-ending event.
- A respawn reapplies the mode loadout. The current Fun Counter Strike config
  uses the dedicated Fun Counter Strike set (`config/weaponsets.json` key `funcounterstrike`): the TDM loadout plus frag, smoke, fire, darkbang, and the bomb, through
  the `fun_counter_terrorist` / `fun_terrorist` roles.
- Do not add a Fun-Counter-Strike-only respawn, health, or inventory path.

## Bomb objective

Bomb rules are the only round-win source:

- **Terrorists win** by detonation.
- **Counter-Terrorists win** by defusing, or by preventing a plant before the
  round timer expires.
- Eliminations never win a round. `features.objective_only_round_wins: true`
  disables the shared elimination-round-end branch for this mode.
- A planted bomb remains authoritative after the round timer and after either
  team is eliminated.
- If the carrier dies or disconnects, the bomb drops and remains recoverable by
  Terrorists. A failed or unreachable drop falls back to a valid living
  Terrorist or a recoverable world position.
- The bomb is an equippable carried item (`config/weapons/bomb/config.json`) and
  objective state. The objective carrier state remains the gameplay authority;
  the item provides inventory/presentation identity.

### Plant, defuse, and interaction

Planting and defusing are deliberate server-authoritative interactions. The
bomb site only makes the interaction available; entering the circle never
plants automatically.

Two interaction modes are supported through the mode objective configuration:

- **Mode 1: hold (default).** Hold Interact while eligible. Progress advances
  once per fixed tick. Releasing Interact, dying, or leaving the valid site or
  range cancels progress and restores the actor's input.
- **Mode 2: repeated tap.** Each distinct press/release adds
  `tap_progress_seconds` (default 0.1 s). Ten presses plant a 1-second bomb and
  fifty presses defuse a 5-second bomb. Releasing between taps does not cancel
  accumulated progress; dying or leaving the valid area does.

The current Fun Counter Strike durations are **1 second to plant** and
**5 seconds to defuse**. While a Mode 1 plant is in progress, movement input and
movement abilities are blocked but momentum is preserved, so a sliding player
keeps sliding. Input is restored when the plant completes or is cancelled.
Mode 2 taps are instantaneous and do not continuously lock movement.

Do not create a second objective state machine in the GUI or NPC brain.
Plant/defuse progress is fixed-tick and interruptible. NPCs use the same
authoritative progress rules; their interaction intent comes from the shared
utility goal/brain path, not a parallel objective implementation.

### Round and objective HUD

Fun Counter Strike uses the general contract in
`docs/specs/gamemodes/objective-hud.md` with a dedicated `funcounterstrike`
layout in `config/gui/gamemode-meta-gui.json`.

Required presentation:

- Intermission text and a synchronized `3 → 2 → 1 → GO!!!` countdown.
- Phase label (`INTERMISSION`, `GET READY`, `GO!`, `ACTIVE`, `ROUND OVER`).
- Round number and CT/T round score.
- Unplanted active round timer: **90 seconds** by default.
- While carried: `BOMB CARRIED` and the team hint (`PLANT THE BOMB` for
  Terrorists, `DEFEND THE SITES` for Counter-Terrorists).
- While dropped: `BOMB DROPPED` and the recovery/hold hint.
- While planted: hide the round timer and show `BOMB PLANTED <seconds>` from the
  authoritative explosion timer (**40 seconds** by default).
- Plant/defuse progress prompts.
- Result: `BOMB DEFUSED`, `BOMB EXPLODED`, or `TIME EXPIRED` alongside the round
  winner.
- No two competing countdowns may be shown at once.

Mode objective timers are authoritative. Map `bomb` timers are fallback only
when the mode omits a value; the effective source and values are logged as
`objective.timers.resolved`.

### Bomb sites and spawn authoring

- Bomb sites live in `config/maps/<map>.json` and are hot-reloadable.
- A site has `id`, `position`, `radius`, and `visible_debug`.
- The editor draws each site as a labeled circular usable area and highlights
  the selected site.
- Sites are editable through the map editor:
  - `mapeditor add bombsite [id] [radius]`
  - select with the outliner or `mapeditor select <id>`
  - move with `mapeditor set_position <x> <y> <z>` / nudge / drag
  - resize with `mapeditor set_radius <meters>`
  - delete with the shared delete action
  - `mapeditor save` persists the site list
- Existing terminal helpers (`site_debug move`, `site_debug save`) remain valid.
- Team spawns resolve from authored `spawnpoint` entities (`ct_spawn`,
  `t_spawn`) or from tagged map nodes. The mode config names the groups; the map
  owns the positions.
- `funcounterstrike` uses `spawn_strategy: team_shared`, never `shared`:
  Counter-Terrorists spawn at the CT spawn group and Terrorists at the T spawn
  group. A shared strategy is a mode-config error for this mode.
- Shipped Fun Counter Strike maps must provide distinct CT/T spawns. The current
  map list is limited to `dust2cyberiav4`, which has explicit `ct_spawn` and
  `t_spawn` authored spawnpoints.
- Runtime evidence must show distinct `team_a`/`team_b` positions and no
  `spawn-strategy-shared` fallback in `gamemode.spawn-resolved`.

## Grenades and configuration

The canonical grenade policy lives in each weapon definition under
`config/weapons/<id>/config.json` in a shared `grenade_effect` block. The old
`config/grenades.json` file is retired so there is one source of truth.

Shared schema fields:

```json
"grenade_effect": {
  "enabled": true,
  "area_kind": "fire|smoke|darkbang|none",
  "radius": 5.0,
  "height": 3.0,
  "duration_seconds": 7.0,
  "damage_per_tick": 10,
  "damage_interval_ticks": 5,
  "damages_enemies_only": true,
  "spawns_area_effect": true,
  "fuse_seconds": 2.0,
  "direct_effect_distance": 5.0,
  "max_effect_distance": 20.0,
  "not_looking_multiplier": 0.5,
  "no_line_of_sight_multiplier": 0.05,
  "audio_on_apply": "weapon/darkbang/brownnoise"
}
```

`GrenadeRegistry` publishes these definitions from the weapon registry and
reloads with the same atomic weapon-config snapshot. Do not add a second
grenade-policy loader.

### Frag

- Highly visible red blast through the shared explosion presentation.
- **1000 damage at the center**, **25 damage at the outer edge** by default.
- Uses the shared splash/damage/knockback path and the optional
  `splashEdgeDamage` center-to-edge interpolation.

### Smoke

- Gray volume, **20 m diameter** (`radius: 10.0`).
- **10-second tick-based lifetime**; clients count down the remaining time.
- The **outer 5% of the radius is the fade band**.
- NPC perception currently does not consume smoke. Treat that as an open hook;
  do not invent a second smoke query in combat code.

### Fire

- Cylinder, **10 m diameter** (`radius: 5.0`) by default.
- Configured to deter and kill over sustained exposure. Defaults are
  `10 damage / 5 ticks` for 7 seconds; tune the JSON, not the code.
- Damage goes through the shared fixed-tick area-effect and damage paths.

### Darkbang

- Exposure-based black-screen intensity from distance, facing, and line of
  sight.
- Duration scales with exposure.
- Plays the configured brown-noise audio on apply; the default asset is
  `assets/sound/weapon/darkbang/brownnoise.wav`.
- Recovery is client-owned and returns the screen/audio state to normal when
  the timer expires.

## NPC grenade policy

- NPCs use grenades **early** when a valid opportunity exists.
- Before throwing, check:
  - a ready grenade is owned and loaded,
  - the target is inside useful throw range,
  - the thrower is not in its own blast,
  - no teammate is inside the blast radius,
  - the shared utility score accepts the throw.
- Log every decision as `npc.grenade-decision` with weapon, kind, distance,
  radius, score, friendly count, enemy density, cover state, and self-blast
  result.
- NPC objective behavior retains the shared TeamBrain/utility owners:
  carrier commits to a site, supporters escort, CTs defend/rotate, and a
  planted bomb triggers retake/defuse behavior.

## Deathball contract

Fun Counter Strike grants Deathball through its dedicated TDM-plus-grenades loadout.

- **999 rounds per clip** (`magazine_size: 999`).
- **1.0 second fire delay** and **1.0 second reload**.
- Keep the slow wall-passing projectile, edge/center damage, tick knockback, and
  cyan-to-dark fade.
- The configured radius is not accepted as proof of world size. Runtime
  diagnostics must record:
  - configured collision radius,
  - effective visual radius,
  - world diameter,
  - player capsule height reference.
- If collision and visual radii diverge, log
  `deathball.scale.first-divergence` before changing tuning. Measure the real
  visual mesh and collision footprint in gameplay first.

## Structured evidence

Bounded, category-owned events (one per transition/decision, rate-limited where
repeated):

- Objective: `objective.carried`, `objective.dropped`, `objective.pickup`,
  `objective.plant-start`, `objective.plant-interrupted`,
  `objective.planted`, `objective.defuse-start`,
  `objective.defuse-interrupted`, `objective.defused`,
  `objective.interaction-progress`, `objective.site-unavailable`,
  `objective.target-unresolved`, `objective.exploded`, round
  start/go/active/result.
- Grenades: `area_effect.spawn`, `grenade.effect.damage`,
  `grenade.effect.expired`, `grenade.smoke.spawn`, `grenade.fire.spawn`,
  `grenade.darkbang.exposure`.
- NPC: `npc.grenade-availability`, `npc.grenade-decision`,
  `npc.grenade-equipped`, `npc.grenade-thrown`.
- Deathball: `deathball.config.resolve`, `deathball.local_scale`,
  `deathball.spawn`, `deathball.wall-pass`, `deathball.contact`,
  `deathball.scale.first-divergence`.
- First expected/actual divergence: each investigation records the first event
  where the expected state and observed state differ, with owner, function, and
  tick.

## Defaults

| Setting | Default | Owner |
|---|---|---|
| Unplanted round timer | 90 s | gamemode JSON |
| Plant | 1 s | gamemode JSON |
| Defuse | 5 s | gamemode JSON |
| Interaction mode | hold (Mode 1) | gamemode JSON |
| Tap progress | 0.1 s | gamemode JSON |
| Countdown | 3 s | gamemode JSON |
| Result break | 3 s | gamemode JSON |
| Rounds | endless | gamemode JSON |
| Match length | configurable (0 = endless) | gamemode JSON |
| Respawn | instant (non-zero seconds) | gamemode JSON |
| Smoke | 20 m diameter, 10 s | smoke weapon JSON |
| Fire | 10 m diameter, 10 dmg / 5 ticks | fire weapon JSON |
| Frag | 1000 center / 25 edge | frag weapon JSON |
| Deathball | 999 clip, 1 s fire, 1 s reload | deathball weapon JSON |

## Runtime acceptance

Build success is not acceptance. Use the dev loop, the exact published
executable, and its `events.jsonl`:

1. Start `funcounterstrike` on a map with authored bomb sites.
2. Confirm the loadout and instant respawn during an active round.
3. Confirm hold and repeated-tap plant/defuse progress, cancellation, and input
   restoration.
4. Confirm T detonation, CT defuse, and CT no-plant timeout outcomes.
5. Confirm elimination does not end a round.
6. Confirm NPC plant/defuse/recovery decisions, CT contest/retake behavior, and
   early grenade decisions.
7. Confirm smoke lifetime/fade and countdown, darkbang exposure and recovery,
   fire damage cadence, and frag center/edge damage.
8. Confirm Deathball clip/fire/reload and damage/feedback for player-to-player,
   player-to-NPC, NPC-to-player, and NPC-to-NPC.
9. Separately perform human visual review for smoke boundaries, Darkbang
   screen/audio recovery, fire/frag readability, Deathball size/knockback, map
   editor authoring, and multiplayer presentation.

Record build, runtime-journal, and human acceptance evidence separately.

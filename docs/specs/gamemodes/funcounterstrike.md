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

## Simulation time units

Authoritative gameplay time is integer fixed ticks at 60 Hz.

- Server configuration uses `*_ticks` fields. `*_seconds` is accepted only as a
  load-time convenience and is converted to ticks.
- Server simulation, client prediction, and replay compare ticks, not render
  frames.
- Human-facing text derives seconds as `ticks / 60` and may show two decimals,
  for example `9.83`.
- Structured logs carry the authoritative `tick` and may also carry derived
  seconds for readability.
- One value has one owner. Per-weapon `grenade_effect` owns grenade fuse and
  area-effect lifetime; `custom_params.fuseTime` is retired for grenades.

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
- Every FCS launch and respawn must resolve to the FCS role/mode loadout. A
  launcher profile's unrelated explicit weapon set must not override it; in
  particular, FCS must never start with weapon set 12 (`captain_falcon`) or
  `captain_falcon_moveset`. The effective set must be ID 30 / key
  `funcounterstrike`, and the inventory must contain its configured guns and
  throwable items. Dev launch modes that inherit a profile set must select
  `weapon_set: auto` unless they deliberately specify a mode-compatible set.
- Runtime acceptance inspects the server's `actor.loadout.spawned` records and
  the connected client's applied spawn loadout. It verifies the effective set
  ID/key, equipped gun, and inventory IDs, and confirms Captain Falcon IDs are
  absent. A configured mode name or HUD alone is insufficient proof.
- `features.destructible_body_parts` is `false` in Fun Counter Strike. Actors
  lose ordinary actor HP from hits; body-part health does not destroy parts or
  independently kill the actor in this mode.
- Do not add a Fun-Counter-Strike-only respawn, health, or inventory path.

## Team identity and avatars

- Every active human and mode-rostered NPC has one authoritative team and role
  before its identity is replicated. A client that joins during a round receives
  a valid team and role or remains explicitly unassigned; it must never receive
  a fabricated identity row with team `255`.
- FCS may define its round lifecycle through `victory.type` and `rounds` while
  the legacy `victory_condition` field is empty. Late-join team/role assignment
  follows the active FCS roster boundary and must not be gated only on that
  legacy field.
- CT actors use the authored `fun_counter_terrorist` role avatar
  (`abusiveboy`); T actors use `fun_terrorist` (`jason`). The server is the
  authority for this selection and for avatar-manifest changes.
- The selected role avatar is copied into the authoritative player identity
  and replicated to peers with actor snapshots. A rejected custom manifest
  cannot replace it; an explicit `actorlist` request reports each actor's
  effective avatar-policy result in that process's journal.
- Custom avatar changes and live avatar changes are disabled for this mode.
  The avatar editor may still expose presentation fields only if they cannot
  change the replicated avatar identity.
- Unknown team identity is not evidence that an actor is an enemy. Until a
  client has a known local team and the actor's known team, it must not apply the
  enemy-team outline.
- Only actors explicitly placed into the mode roster may participate. Generic
  startup/map NPCs that have no mode team and role are removed or otherwise
  prevented from entering the FCS match. NPCs intentionally assigned to either
  team remain eligible.
- The actor inspection command reports controller, team, role, selected avatar,
  and the effective avatar-policy result. An explicit audit request writes one
  bounded snapshot to that process's canonical `events.jsonl`; normal operation
  logs identity/avatar transitions without dumping the full roster every tick.

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

The current Fun Counter Strike durations are **5 seconds to plant** and
**5 seconds to defuse**. The plant/defuse interaction range is configurable in
the objective JSON through `interaction_range_m` (default 3 m; Fun Counter
Strike ships with 6 m). The same value is authoritative on the server and drives
the client prompt range; do not hardcode a second range in the HUD. While a
Mode 1 plant is in progress, movement input and
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
`config/grenades.json` file is retired so there is one source of truth. The
`grenade_effect` block is the only owner of grenade fuse and area-effect timing;
`custom_params.fuseTime` must not appear on grenade weapons.

Shared schema fields:

```json
"grenade_effect": {
  "enabled": true,
  "area_kind": "fire|smoke|darkbang|none",
  "radius": 5.0,
  "height": 3.0,
  "duration_ticks": 420,
  "fuse_ticks": 120,
  "damage_per_tick": 10,
  "damage_interval_ticks": 5,
  "damages_enemies_only": false,
  "spawns_area_effect": true,
  "direct_effect_distance": 1.0,
  "max_effect_distance": 20.0,
  "not_looking_multiplier": 0.5,
  "no_line_of_sight_multiplier": 0.05,
  "visual_ticks": 30,
  "visual_radius": 6.0,
  "visual_alpha_start": 0.5,
  "visual_alpha_end": 0.0,
  "visual_color_rgb": [0.0, 0.0, 0.0],
  "camera_overlay": "world_hud",
  "audio_on_apply": "weapon/darkbang/brownnoise"
}
```

`GrenadeRegistry` publishes these definitions from the weapon registry and
reloads with the same atomic weapon-config snapshot. Do not add a second
grenade-policy loader. Player throws, NPC throws, client prediction, and the
local persistent-physics path all derive their fuse from `fuse_ticks`.

Grenade projectiles are physically larger and bouncier than the legacy
defaults. The configured projectile radius and bounce values are presentation
and physics policy, not a separate gameplay system.

Frag, smoke, fire, and darkbang are inventory throwables while held and must not
contribute a weapon collision capsule or mesh to actor movement/world contact.
After a click, the shared projectile owns its sphere collision and bounce. Each
weapon tunes forward speed (`projectile_speed`), vertical launch bias
(`custom_params.upBias`), gravity, drag, radius, restitution, friction, and
maximum bounce count through its existing weapon definition. There is no
charge-to-throw input. The act of throwing applies zero movement impulse to the
thrower; explosion knockback remains separately configurable.

The current readable projectile presentation is a procedural cylinder, with
per-type tint, outline, and emissive glow. An optional per-type dynamic light
follows the projectile origin plus its authored offset. See the shared weapon
spec for these per-definition fields and the current limitation that the Map
Editor does not author reusable weapon model assets yet.

### Frag

- Highly visible red blast radius through the shared explosion presentation.
- **1000 damage at the center**, **25 damage at the outer edge** by default.
- Damage is applied for exactly **one fixed tick**. The visual radius is not a
  second damage volume and must not repeatedly damage actors.
- The red radius sphere shows the full configured splash radius (20 m by
  default), starts at alpha **0.5**, and reaches alpha **0.0** over 30 ticks.
- Uses the shared splash/damage/knockback path and the optional
  `splashEdgeDamage` center-to-edge interpolation.

### Smoke

- Gray volume, **20 m diameter** (`radius: 10.0`).
- **600-tick lifetime** (10 seconds at 60 Hz); clients display the derived
  remaining seconds live with two decimals, for example `9.83`.
- The **outer 5% of the radius is the fade band**. The camera-inside volume
  primitive applies a world-only fog overlay so an actor inside the smoke cannot
  see the world through it.
- Smoke blocks NPC perception through the shared LOS query. A smoke volume
  between an NPC and a target is a blocker; do not add a second smoke query in
  combat code.

### Fire

- Cylinder, **10 m diameter** (`radius: 5.0`) by default.
- **420-tick lifetime** (7 seconds at 60 Hz).
- Starts bright orange with alpha **0.7**, transitions to dark red with alpha
  **0.3**, then disappears.
- Deals **1 damage every fixed tick** to every actor whose capsule touches the
  cylinder, including the thrower and teammates. Intersection is body/capsule
  based, not a single center-point test. Damage goes through the shared
  fixed-tick area-effect and damage paths; there is no separate fire damage path.
- NPCs treat fire as a hazard and path around it instead of entering it.

### Darkbang

- **6 m effect radius** (`radius: 6.0`). Full effect is applied within **1 m**
  of the exact centre; exposure falls off with distance.
- Exposure also considers facing and line of sight.
- The black radius sphere shows the effect radius, starts at alpha **0.5** on
  tick 1, and reaches alpha **0.0** at tick 30.
- The camera-inside volume primitive darkens the world and, in
  `world_hud` mode, the GUI. A fully opaque configured sphere makes the affected
  layer fully black.
- While intensity is active, all other sounds are ducked (made quieter). The
  Darkbang apply sound itself is not ducked.
- Duration scales with exposure in ticks.
- Plays the configured brown-noise audio on apply; the default asset is
  `assets/sound/weapon/darkbang/brownnoise.wav`.
- Recovery is client-owned and returns the screen, GUI, and audio state to
  normal when the timer expires.
- NPCs look away from a nearby Darkbang and prefer distance when no closer
  visible target forces them to engage.

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
- NPCs treat active grenade volumes as hazards:
  - smoke blocks their line of sight and target acquisition;
  - fire is avoided; NPCs path around it rather than entering it;
  - frag projectiles and fresh frag blasts are avoided; NPCs run away when they
    see them;
  - darkbang makes NPCs look away and prefer distance when no closer visible
    target forces engagement.
- Hazard decisions use the shared hazard snapshot and are logged as
  `npc.hazard-avoid`.

## NPC objective strategy

Terrorist NPCs:

- Choose **one bomb site for the whole round** as a team. The choice is
  deterministic for a given round/map seed, logged as `team.site-choice`, and
  stable until the next round.
- Every living Terrorist NPC travels to that chosen site. The carrier plants;
  supporters escort and clear the route.
- If the bomb is dropped, the nearest living Terrorist is assigned to recover
  it. It paths to the bomb, picks it up, and then resumes travel to the chosen
  site to plant.
- After the plant, Terrorists defend the bomb. They kill enemies close to the
  bomb, prioritise an active defuser, and do not chase enemies far away from the
  bomb.

Counter-Terrorist NPCs:

- Do not receive the Terrorists' round site choice. Below-elite profiles use
  perception-gated team reports and memory only. An explicitly selected elite
  profile may use the exact enemy-position knowledge defined by the NPC
  behavior spec, including through walls, but still needs legal line of sight
  to aim and fire.
- With a report, they rush toward the reported enemy area at maximum useful
  speed, fight, and try to prevent a plant.
- Without a report, they defend usable sites and rotate on new reports.
- After a plant, the nearest CT is assigned to defuse; the others cover it.
  Defusing remains the top priority.

Fun Counter Strike NPC rush:

- NPCs use known navigation routes directly instead of searching or wandering.
- The default `fun_cs_rush` profile keeps objective rush enabled at elite
  difficulty. To use the easier alternative, change the single
  `npc_behavior_profile` setting in `config/gamemodes/funcounterstrike.json`
  from `fun_cs_rush` to `fun_cs_rush_easy`; change it back to restore elite.
  The easy profile keeps objective rush enabled with 25-degree aim error, a
  120-tick minimum gap between shots, and a 0.1 chance for optional
  objective-route dashes. Dashes required to traverse a gap or large step remain
  available to preserve valid navigation. The shared dash ability still decides
  whether a request can be accepted. The profiles share objective and formation
  behavior; their optional combat and movement technique use may differ.
- NPCs may combine jump/bunny-hop and air-strafe movement to reduce travel time.
  Elite profiles may also use the NPC behavior spec's context-aware dash,
  down-dash, slope-contact chain, and anticipatory wall-avoidance rules. Easier
  profiles may reduce optional technique use while retaining movement needed
  for legal navigation.
- Rush behavior is a Fun Counter Strike behavior-profile choice, not a change to
  human Counter Strike movement.
- All timing is expressed and logged in fixed ticks; display seconds are derived.

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
  `grenade.darkbang.exposure`, `grenade.equipped-collision`,
  `grenade.thrower-impulse`, `grenade.spawn`, and `grenade.bounce`.
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
| Plant | 5 s | gamemode JSON |
| Defuse | 5 s | gamemode JSON |
| Interaction range | 6 m | gamemode JSON `interaction_range_m` |
| Interaction mode | hold (Mode 1) | gamemode JSON |
| Tap progress | 0.1 s | gamemode JSON |
| Countdown | 3 s | gamemode JSON |
| Result break | 3 s | gamemode JSON |
| Rounds | endless | gamemode JSON |
| Match length | configurable (0 = endless) | gamemode JSON |
| Respawn | instant (non-zero seconds) | gamemode JSON |
| Grenade fuse | 120 ticks (2 s) | weapon `grenade_effect.fuse_ticks` |
| Smoke | 20 m diameter, 600 ticks | smoke weapon JSON |
| Fire | 10 m diameter, 420 ticks, 1 dmg / tick, all actors | fire weapon JSON |
| Frag | 20 m radius, 1000 center / 25 edge, one damage tick | frag weapon JSON |
| Darkbang | 6 m radius, full within 1 m, 30-tick visual | darkbang weapon JSON |
| Grenade bounce | restitution ~0.82, friction ~0.12, 30 bounces | per-weapon JSON |
| Deathball | 999 clip, 60-tick fire, 60-tick reload | deathball weapon JSON |

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

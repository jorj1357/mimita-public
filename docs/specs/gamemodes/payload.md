2026 10 06 1607 jorj todo - make it like

like tf2 payload on upward
thats awesome

ppl flying aroundn  bunches of voice lines sound effects
air pressure grund pressure
INSTANT RESPAWN THO NO  WAITING THAT SUCSK 
AND ALSO MUCH More  varied classes like its fine just do whatever u want really 

but expand this 

2026 10 21 1515

# MiMITA Payload v1.0 — Working Design Specification

**Status:** Implemented vertical slice; active bug-fix/handoff phase  
**Target:** 12v12 objective-based combat, including NPC fills  
**Design philosophy:** Make the game playable first, then improve it through repeated testing. Preserve the freedom of MiMITA's movement system.

---

## 0. Implementation snapshot (2026-10-25)

This section is the handoff source of truth for a fresh agent. It describes what the code does now, what was verified, and what is still wrong. The design sections below remain the desired behavior where they do not conflict with this snapshot.

### 0.1 Current status

- A playable Payload vertical slice exists for `payload1`.
- `payload2` is now listed as a Payload map and has a starting `config/maps/payload2.json` copied from Payload 1. Its authored route/spawns still need map-specific review.
- The cart, route/checkpoints, overtime, healing, HUD, and NPC cart objective are server-authoritative.
- The cart renders the authored GLB and uses a separate gameplay collision box.
- The client HUD shows intermission, countdown/GO, red/blue team labels, timer, cart progress/status, and overtime.
- Remote/AI damage presentation was expanded to show damage numbers and hit effects for hits the local client did not predict.
- A map-editor outliner lists authored entities, blocks, route points, cart origin, and all mesh-bearing GLB nodes from the loaded map.
- Last verified build: `3212` (debug). Runtime NPC/spawn proof was captured on `3197`; build success is not gameplay acceptance.
- The dev loop later entered `launch_failed` with `server_build_identity`; manual server startup from the same build remained healthy. Treat that as an open tooling issue, not proof that the game cannot start.

### 0.2 Authoritative owners and data flow

Server:

1. `src/network/server-gamemode.cpp` owns phases, roster, round clock, checkpoints, overtime, cart result, and team statistics.
2. `src/gamemode/payload-runtime.*` owns cart distance, push/block/rollback, healing remainder, checkpoint crossing, and overtime ticks.
3. `src/network/server-npcs.cpp` owns NPC bodies, role/team assignment, respawn, and snapshots.
4. `src/physics/physical-entity.*` owns the cart body and actor support/carry.
5. `src/network/physical-entity-replication.cpp` replays physical-entity spawns to late joiners.
6. `src/network/packets.h` protocol `45` carries Payload HUD fields in `DuelStatePacket`.

Client:

1. `src/network/community-match-client.*` applies replicated Payload HUD state.
2. `src/engine/engine-tick-ui-overlays.cpp` draws Payload HUD elements from `config/gui/gamemode-meta-gui.json`.
3. `src/map/map-editor.*` edits map JSON and loaded GLB object selections.
4. `src/map/map-loader-gltf-nodes.cpp` records `MapGLBObject` metadata during GLB loading.
5. `src/network/confirmed-damage-presentation.cpp` and `src/network/multiplayer-shots.cpp` present remote damage feedback.

Map/mode data:

- `config/gamemodes/payload.json` owns mode rules, teams, class pool, map list, and presentation.
- `config/maps/payload1.json` and `config/maps/payload2.json` own spawnpoints, cart origin, cart size, interaction box, facing, and route/checkpoints.
- `config/roles.json` and `config/weaponsets.json` own Payload classes/loadouts.
- `config/gui/gamemode-meta-gui.json` owns the Payload HUD layout.

### 0.3 Current cart behavior

- Cart origin comes from map JSON `payload.cart_origin.position`.
- Cart size comes from `payload.cart_size`; current authored size is `3 x 3 x 2` world units.
- Cart model is `assets/objects/things/gamemode-objects/payload-payload.glb`; it is scaled to the gameplay size and rendered separately from collision.
- `payload.facing_mode` is `"fixed"` or `"route"`.
  - `"fixed"` uses `cart_origin.yaw` as `cart_yaw`, with no route pitch.
  - `"route"` follows the route tangent and pitch.
  - Payload 1 currently uses `"fixed"`; the authored yaw is `180`.
- The cart moves only on the server's fixed 60 Hz tick.
- A living Team A actor whose capsule intersects the transformed interaction box contributes to push speed.
- A living Team B actor in the box blocks forward movement.
- Standing on the cart top is supported and carried by the cart delta.
- Cart facing/yaw is hot-reloaded from map JSON each active Payload tick.

### 0.4 Current route and checkpoint behavior

- `payload.route[]` is ordered; each point has `id`, `position`, `radius`, `order`, `waitTicks`, `enabled`, `checkpoint`, `final`, and `time_bonus_seconds`.
- The cart position is interpolated along the authored route by `distanceTravelled`; it is not a free-moving physics cart.
- `radius` is a real arrival sphere in cart world space. The cart triggers a route point/checkpoint when its position enters that radius, so a large radius can fire slightly before the exact route distance.
- `waitTicks` is a real authored pause. The cart holds its current distance for that many server ticks when the point is first entered. Attackers in the box still refresh overtime during the pause; they just cannot move the cart.
- Checkpoints are once-only. Intermediate checkpoints add their configured seconds to the round clock.
- The final point ends the round for Team A.
- Unattended rollback is clamped to the last reached checkpoint distance (the cart's actual trigger distance, not a forward snap).
- Route points and cart origin are visible/selectable in the map editor. Selected points show checkpoint/final/off/radius/wait/bonus state in their label.
- The editor exposes route-point authoring through terminal commands (`route_checkpoint`, `route_final`, `route_enabled`, `route_radius`, `route_bonus`, `route_wait`, `route_move`, `route_validate`) and shows the same data in labels. A mouse property panel is later work.
- Authored-data validation reports long/steep segments, cart-origin/route-start mismatch, missing final, zero checkpoint bonuses, duplicate ids, and tiny radii. Warnings keep the map playable; errors are logged as `payload.authoring-validated` with `valid=false`.
- `payload2.json` is still a copy of Payload 1 and must be re-authored against Payload 2 geometry before calling it playable content.

### 0.5 Current spawn behavior and the open spawn bug

Current code path:

- `gamemodeSpawnPointForActor` → `gamemodeSpawnPoint(d, team)`.
- Team identity comes from `actorTeamOf`: `matchTeams` → `matchActors.teamId` → `ServerPlayer/ServerNpc.matchTeam`.
- `gamemodeSpawnPoint` prefers authored spawnpoint entities whose `tag` matches the team's `spawn_group`.
- If no authored candidates exist, it falls back to `teamSpawnPoints`, then `spawnA`/`spawnB`, then the shared anchor.
- Payload teams declare `spawn_group: "attacker_spawn"` and `"defender_spawn"`.
- Payload 1 map entities declare `tag: "attacker_spawn"` and `tag: "defender_spawn"`.

Resolved 2026-10-25. Root cause: `config/gamemodes/payload.json` did not declare `spawn_strategy`, and `Gamemode::spawnStrategy` defaults to `"shared"`. The shared strategy returns the single map anchor before the resolver can read team spawn groups, so both teams collapsed onto the untagged `spawnpoint_1`.

Current behavior:

- `payload.json` declares `"spawn_strategy": "team_shared"`.
- `gamemodeSpawnPointForActor` now logs `gamemode.spawn-resolved` with actor, role, team, spawn group, tag filter, candidate count, position, fallback, reason, mode, and map.
- `assignGamemodeSpawns` logs `gamemode.spawn-groups-resolved` with both team cluster counts, positions, separation, and resolution state.
- The resolver refuses untagged spawnpoints for a team that declares spawn tagging.
- `serverSpawnTagSelfTest` now checks that Payload does not use the shared strategy and that both Payload maps contain enabled `attacker_spawn`/`defender_spawn` entities.

Runtime proof (2026-10-25, payload1, build 3197):
- `gamemode.spawn-groups-resolved`: team A `(-312.59,-28.12,1150.36)`, team B `(-292.56,-64.83,2256.30)`, separation `1106.74`.
- `gamemode.spawn-resolved` for the player: `candidate_count=1`, `fallback=authored-team-group`.
- Human observation: all NPCs spawned and were visible.

Do not add a Payload-name branch. The one owner remains the shared spawn resolver plus the mode's spawn strategy.

### 0.6 NPC invisibility — resolved 2026-10-25

The reported invisibility had two code causes, both now fixed:

1. **Roster ids could collide with player ids.** `buildObjectiveRoster` only reserved the `100000+` range when the NPC mirror map was empty. A normal GUI server starts with one startup NPC, so roster ids began at `0`. A roster NPC with id `1` collided with the host player id, and the client rejects an entire snapshot whose entity ids repeat (`snapshot-chunks.cpp`, `duplicate-entity`). Roster NPCs therefore never replicated. The old cleanup also only erased ids `>=100000`, so low-id roster bodies accumulated every rebuild.
2. **Shared spawn strategy bypassed team spawns.** `payload.json` had no `spawn_strategy`; the default is `"shared"`, which returns the map anchor before reading `attacker_spawn`/`defender_spawn`. Both teams and all roster NPCs spawned at the untagged `spawnpoint_1`, fought, died, and respawned there.

Current behavior:

- Objective roster ids always start at `100000`, never use a player/startup/map id, are tracked in `objectiveRosterNpcIds`, and are reused after the previous roster is retired.
- The server drops and logs a duplicate snapshot id instead of sending an invalid snapshot. The client logs `client.snapshot.reassemble-failed` with the reason if reassembly still fails.
- `client.npc.viewport-window` now reports `dead`, `sample_npc_alive`, and `model_loaded`.
- `client.actor.draw-submission` records one-time render evidence per remote actor: alive, model loaded, valid parts/batches, tint, world position, and camera distance.
- Payload uses `team_shared` spawns and the resolver prefers authored team groups.

Runtime proof (2026-10-25, payload1 with `--npcs 1`, build 3197, journal `logs/2026-10-25/20261025_151724/events.jsonl`):

- roster ids `100000..100022`; no duplicate snapshot ids; no client reassembly failures.
- `remote_npcs=24`, `alive=24`, `dead=0`, `model_loaded=24`, `in_view` up to `10`.
- team spawns resolved to separate authored clusters.
- Human observation: all NPCs spawned and were visible.

Do not add a second NPC renderer. The shared remote NPC snapshot/render path remains the owner.
### 0.7 Current HUD behavior

- HUD layout: `config/gui/gamemode-meta-gui.json`, `gamemodes.payload`.
- Intermission text uses `match.phaseTimer()`.
- Countdown shows 3, 2, 1, then `GO!!!`.
- Top timer uses replicated `roundTimerLeft`.
- Overtime shows whole seconds plus milliseconds, derived from replicated server ticks.
- Red/blue team labels show “YOUR TEAM” / “ENEMY”.
- Tab scoreboard team tags are red/blue.
- Cart progress, nearby attacker/defender counts, and cart state are replicated.
- `DuelStatePacket` protocol is `45`.

### 0.8 Current damage and hit-effect behavior

- Confirmed hits are broadcast to all clients.
- Local attacker: hitmarker, hit sound, damage number, predicted effects remain attacker-owned.
- Remote attacker/AI: damage number, blood, decals, and hit burst are presented locally through `HitEffects::onHit`.
- Local victim: existing local-victim presentation path remains.
- NPC damage events now present the same effects to observers, not only to the local shooter.
- Decal opacity still comes from `config/impact_decals.json`; there is no separate NPC opacity path in the current code.
- If visual opacity still differs, compare the actual effect source (predicted local hit vs confirmed remote hit) before changing config.

### 0.9 Current map-editor behavior

- Outliner lists cart origin, route points, all loaded GLB mesh objects, entities, and blocks.
- World picking supports GLB objects via their world bounds.
- Ctrl+click toggles selection; Shift+click selects a range in the outliner.
- PgUp/PgDn scroll the outliner.
- Moving a selected GLB object translates its vertex range and rebuilds collision through `buildCollisionMeshFromRenderMesh` plus authored geometry.
- Selected-object position/name/size/vertex count is displayed live.
- GLB object copy/cut/delete/paste and JSON persistence of GLB transform overrides are **not implemented**.
- RMB aim-FOV is disabled while the editor is active.
- Local death deactivates the editor and restores the player camera.

### 0.10 Verification commands and evidence

Canonical build/test:

```text
python devscripts/dev-loop.py --request build --wait-for-build
.dev/builds/<build>/mimita.exe --payload-selftest
.dev/builds/<build>/mimita.exe --payload-authoring-selftest
.dev/builds/<build>/mimita.exe --map-config-selftest
.dev/builds/<build>/mimita.exe --map-authored-geometry-selftest
.dev/builds/<build>/mimita.exe --npc-utility-selftest
.dev/builds/<build>/mimita.exe --team-brain-selftest
```

Runtime journals to inspect:

- `logs/2026-10-25/20261025_020711` — Payload 1, build 3165, 23 NPCs/model loaded.
- `logs/2026-10-25/20261025_020848` — Payload 2, build 3166, cart configured, 23 NPCs/model loaded.
- `logs/2026-10-24/20261024_032221` — earlier GLB cart mirror and push evidence.

### 0.11 Next todos, in order

1. Finish the NPC invisibility investigation with the draw-submission diagnostic; do not assume the model is missing.
2. Fix the Payload team-spawn collapse and add spawn-cluster diagnostics/invariant.
3. Author Payload 2 route/spawns against its actual geometry; do not ship the copied Payload 1 route as final content.
4. Make GLB object transforms persist as map JSON overrides and add duplicate/delete/paste coverage.
5. Fix the dev-loop `launch_failed` / `server_build_identity` regression so live verification is reliable.
6. Re-run the Payload acceptance sequence with human visual review.
7. Add remote-damage and decal-opacity visual checks to the acceptance list.
8. Add `payload2` to the allowed dev map pool only after its config is real.

---


## 1. Core game rules

Payload is an asymmetric team-based mode.

- **Team A (Offense):** Push a large bomb/cart up a tower to the final destination.
- **Team B (Defense):** Prevent Team A from completing the objective before the timer and overtime expire.
- Maximum of 12 actors per team, 24 combat actors total.
- Humans and NPCs can occupy team slots.
- Both teams respawn instantly throughout active gameplay.
- No halftime or mandatory side switching.
- After each match, the game automatically begins another round.
- The server keeps round/session statistics, including team victories and actor kills, in memory for this server process only. If the server process closes, these statistics are discarded. They are not saved across server restarts. Host migration is a later feature; do not promise that statistics or a match survive a host/server shutdown in v1.

The cart is physically present in the world and provides cover, healing, and a central combat objective.

## 2. Match phases

### Phase 1 — Intermission

**Duration: 30 seconds**, JSON-configurable.

- Players can join, explore, talk, and move around.
- Combat may occur during intermission but should not affect the upcoming round's outcome.
- Players can freely select Team A or Team B.
- NPCs fill unoccupied team slots.
- At the end, actors are returned to their team spawns for the countdown.

### Phase 2 — Countdown

- Display 3, 2, 1, GO.
- Everyone is frozen in their assigned spawn position before GO.
- On the same authoritative server tick, all players and NPCs are released.
- Round timer starts at GO.
- JSON-driven map events may occur at specified ticks, including doors opening or moving objects beginning their animations.

### Phase 3 — Active

- Team A pushes the cart toward its final destination.
- Team B defends and contests the cart.
- Players and NPCs fight continuously.
- Death causes immediate respawn.
- Checkpoints can add time to the clock.
- If the active round timer reaches zero, the exact overtime rules in Section 4 decide whether the round continues.
- The round ends with Team A's successful delivery or Team B's successful defense.

### Phase 4 — Results

**Proposed duration: 5–10 seconds**, configurable.

- Show the winning team.
- Show basic round statistics.
- Update server-session victories and actor statistics.
- Transition automatically into the next 30-second intermission.

## 3. Checkpoints and time

Four significant positions exist along the route:

| Position | Purpose |
|---|---|
| Checkpoint 1 | Starting position |
| Checkpoint 2 | First major milestone; adds 5 minutes |
| Checkpoint 3 | Second major milestone; adds 5 minutes |
| Checkpoint 4 | Final objective; Team A wins |

Initial time: approximately 5 minutes, configurable.

Each intermediate checkpoint awards additional time once.

Match length is not a fixed target. A match may be short or long depending on how the teams play. The initial clock, checkpoint bonuses, and overtime rules still need playtesting and final values; the numbers above are starting examples, not a promise that a match should last 15, 20, or 30 minutes.

## 4. Overtime

There is no automatic loss just because the timer hits zero while Team A is actively fighting for the payload.

V1 rule:

- When the active round clock reaches zero, enter overtime using the tick rules below.
- A living Team A actor intersecting the server-owned cart interaction box refreshes overtime once per server tick, as defined below.
- If the window expires, Team B wins.
- If the cart reaches the final destination, Team A wins.

Overtime behavior for v1:

- When the active round timer reaches zero, enter overtime and start the configured overtime window. The starting value is **10 seconds**, or **600 server ticks** at 60 Hz.
- On every overtime server tick, check the server-authoritative cart interaction box for living Team A actors.
- If at least one living Team A actor overlaps the box on that tick, refresh the overtime countdown to its configured full duration. This happens on every qualifying tick, even if a defender is contesting the box and preventing movement. Cart movement and overtime presence are separate checks: an attacker can keep overtime alive while defenders hold the cart still.
- If no Team A actor overlaps the box on a tick, decrease the remaining overtime by one tick. If it reaches zero before the cart reaches the finish, Team B wins.
- If the cart reaches the finish, Team A wins immediately.


## 5. Payload movement

The initial system is inspired by TF2's automatic proximity-based pushing.

- Team A actors whose collision body overlaps the configured cart interaction area can push it forward.
- Each nearby Team A actor adds **0.2 meters per second** of cart speed.
- Six or more nearby attackers produce the maximum speed of **1.2 meters per second**; additional attackers do not increase it.
- One attacker therefore moves the cart at 0.2 m/s; two at 0.4 m/s; three at 0.6 m/s; four at 0.8 m/s; five at 1.0 m/s; and six or more at 1.2 m/s.
- Movement acceleration and participant counts should be JSON-configurable.
- Team B actors whose collision body overlaps that same area contest it and prevent forward progress. One qualifying Team B actor is enough to block forward progress.
- An unattended cart eventually begins rolling backward.
- Suggested unattended delay: 30 seconds.
- Rollback proceeds toward the previous stable/checkpoint position.
- The cart should not roll backward indefinitely past its applicable rollback boundary.

**For v1:** The cart follows a predefined route rather than being freely driven.

### Cart origin and server-authoritative interaction box

The cart needs a small, exact 3D origin point: three world coordinates that mark its anchor. The cart model can have any visual size. Its route, collision, and interaction area should be positioned from the cart's origin, with editable offsets, rather than assuming the visible model's center is always the right point.

The interaction area is a **box**, owned and checked by the authoritative server. Its position and rotation are configured offsets from the cart origin, so the box follows the cart's transform instead of depending on the GLB's size or pivot. The first size estimate uses a cart about 2 m long, 1.5 m wide, and 1 m tall; making the interaction box 150% of those dimensions gives about 3 m × 2.25 m × 1.5 m. These are starting dimensions and remain JSON-configurable. An actor counts as inside when its collision body intersects the transformed box; checking only the actor's origin point is not sufficient.

Use existing collision math and map-entity data/loading where their contracts fit, but do not use weapon hitboxes to decide payload behavior. The payload still needs one clear interaction-area owner and a confirmed configuration loader. Exact JSON owner/key names and final box dimensions need to be confirmed during implementation planning. The actual GLB size must not silently change the gameplay area.

```text
each server gameplay tick:
    cartWorldOrigin = cart transform's authored origin
    interactionBox = configured box placed at:
        cartWorldOrigin + rotated(configured local offset)
    nearbyActors = living actors whose collision bodies intersect interactionBox
    attackers = nearby Team A actors
    defenders = nearby Team B actors

    if defenders is not empty:
        cartSpeed = 0
    else if attackers is not empty:
        pushCount = min(count(attackers), 6)
        speedMetersPerSecond = min(pushCount * 0.2, 1.2)
        distanceThisTickMeters = speedMetersPerSecond / 60
        advance cart along its configured route by distanceThisTickMeters
    else:
        apply the configured unattended-cart rule

    if overtime is active:
        if attackers is not empty:
            overtimeTicksRemaining = configuredOvertimeSeconds * 60
        else:
            overtimeTicksRemaining -= 1
            if overtimeTicksRemaining <= 0 and cart is not at finish:
                Team B wins
```

`configuredOvertimeSeconds` starts at 10 seconds (600 ticks) and is JSON-configurable. `pushCount` is capped at six for speed calculation. A defender in the box makes cart speed zero, but does not stop a living Team A actor in the box from refreshing overtime.

At 60 ticks per second, one attacker moves the cart about **0.0033 m per tick**; five attackers move it about **0.0167 m per tick**; six or more move it **0.02 m per tick**. The server stores overtime as an integer tick count; the HUD presents the remaining time in seconds.

This is a behavior sketch, not a claim that the current game already has a Payload cart, loader, or overlap query.

The cart cannot be damaged, destroyed, or repaired in v1.

Future designs may allow destruction, repairs, physical pushing, steering, and fully player-controlled vehicles.

## 6. Physical cart

The cart initially looks like a large, approximately 1-meter-tall box or simple bomb-carrying vehicle.

- Big enough to take cover behind while crouching or positioning.
- Not tall enough to fully conceal a standing player's head.
- Has working world collision.
- Players and NPCs can stand on top.
- The cart physically carries actors standing on it.
- Actors can shoot and fight while riding.
- Actors can jump off freely.
- No vehicle seats, driving interface, or attachments in v1.
- Movement and collision must behave consistently under server authority.

### Payload healing

Nearby friendly attackers receive healing from the cart. The earlier estimate of 20 HP to 100 HP in 10 seconds is superseded by the tick rule below.

**Starting rule:** Give a nearby friendly attacker 1 HP for every 10 server ticks. At 60 ticks per second, that is 6 HP per second, so going from 20 HP to 100 HP takes about 13⅓ seconds. Keep a fractional remainder between ticks so healing does not get lost to rounding, and never heal above the actor's normal maximum. This replaces the earlier 10-second estimate.

Whether defenders also receive payload healing remains undecided.

## 7. Tower map

**Proposed direction:** Create a dedicated Payload map rather than directly modifying the existing city map. The map has not been made yet, so its exact visual description and final geometry must come from building and playing it.

### Environment inspiration — not a finished map description

A huge ascending tower with a central shaft/hole. The payload climbs toward the final destination and drops into the hole, detonating or otherwise completing the objective.

Visual influences:

- Silent Hill 2/3 and PS2-era graphics.
- Y2K / early internet / weirdcore.
- Serial Experiments Lain-inspired atmosphere.
- Dark gray, faded blue, muted green, brown, yellow.
- Cold, desaturated Midwestern winter atmosphere.
- Breakcore, jungle, drum-and-bass and rhythm-game influences.

### Geometry ideas — confirm through the map blockout

- The long, shallow ramp is the main map idea: about 100 m long with an incline around 30 degrees. The exact angle reference and dimensions need to be checked while making and playing the map.
- Use simple gray boxes to plan the route and fighting spaces first. Then replace or reshape those boxes into finished, freeform geometry that keeps the intended route, cover, choke points, and movement choices.
- Some spaces may be around 10m × 10m and larger fights around 30m × 30m; these are examples to guide the blockout, not limits on the final map.
- Very large ramps and slopes.
- Multiple levels, ledges and vertical routes.
- Potentially around 10 floors, but not finalized.
- Central open shaft for the final payload drop.
- Elevated positions for defenders to attack downward.
- Cover suitable for both gunfights and melee ambushes.
- Freeform geometry: rocks, spheres, cones, triangles and irregular surfaces, not exclusively rectangular rooms.

Avoid mandatory waiting mechanics such as elevators that players must wait for before progressing.

The environment should encourage creative movement without artificially restricting skilled players.

## 8. Spawning

- Team A has one fixed spawn location.
- Team B has one fixed spawn location.
- Spawn points do not advance with checkpoints in v1.
- Both teams instantly respawn after death.
- Suggested spawn protection: 60 server ticks.
- Players retain MiMITA's fast traversal mechanics.

Spawn locations and protection should be editable in JSON.

## 9. Classes and actor presets

Players select classes without fixed class-count restrictions.

Initial proposed classes:

| Class | Weapons / identity |
|---|---|
| Scout | Revolver + shotgun, highly mobile |
| Soldier | Rocket launcher, rocket jumping |
| Demoman | Grenade launcher; possible sticky launcher |
| Heavy | Large machine gun, higher health and slower mobility |
| Melee fighter | Smash-inspired attacks, launches and combos |
| Sword fighter | Swordsword with Marth-inspired reach and movement |
| Chainsaw fighter | Extremely dangerous at close range, ambush-focused |

Some class definitions are still experimental.

Classes may share weapons or expose configurable loadout customization.

Avoid artificial class limits. Prefer inherent tradeoffs in movement, range, recoil, damage and positioning.

### Heavy machine gun

Sustained firing should naturally become harder to control because recoil accumulates.

A full team choosing Heavy should have exploitable weaknesses arising from the underlying gameplay, rather than being blocked by arbitrary class limits.

### Chainsaw

Possible sustained close-range damage of 10–30 per damage interval, subject to balancing.

Should feel dangerous and intimidating when an opponent is caught nearby, while allowing meaningful counterplay through positioning, anticipation or telegraphed attacks.

The exact damage interval and attack rules remain unresolved.

## 10. Combat rules

- Preserve MiMITA's existing shooting mechanics where practical.
- Weapon properties must support mode-specific overrides.
- Headshots enabled.
- Self-knockback and target-knockback enabled.
- Strong air control and movement.
- Fall damage disabled.
- Ragdolls enabled.
- Environmental/void kills enabled.
- Collision knockback permitted.
- Collision damage not required.
- Friendly fire is disabled in v1: attacks pass through teammates and deal no teammate damage. A possible later rule is 10% teammate damage, but that is not part of v1.

Melee should support powerful, visibly telegraphed attacks that can launch players away from the payload.

Knockback-caused environmental deaths should be credited to the responsible attacker where attribution is clear.

## 11. Movement

Preserve the character of MiMITA's fast movement:

- Dash and down-dash combinations.
- Air movement and directional control.
- Rocket jumping.
- Momentum preservation.
- Advanced movement techniques.
- High-speed traversal across the map.
- Movement speed, weight, and available movement options may differ by actor role/class. Roles should use the shared movement system with different presets, not separate movement code for each class.

Do not arbitrarily prevent players from reaching distant parts of the level.

The payload is the reason players need to return to the objective.

The mode should reward teamwork while providing readable teammate and objective information so players can understand the battle.

## 12. NPC behavior

NPC behavior and NPC movement are two different jobs:

- **Behavior chooses what the NPC is trying to do:** fight a visible enemy, defend a place, or help the team objective.
- **Movement works out how the NPC physically gets there:** choose a route, steer around walls, use ramps, and move using the same actor movement rules as players.

Payload needs both jobs to work together. For example, a Team A NPC may choose “help push the cart,” while the movement system finds a route to the cart. If an enemy attacks, behavior may choose “fight back”; after the threat is handled, it should return to helping with the cart. A Team B NPC may choose “block the cart” or “intercept attackers.” These priorities and when one interrupts another need their own detailed behavior rules.

**Current-code finding:** NPC combat, target selection, state choice, navigation, and shared actor movement have separate owners. The general game-mode code already runs a `TeamBrain` for team coordination and gives it bomb/site objectives for the current round-based objective modes. The NPC utility system can use that information to choose an objective goal. However, this does not prove Payload behavior: there is no Payload mode/configuration in `config/gamemodes/`, and the current objective code supplies bomb/site targets, not a moving cart route or push/block rules. Payload NPCs must not be described as already knowing how to push or defend the cart.

Fill all unused combat slots with NPCs.

For example, one human joins a 24-actor match and the remaining 23 actors are NPCs.

### Team A NPC priorities

- Travel toward the payload.
- Push it whenever useful.
- Respond to immediate threats.
- Defend themselves and nearby teammates.
- Resume pushing after combat.
- Use their selected class's weapons and movement abilities.

### Team B NPC priorities

- Locate and intercept attackers.
- Contest the payload.
- Attack advancing enemies.
- Use elevated positions, cover and appropriate combat range.
- Prevent progress until the timer expires.

These are desired goals, not proof that the current NPC code performs them in Payload.

### Navigation and stuck recovery requirements

Payload NPCs must navigate the actual walkable 3D map, including long shallow ramps, vertical changes, narrow passages, corners, irregular triangle geometry, and routes around cover. “A path exists” is not enough: an NPC must be able to follow it using the movement abilities allowed by its role and the same collision and fixed-tick movement rules as other actors. A route that requires an unavailable jump, dash, or traversal ability is not a usable route for that actor.

Keep these jobs distinct when investigating a stuck NPC:

1. **Goal:** Did behavior choose the right cart, defense, or combat goal?
2. **Route:** Did navigation return a route that is reachable for this role?
3. **Steering:** Did local avoidance choose movement toward the next useful waypoint?
4. **Action:** Did combat, waiting, or another higher-priority behavior intentionally override travel?
5. **Physics:** Did shared movement and collision produce the requested movement?
6. **Progress:** Did the NPC make useful progress toward its current goal over time?

An NPC is not stuck just because it pauses for one tick or makes little progress while intentionally defending, fighting, waiting for the cart, or moving sideways to dodge. Treat it as stuck only when it has an active travel goal, is not intentionally waiting or fighting in place, and its net progress toward that goal stays below a configured threshold for a configured time window. Measure progress toward the goal, not only raw movement: circling, moving into a wall, or moving sideways forever is not success.

Desired bounded recovery outline (pseudocode, not a claim about current behavior):

```text
each server simulation tick:
    goal = current authoritative NPC behavior goal
    if goal is absent or NPC is intentionally waiting/fighting:
        clear or pause this goal's no-progress timer
        continue

    record progress toward goal over the configured observation window
    if progress is sufficient:
        clear stuck episode and continue following the route
        continue

    if no-progress duration is below stuck threshold:
        continue

    mark one stuck episode and try the next bounded recovery step:
        1. adjust local steering around the immediate obstacle
        2. try a different nearby direction or valid traversal action
        3. invalidate the failed route and request a new route
        4. choose a different reachable approach to the same goal
        5. if all attempts fail, report route failure and wait for a
           meaningful change before retrying

    never teleport, pass through solid geometry, repeat the same failed
    route/traversal forever, or silently discard the team objective
```

Recovery attempts must be bounded and observable. Record the actor, active goal, position, route/waypoint, role movement abilities, chosen recovery step, and whether useful progress resumed. If recovery fails, keep the objective available so behavior can choose another task or retry after the map, goal, or obstruction changes. A persistent failure should be diagnosable as goal selection, route construction, local steering, traversal, collision, actor crowding, or networking/state replication rather than appearing as an unexplained frozen NPC.

### Map-authored navigation hints — open design choice

Generic navigation should handle ordinary map geometry without requiring a designer to place escape points everywhere. A map-authored “go out this way” hint or recovery volume may help with a known trap, one-way route, or intentional choke, but it is not yet an established Entity Editor feature. **NEEDS_SPEC_DECISION:** decide whether the editor should support named navigation hints, what their volume and exit target mean, which NPC roles may use them, and how they are validated. If adopted, hints should guide route choice only when relevant; they must not teleport actors or replace normal collision and route validation. Keep this question linked to the Entity Editor and NPC movement specifications.

The map must be played with NPCs on routes that include ramps, narrow spaces, corners, dead ends, and the cart approach. Acceptance requires that NPCs reach the objective when a valid route exists, recover from a temporary obstruction without looping, and stop retrying/report a bounded failure when no valid route exists. Include cases where combat interrupts travel and the NPC resumes its previous objective afterward.

## 13. Team selection and spectators

Commands should include:

- `start payload`
- `team_list`
- `team_pick 1` — Team A
- `team_pick 2` — Team B

During intermission, team changes are unrestricted.

During an active round, changing teams should have an approximately 5-minute cooldown.

Admins/hosts may override that restriction.

Spectators should receive the necessary game state without simulating a combat actor or sending continuous player movement updates.

Connection keepalives and intentional spectator camera/control commands remain permitted.

## 14. Networking

The authoritative server handles:

- Phase timing and transitions.
- Payload position and movement.
- Objective progress.
- Contesting actors.
- Damage, knockback and kills.
- Checkpoints and overtime.
- NPC decisions and movement.
- Session statistics.

The server decides cart movement and all match results. Clients may ask to push, but the server checks the actor's team, collision body, and overlap with the configured interaction area before changing the cart.

Current implementation uses two replication paths:

- The cart body is a replicated kinematic `PhysicalEntity`; late joiners receive a spawn replay.
- Cart progress, contest counts, overtime ticks, cart state, and team HUD data travel in `DuelStatePacket` protocol 45.

This client/server rule also applies when one person plays alone on one computer: the game client still sends normal gameplay requests to a local server process, and that server still owns NPC decisions, navigation, movement, cart state, collision, and match results. There must not be a separate offline-only gameplay implementation whose rules differ from multiplayer. Clients may predict presentation where the general networking design permits it, but the server's accepted state is authoritative and is what clients render or reconcile to. See `../networking/networking.md` for the shared connection and authority contract.

### Host handoff and session statistics

The networking specification describes host migration as future work, not a working feature. Choosing the player with the lowest ping is not enough: the replacement host needs the server's current match state, including the tick, random state, actors, NPCs, game-mode state, score, timers, and pending events. The networking spec describes checkpoints and a possible handoff flow, but the current Payload spec must not promise that a match survives when its host closes the server.

Host migration is future work and is not a v1 requirement. If the server process closes, the match and its in-memory session statistics end and are discarded. Do not save Payload's round/session statistics through the generic persistent match-result path. If host migration is added later, deciding which match state and statistics transfer is part of that separate feature.

## 15. HUD and player information

The player should know:

- Where the payload is.
- Its progress toward the final objective.
- Whether it is advancing, blocked or rolling backward.
- How many attackers and defenders are nearby.
- Remaining round time and overtime status.
- Checkpoint locations.
- Where teammates are.
- Which enemies are visible or have revealed themselves.
- What class and weapon an enemy appears to be using.
- Whether shots and melee attacks are connecting.

Initial UI concepts:

- Payload locator visible through walls.
- Timer at the top of the screen.
- World-space glowing checkpoint markers.
- Cart progress and contesting status displayed on or near the cart.
- Tab-held scoreboard showing teams, kills and other statistics.

## 16. Presentation

Art direction: PS2 atmospheric environments combined with Melee-inspired combat readability and high-impact visual feedback.

- Strong character visibility against muted geometry.
- Preserve user-generated avatar appearances.
- Experiment with client-side lighting/contrast rather than mandatory body outlines.
- High-quality hit sparks and directional hit reactions.
- Strong weapon/arm animation and recoil.
- Relatively weak camera recoil.
- Knockback and movement effects.
- Optional small damage numbers.
- Blood, ragdolls and satisfying impacts.
- Clear audio for weapons, objective changes and combat.

Music and announcer/voice presentation require a separate specification.

Potential future work includes configurable procedural speech, including pitch, speed and volume.

## 17. JSON configuration

The game mode should be composed from reusable JSON-controlled systems:

- Match phases and timing.
- Team definitions.
- Actor and class presets.
- Loadouts and weapon overrides.
- Movement parameters.
- Damage and knockback.
- Presentation presets.
- Spawn points.
- Payload path and checkpoints.
- Map triggers and tick-scheduled events.
- NPC behavior configuration.
- HUD configuration.
- Cart origin and cart interaction-area shape, size, local offset, and rotation.

Presets should be reusable and versionable without rewriting unrelated modes.

Avoid one enormous weapon-definition file. Prefer independently managed weapon definitions, with game-mode-specific overrides.

The files listed by the JSON architecture document are a map of configuration families, not proof that Payload settings already have a loader. Before adding a setting, identify the active file and the code that reads it. Do not create a second copy of the same setting in both a map file and a game-mode file without deciding which one owns it.

### Current Payload JSON files

| File | Owner | Current keys |
|---|---|---|
| `config/gamemodes/payload.json` | Gamemode loader | `teams`, `spawn_groups`, `spawn_strategy`, `rounds`, `payload.*`, `npc_targeting`, `npc_travel`, `presentation` |
| `config/maps/payload1.json` | Map config loader | spawnpoint entities, `payload.cart_origin`, `cart_size`, `facing_mode`, `interaction_box`, `route[]` |
| `config/maps/payload2.json` | Map config loader | starting copy of Payload 1; needs map-specific authoring |
| `config/gui/gamemode-meta-gui.json` | GUI layout loader | `gamemodes.payload` HUD elements |
| `config/roles.json` | Role registry | `payload_attacker`, `payload_defender`, and class roles |
| `config/weaponsets.json` | Weapon set registry | `payload_*` sets |

## 18. Map-building philosophy

The tower ramp is the main Payload map idea, but this document does not pretend the finished map already exists. First build a simple blockout so people can try the route, fight around the choke point, and test whether the cart is fun to defend and push. Then replace the plain boxes with finished shapes while keeping the useful routes and fighting spaces.

The map should support fast movement and several ways to approach or leave the objective. A choke point can make a good fight, but the map should not force every player to stand still or wait for a lift. The map's JSON and Entity Editor should own editable map details such as spawn points and placed entities; the GLB should provide world geometry and visuals. See [`mapping.md`](../../architecture/vision-mentality/mapping.md) for the broader map-making philosophy and [`entity-editor.md`](../entity-editor/entity-editor.md) for map JSON and entity editing. The exact map route and final geometry need to come from building and playing the tower map.

## 19. Future mode scripts

**Long-term direction:** Let a creator write a Lua script for a game mode or a small game, and let that script call existing, approved game functions. The engine remains the owner of physics, movement, networking, damage, and other core rules. A script asks the engine to do supported actions; it does not become unrestricted access to the C++ engine or let a remote player run arbitrary code on the server.

JSON should continue to own simple data such as numbers, routes, timings, and lists. A script is for rules and sequences that are awkward to express as data alone. The game needs a defined safe script sandbox, an approved function list, server/client authority rules, error handling, and a way to stop a bad script. Those details belong in the separate [`Code Wrapper` specification](../code-wrapper/code-wrapper.md). Lua is the preferred direction expressed here, but the exact language and safe API still need design work. This is a future capability, not a Payload v1 requirement.

## 20. Payload v1 acceptance experience

A successful v1 should support this playable sequence:

1. Start Payload.
2. Join a team with `team_pick`.
3. Fill unused slots with NPCs.
4. Run a 30-second intermission.
5. Teleport both teams to their spawns.
6. Show synchronized 3, 2, 1, GO.
7. Release all actors on GO.
8. Start the round timer.
9. Attackers push the cart.
10. Defenders contest and fight.
11. The cart carries standing actors.
12. Players use their class weapons, movement and melee.
13. Killed actors instantly respawn.
14. NPCs independently perform meaningful offensive/defensive behavior.
15. Checkpoints award their configured time bonuses.
16. An unattended cart can roll backward.
17. Overtime works correctly.
18. Team A wins by delivering the cart or Team B wins by defending.
19. Results and session statistics display.
20. A new round begins automatically.

The same sequence should work in repeated tests without accumulating incorrect state or breaking existing MiMITA modes.

### Current acceptance status (2026-10-25)

- Implemented in code: start, roster, phases, countdown/GO, server timer, cart movement, contest, carry, class weapons, instant respawn, NPC cart objective, checkpoints, rollback, overtime, win/loss, results.
- Reproduced in runtime: cart config/spawn/push, late-join cart mirror, 23 remote NPCs with models loaded, Payload 2 map identity.
- Not yet accepted: NPC visual visibility, team-separated spawn clusters, remote damage-number visibility, decal-opacity parity, ragdoll appearance, Payload 2 content correctness, and the full 20-step sequence on one journal.
- The provided acceptance list is intentionally not marked fully passed until a human watches the sequence and the open defects in Section 0 are closed.


## 21. Follow-up files to review

This is a review list, not a promise that every file must change. First find each feature's current owner. Edit a file only when it owns a rule Payload needs or when an audit finds a real gap.

### Specifications and configuration

| File | Why it is related | What to check |
|---|---|---|
| [`gamemodes.md`](gamemodes.md) | Shared match phases, teams, spawns, and mode-neutral NPC objectives. | Make Payload fit the shared mode system instead of adding a Payload-only branch for a reusable rule. |
| [`../../specs/networking/networking.md`](../../specs/networking/networking.md) | Server authority, state replication, disconnects, and host migration. | Find what match state is actually replicated and whether any host handoff exists. |
| [`../entity-editor/entity-editor.md`](../entity-editor/entity-editor.md) | Map JSON, placed entities, spawn points, and editing without rebuilding a GLB. | Decide where the cart origin, route, and interaction-area settings belong. |
| [`../../architecture/vision-mentality/mapping.md`](../../architecture/vision-mentality/mapping.md) | General map-making philosophy. | Explain the blockout-first, playtest, then detailed-geometry process for maps. |
| [`../npc-behavior/npc-behavior.md`](../npc-behavior/npc-behavior.md) | What an NPC decides to do. | Add objective goals such as push, block, defend, and return after a fight. |
| [`../../architecture/player-npc-systems/npc-movement.md`](../../architecture/player-npc-systems/npc-movement.md) and [`../movement/movement.md`](../movement/movement.md) | How an NPC follows a goal and which movement rules actors share. | Check routes over ramps, role-specific movement presets, and how movement stays shared with players. |
| [`../../architecture/collision/collision.md`](../../architecture/collision/collision.md) | World and actor collision ownership. | Find the reusable way to test whether an actor overlaps the cart area. |
| [`../../architecture/json-configuration/json-configuration.md`](../../architecture/json-configuration/json-configuration.md) and [`../config-load/config.load.md`](../config-load/config.load.md) | Where JSON settings live and how they are loaded. | Confirm the real loader before naming or adding a Payload configuration file. |
| [`../code-wrapper/code-wrapper.md`](../code-wrapper/code-wrapper.md) | Long-term scripted game modes. | Specify the safe Lua sandbox, approved engine functions, and server/client rules. |
| [`../vehicles/vehicles.md`](../vehicles/vehicles.md) | Future drivable Payload and reusable vehicles. | Keep v1's route-following cart separate from later driving and vehicle work. |
| [`../weapons/weapons.md`](../weapons/weapons.md), `config/weapons.json`, and `config/weaponcollisions.json` | Class weapons, weapon overrides, and collision configuration examples. | Keep shared weapon rules reusable and investigate how to organize definitions without expanding one huge file. |
| [`../gui/guiv2.md`](../gui/guiv2.md), [`../gui/gui-general.md`](../gui/gui-general.md), and `config/gui/hud.json` | HUD ownership and editable player information. | Find which HUD file is actually active before changing Payload's display. |
| [`../live-voice-generation/live-voice-generation.md`](../live-voice-generation/live-voice-generation.md) | Voice and announcer features. | Keep voice generation in its own system and link to it from Payload. |

The router mentions `docs/specs/gui/gui.md`, but that exact file is not present in this checkout. The current GUI documents found during this review are `guiv2.md` and `gui-general.md`.

### Code owners to inspect before implementation

| File or group | Current job | Payload question |
|---|---|---|
| `src/network/server-gamemode.cpp` and `src/network/server-gamemode.h` | Runs the server's shared game-mode lifecycle and current TeamBrain updates. | Where should server-owned cart route, phase, checkpoint, and result state live? |
| `src/network/server-npcs.cpp` | Connects NPCs to the authoritative server. | How are Payload NPCs spawned and given the correct team and role? |
| `src/network/community-match-client.h` and `src/network/community-match-client.cpp` | Carry and apply current replicated objective state. | Can the existing generic objective state describe a moving cart, or does it need a general extension? |
| `src/npc/team-brain.cpp` and `src/npc/team-brain.h` | Shares team information and chooses team-level objective/focus positions. | Can the general team planner support a moving Payload objective and different attacker/defender jobs? |
| `src/npc/npc.cpp`, `src/npc/npc-state-machine.cpp`, `src/npc/npc-states.cpp`, and `src/npc/npc-combat.cpp` | Builds NPC senses, selects actions, and uses the shared combat rules. | How should objective work pause for danger and resume afterward? |
| `src/npc/npc-targeting.cpp`, `src/npc/npc-navigation.cpp`, `src/npc/npc-navigator.cpp`, `src/npc/npc-nav-request.cpp`, `src/npc/recast-navigation.cpp`, and `src/npc/npc-traversal.cpp` | Finds targets and routes, avoids obstacles, and performs traversal. | Can NPCs reach the cart across the tower's ramps and alternate paths? |
| `src/physics/movement/actor-pair-collision.cpp` and the shared actor movement code | Resolves physical contact between actors and their movement. | Which existing collision query can test cart-area overlap without making a duplicate hot-loop system? |

### Current-code reuse audit (source inspection, 2026-10-22)

This is a source-level ownership audit, not proof of live Payload behavior. The references below show current code paths and reusable pieces; runtime integration and gameplay still need verification.

| Existing owner | Reusable foundation | Current gap and work Payload needs |
|---|---|---|
| `src/network/server-gamemode.cpp` (`serverGamemodeTick`, `serverGamemodeState`) and `src/network/server-gamemode.h` | The server already owns the fixed-tick game-mode lifecycle, phases, round state, and team statistics. Use this authoritative lifecycle and its existing state owner. | Generic match-time expiry currently transitions to results. Payload needs its own overtime decision, route/checkpoint progress, contest and push calculation, and finish result, integrated through the shared mode owner. Do not create a second independent match loop. |
| `src/gamemode/gamemode.cpp`, `src/gamemode/gamemode.h`, and `src/game/objective-state.*` | Game-mode JSON already loads shared time and objective definitions; the generic objective state and replication envelope can inform a Payload extension. | The objective kind parser currently recognizes Bomb only, and the objective self-test expects `payload` to resolve to None. The generic objective does not contain route progress, push/block state, checkpoints, or Payload overtime. Add a deliberate shared objective contract before extending packets or adding a Payload-specific state path. |
| `src/gamemode/map-config.*` | `MapConfigRegistry` loads, saves, and reloads map JSON. Map entities already have positions and sizes. Reuse this established map-data owner if cart placement and route belong to map data. | `mapEntityContainsPoint` checks a point against an axis-aligned box or radius; it does not check an actor collision body against a rotated box. The current map entity shape has no cart origin/route/interaction offset contract. Add the needed schema and a suitable overlap query, or select another single owner. |
| `src/physics/physical-entity.h`, `src/physics/physical-entity.cpp`, `src/network/physical-entity-replication.cpp`, and `src/network/multiplayer-physical-entities.cpp` | `PhysicalEntitySystem` supports kinematic entities, collision geometry, and moving them. Existing moving-entity behavior includes actor support/carrying, and physical-entity replication already exists. This is the strongest candidate for the cart's physical body and server-authored movement. | It is not yet wired to Payload route/configuration, objective phase/results, or cart interaction rules. Verify dedicated/listen-server creation and replication ownership before using it as the cart implementation. Its source self-test is not live-game proof. |
| `src/npc/team-brain.*`, `src/network/server-gamemode.cpp`, and `src/npc/npc-*` | TeamBrain shares team information and supports coordinated objective/focus choices; NPC behavior and navigation already have separate owners. Reuse those decision and movement systems. | Current game-mode context supplies bomb/site information, not a moving cart target or push/contest roles. Provide a moving cart objective to the shared planner, define attacker/defender priorities, and ensure NPCs resume their objective after combat. Route following over real tower geometry needs map playtesting. |
| `src/network/server-gamemode.cpp` and `src/persistence/persistence-queue.cpp` | Current team/kill session values live in server game-mode state, which matches Payload's v1 process-only statistics. | Generic match completion also emits a persistent match result and queues a persistence save. Payload's temporary round/session counters must not be sent through that path. On server process shutdown, v1 state is discarded; host migration remains future work. |
| `src/network/packets.h` and `src/network/server-gamemode.cpp` objective replication | Existing objective state already carries a position and basic status, which may be enough to avoid inventing a wholly separate cart presentation protocol. | It has no confirmed Payload contract for route progress, checkpoint, overtime, contested state, or display data. First list the client-visible fields required by the HUD and interpolation, then inspect packet size limits and serialization before changing packet layouts. |

### Payload-specific implementation work identified by the audit

1. Add Payload configuration through the confirmed game-mode or map-data loader, with one owner for cart origin, route/checkpoints, interaction-box dimensions/offset/rotation, push rate, cap, overtime duration, and unattended-cart behavior.
2. Advance the cart only on the server's fixed 60 Hz tick. Count living Team A and Team B actor collision bodies intersecting the transformed box. One defender sets cart speed to zero. Otherwise, attackers produce `min(attackerCount, 6) * 0.2 m/s`, capped at `1.2 m/s`; convert that speed to distance per tick by dividing by 60.
3. Keep overtime presence separate from motion: each tick with at least one living Team A actor in the box resets the configured countdown, even while a defender contests. With no Team A actor in the box, decrement by one tick. Finish wins for Team A; expiry wins for Team B.
4. Decide and document the replicated cart state needed by clients, then reuse or extend the existing objective/physical-entity replication after packet size and receive/apply paths are traced.
5. Feed the current cart position and team-specific jobs to NPC planning. Keep behavior choices in NPC behavior and route following/traversal in NPC movement/navigation; do not duplicate navigation in Payload code.
6. Keep v1 statistics in server process memory and keep them out of the persistent match-result path. A server shutdown ends the match and discards those values.

`config/gamemodes/payload.json`, `config/maps/payload1.json`, and `config/maps/payload2.json` now exist and are loaded by the shared gamemode/map-config owners. Payload-specific behavior still belongs in those shared owners; do not add a parallel cart/map loader. See Section 0 for the current implementation handoff.

---

## 22. Handoff checklist — next implementation pass

### P0 — NPCs are still reported invisible

Acceptance: a human stands in each team spawn area and sees allied and enemy NPCs at the expected positions.

Investigate with:

- `client.npc.viewport-window` (`remote_npcs`, `alive`, `model_loaded`, `in_view`, map, camera, sample NPC).
- A one-time `renderNetworkPlayer` submission log with modelLoaded, part-mesh count, position, camera distance.
- Snapshot membership if `remote_npcs` is lower than the server roster.

Do not assume the model is missing. Current evidence shows 23 remote NPCs and 23 models loaded.

### P0 — Team A and Team B spawn separation

Acceptance: attacker and defender NPC clusters are separated by the authored spawn groups; no actor with a valid team falls back to the shared anchor.

Add diagnostics to `gamemodeSpawnPoint`/`gamemodeSpawnPointForActor`:

- actor id, role, resolved team
- spawn group
- candidate count
- chosen position
- fallback reason

Add a Payload spawn selftest using the real mode and map JSON. Do not add mode-name branches.

### P1 — Payload path authoring for Payload 2

- Re-author `config/maps/payload2.json` against Payload 2 geometry.
- Verify `cart_origin`, `cart_size`, `facing_mode`, interaction box, and every route point.
- Add route points through the map editor and save them to JSON.
- Keep route points visible in the editor.

### P1 — GLB object editing persistence

- Persist selected GLB object transforms as map JSON overrides.
- Add duplicate/cut/delete/paste coverage.
- Keep using `buildCollisionMeshFromRenderMesh` and `buildCollisionChunks`; do not add a second GLB collision path.

### P1 — Remote damage and opacity visual acceptance

- Observe AI-vs-AI and remote-player-vs-AI hits.
- Confirm damage numbers appear for hits the local player did not fire.
- Compare NPC and player decal opacity at the same distance and weapon.

### P2 — Dev-loop launch reliability

The dev loop entered `launch_failed` with `server_build_identity`; manual server startup worked. Fix the preflight/identity path before using the dev loop as the only runtime evidence source.

### P2 — Full acceptance run

Run Section 20 end to end on the requested map, one journal, and record:

- requested map vs server map vs client map
- red/blue team rosters
- spawn cluster positions
- cart route completion or overtime result
- NPC visibility and combat behavior
- human visual acceptance

---

## Future features explicitly excluded from the initial playable version

- Player-controlled driving.
- Freely physics-pushed carts.
- Cart destruction and repairs.
- Fully simulated wheels and suspension.
- Multiple drivable vehicle types.
- Extensive dynamic world destruction.
- Advanced presentation systems not required for v1.
- Automatic host migration, unless a working reusable system already exists.

These remain documented future goals rather than being discarded.

**Specification status:** Gameplay direction established. Exact formulas, implementation ownership, JSON schemas, numerical defaults, map dimensions, and tests still require engineering refinement. This document records desired behavior and research needs; it does not claim these features already exist.

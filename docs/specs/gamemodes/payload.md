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

**Status:** Design draft, not yet approved for implementation  
**Target:** 12v12 objective-based combat, including NPC fills  
**Design philosophy:** Make the game playable first, then improve it through repeated testing. Preserve the freedom of MiMITA's movement system.

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
- The server should track statistics during the match session, including team victories and actor kills. Whether those statistics survive a host leaving, a server closing, or a restart is still open; do not describe them as safely persistent until that behavior is checked and specified.

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
- If the timer reaches zero, overtime may keep the round running.
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

Proposed rule:

- Once the clock reaches zero, enter overtime.
- If the cart was recently pushed or touched by Team A within a configurable window, keep the match alive.
- Suggested initial window: 10 seconds, measured using authoritative server ticks.
- Every qualifying offensive interaction can refresh the overtime window.
- If the window expires, Team B wins.
- If the cart reaches the final destination, Team A wins.

Exactly which interactions refresh overtime requires a precise definition.

## 5. Payload movement

The initial system is inspired by TF2's automatic proximity-based pushing.

- Team A actors whose collision body overlaps the configured cart interaction area can push it forward.
- More nearby attackers produce greater speed.
- Approximately six attackers are required for maximum speed.
- Maximum intended speed: approximately 1 meter per second.
- One attacker pushes much more slowly.
- Movement acceleration and participant counts should be JSON-configurable.
- Team B actors whose collision body overlaps that same area contest it and prevent forward progress. One qualifying Team B actor is enough to block forward progress.
- An unattended cart eventually begins rolling backward.
- Suggested unattended delay: 30 seconds.
- Rollback proceeds toward the previous stable/checkpoint position.
- The cart should not roll backward indefinitely past its applicable rollback boundary.

**For v1:** The cart follows a predefined route rather than being freely driven.

### Cart origin and interaction area

The cart needs a small, exact 3D origin point: three world coordinates that mark its anchor. The cart model can have any visual size. Its route, collision, and interaction area should be positioned from the cart's origin, with editable offsets, rather than assuming the visible model's center is always the right point.

The interaction area is editable map/game-mode data. Its position and rotation are offsets from the cart origin. The first size estimate uses a cart about 2 m long, 1.5 m wide, and 1 m tall; making the interaction area 150% of those dimensions gives about 3 m × 2.25 m × 1.5 m. If an actor's collision body touches or enters the area, the actor counts as being near the cart.

The area should use the existing collision configuration approach where practical, including the same clear ideas of a shape, dimensions, and local offset used for weapon collision configuration. This does not mean a weapon hitbox should decide payload behavior. The payload still needs one clear interaction-area owner and a confirmed configuration loader. Whether the area is a box or another shape, how rotation affects it, and whether its anchor is the authored origin or a center-of-mass point remain open choices. The actual GLB size must not silently change the gameplay area.

```text
each server gameplay tick:
    cartWorldOrigin = cart transform's authored origin
    interactionShape = configured shape placed at:
        cartWorldOrigin + rotated(configured local offset)
    nearbyActors = actors whose collision bodies overlap interactionShape
    attackers = nearby Team A actors
    defenders = nearby Team B actors

    if defenders is not empty:
        block forward cart movement
    else if attackers is not empty:
        advance cart along its configured route
    else:
        apply the configured unattended-cart rule
```

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

NPCs should navigate irregular maps, not only flat rectangular spaces.

They should be capable of jumping, using ramps and traversing freeform triangle geometry.

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

### Host handoff and session statistics

The networking specification describes host migration as future work, not a working feature. Choosing the player with the lowest ping is not enough: the replacement host needs the server's current match state, including the tick, random state, actors, NPCs, game-mode state, score, timers, and pending events. The networking spec describes checkpoints and a possible handoff flow, but the current Payload spec must not promise that a match survives when its host closes the server.

Before implementation, check whether any reusable host-migration code exists and what state it can actually move. Then decide whether the first version ends the match when the server closes, or whether a separate host-migration project is required. Also decide what “session statistics” means: only while this server process is alive, saved across reconnects, or saved across server restarts. These are different promises.

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

`config/gamemodes/payload.json` does not exist in the current configuration list. It is a possible future data file, not a decided path. Confirm the game-mode loader and ownership first. The same rule applies to any new cart or map JSON file.

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

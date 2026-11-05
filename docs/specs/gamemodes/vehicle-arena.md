# Vehicle Arena

**Purpose:** define the ball, goal, scoring, and match rules for a
Rocket-League-style vehicle arena mode.
**Audience:** gamemode, vehicle, physics, networking, map, HUD, and replay
contributors.
**Status:** desired behavior; a Rocket League comparison baseline is recorded,
while MiMITA arena tuning and first implementation remain pending.
**Does not:** add a second vehicle physics solver, require vehicle-mounted
weapons, or replace the shared gamemode lifecycle.

## 1. Scope

Vehicle Arena is a mode on top of the shared vehicle library. It owns:

- the arena match phases and team rules,
- the ball,
- goal volumes,
- scoring and results,
- arena-specific HUD and reset behavior.

It does not own vehicle suspension, tires, drivetrain, collision, or
replication. Those remain in `docs/specs/vehicles/vehicles.md` and the shared
physical-object owners.

## 2. Core loop

Two teams drive cars in a walled arena. A ball starts at the arena center. Teams
push, hit, or carry the ball toward the opposing goal. A goal scores for the
attacking team, resets the ball, and returns cars to their kickoff positions.
The match ends at the configured score or time limit.

The first playable milestone is one stable, authored arena, one physical ball,
two teams, kickoff, goals, scoring, resets, and a score/timer HUD. Vehicle
mounted weapons, NPC drivers, primitive vehicle construction, live map editing,
and generated arena modules follow this focused mode. Existing player-held
weapons remain a separate sandbox/custom-rule option, not a default arena rule.

## 3. Arena and goal data

Arena data belongs to the map and mode configuration, not vehicle code:

- arena bounds and walls use authored map geometry,
- goal volumes are authored zones with a team owner, center, half-extents or
  radius, facing direction, and enabled flag,
- kickoff positions are authored spawn points with a team tag,
- the ball spawn is an authored point or the arena center fallback.

A goal counts when the authoritative ball center crosses the goal plane or
enters the goal volume on the scoring tick. Visual goal geometry may be larger
than the scoring volume; the scoring volume is the authority.

## 4. Ball

- The ball is one dynamic shared `PhysicalEntity` with a stable network id.
- Physics use the shared mass, collision, restitution, friction, and impulse
  path. A sphere may be tessellated for triangle collision; it must not get a
  mode-only collision solver.
- The server owns ball motion, goal detection, and reset.
- Clients receive spawn/despawn/state through the shared physical-entity
  replication path.
- Ball mass, radius, restitution, and drag are mode or map data.
- A goal reset returns the ball to its spawn with zero velocity and no residual
  contact or sleep state.

## 5. Cars in the arena

- Cars use shared vehicle templates and the shared fixed-tick simulation.
- Car mass, suspension, grip, acceleration, and top speed come from templates.
- Boost, jump, air control, and the arcade control profile are selected through
  mode/template data on the shared vehicle input and physics owners. They do
  not introduce an arena-only car controller.
- Car contact uses the shared physical contact path. The client may predict its
  immediate car-ball response; the server confirms the authoritative ball
  state and scoring under the networking contract.
- Human playtesting must accept steering response, recovery, boost, jump, and
  aerial control. The reference table below guides tuning but does not replace
  the recorded playtest decision.
- Car destruction and vehicle-mounted weapons are not required for the first
  playable arena.

## 6. Match lifecycle

The mode uses the shared gamemode lifecycle:

`WAITING -> INTERMISSION -> COUNTDOWN -> GO -> ACTIVE -> RESULTS -> INTERMISSION`

The mode declares:

- team names and capacities,
- score goal,
- time limit and timeout outcome,
- countdown and intermission seconds,
- kickoff layout and spawn assignment,
- overtime or tie behavior when decided,
- whether the match resets the ball and cars on each goal.

A goal during the final active tick still counts if the server accepts it before
the phase transition, matching the shared score-limit ordering rule.

## 7. Scoring and HUD

- The server is the only scoring authority.
- A goal event records tick, scoring team, scorer when known, ball position,
  goal id, and the score before/after.
- Own goals score for the opposing team.
- The HUD shows both team scores, authoritative active timer, goal
  flash/reason, and match phase, following the shared
  [Objective and Round HUD contract](objective-hud.md).
- Killfeed-style goal text may use the shared killfeed/presentation owner, but
  scoring data remains mode state.
- Results show the winner, final score, and reason (`score`, `time`, or
  `forfeit`).

## 8. Networking acceptance

- Two clients can complete the full match and receive the same authoritative
  match phase, score, timer, goal, and reset state.
- Both clients predict their local car control; car-ball contact prediction is
  reconciled to the server result without applying a confirmed impulse twice.
- Contested 50/50 hits converge to the same confirmed ball outcome on both
  clients. Test with simultaneous opposing approaches and compare first-contact
  tick, contact point, pre-contact velocities, predicted result, and correction.
- Clients agree on ball position within the established physical-entity
  interpolation tolerance after reconciliation.
- A goal is confirmed once by the server; clients do not score locally.
- The same client is never awarded two goals for one crossing.
- Car contact with the ball produces the same visible outcome on all clients.
- A late or duplicate state packet cannot reverse a confirmed goal or score.
- A missing ball or goal definition fails closed with a diagnostic; it does not
  invent a default scoring zone.

## 9. Reference tuning

### 9.1 Rocket League reference baseline

This is a comparison baseline, not a claim that Epic published Rocket League's
physics constants. Epic's support page confirms six hitbox classes and says
cars share stats; numeric dimensions and physics values below come from
community extraction or reverse-engineered simulators. Treat those values as
starting evidence, not authoritative source code. The extracted database
identifies build `25914621`; its hitbox page says it was updated 2026-10-21 and
its physics page says 2026-11-04. Both pages were retrieved 2026-11-04.
RocketSim `main` was retrieved the same day but is not pinned to a commit. Keep
uncertain or conflicting values flagged until measured against the game.

Rocket League game distances are Unreal units (UU), with 100 UU = 1 m; retain
UU values when comparing curves and convert to SI for MiMITA's metre-based
vehicle templates. Speeds are UU/s. RocketSim notes that force/impulse values
use simulator-specific engine scales, so those raw numbers must not be copied
into MiMITA as Newtons. Confidence describes confidence in the cited reference
or transcription, not official endorsement.

| Behavior or parameter | Rocket League value | Units | Source/build | Confidence | MiMITA comparison |
|---|---|---|---|---|---|
| Body classes and shared stats | Six classes: Breakout, Dominus, Hybrid, Merc, Octane, Plank; cars share stats and differ by hitbox | Classes | [Epic support](https://www.epicgames.com/help/c-37599050/c-7261959845275/a20257614?lang=en-US), retrieved 2026-11-04 | High for published statement | This baseline selects Octane; MiMITA sedan is a separate template |
| Octane hitbox dimensions | 118.01 × 84.20 × 36.16 = 1.1801 × 0.8420 × 0.3616 | UU (cm) × m; length × width × height | [rl.source.gs hitboxes](https://rl.source.gs/hitboxes/), build 25914621, updated 2026-10-21; retrieved 2026-11-04 | Medium; extracted, not official | `car_sedan.json`: 5.0 × 3.0 × 1.0 m body dimensions; compare collision shape separately |
| Hitbox shape, orientation, and visual offset | A box; base Octane box sits 13.88 UU forward and 20.75 UU above car centre (0.1388 m, 0.2075 m); pitch angle not published | Shape; UU/m; degrees | [rl.source.gs Octane body](https://rl.source.gs/items/bodies/octane/), build 25914621, updated 2026-10-21; retrieved 2026-11-04 | Medium for reported offset; pitch unknown | MiMITA collision shape and offset need an explicit arena template measurement |
| Car mass and center of mass | Mass 180; center of mass not listed | Community mass units; COM unknown | [rl.source.gs](https://rl.source.gs/physics/), build 25914621, updated 2026-11-04 | Medium for mass; unknown COM | `car_sedan.json`: 2,600 kg, COM [0, 0, -0.35] m; units/mass are not directly equivalent |
| Speed caps | Car 2,300 (supersonic threshold 2,200); ball 6,000 | UU/s (23, 22, 60 m/s) | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium-high, community extracted | Sedan target is 999 km/h (configuration cap is not evidence of achieved speed); measure arena car cap |
| Gravity and standard arena scale | Gravity Z −650; half extents X 4,096, Y 5,120 (excluding inner goal), height 2,048 | UU/s² (−6.5 m/s²); full field 81.92 × 102.4 m, height 20.48 m | [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium; simulator constants | MiMITA arena dimensions and gravity: not set by this mode spec; author map scale before comparison |
| Center of mass / suspension | COM unknown; travel 12, stiffness 500, compression damping 25, relaxation damping 40, max raise 2.5 | UU (0.12 m, 0.025 m max raise); engine-specific spring/damper values | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium for reported values; units/model mapping uncertain | Sedan: COM −0.35 m; travel 0.18 m, spring 420,000 N/m, damper 22,000; tune only after matching mass and force scales |
| Wheel contact model | Simulator uses four raycast wheel contacts; exact retail game implementation is inferred | Contact model | [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h) comments describe a Bullet raycast-vehicle analogue | Low-medium; reverse-engineered model | MiMITA uses cached wheel contact sweeps and analytic tire forces; compare measured contact behavior, not implementation labels |
| Throttle and brake | Drive torque 288,000; brake torque 10,500; drive-speed curve: −1,400→0.1, 0→1, 1,400→0.1; idle brake factor 0.15 | Simulator torque units; UU/s → normalized scale | [rl.source.gs driving values](https://rl.source.gs/physics/), build 25914621, updated 2026-11-04 | Medium; engine scales and curve interpolation are simulator-specific | MiMITA throttle/brake are drivetrain data in `car_sedan.json`; compare acceleration, coast-down, and stopping traces, not raw torque |
| Steering versus speed | Wheel angle curve: 0→35°, 500→20°, 1,000→11°, 1,500→6.25°, 1,750→5°, 3,000→2°; powerslide curve 0→25°, 2,500→7.5° | UU/s → degrees | [rl.source.gs driving curves](https://rl.source.gs/physics/), retrieved 2026-11-04 | Medium; extracted curves, interpolation details not specified | MiMITA steering uses `maxAngleDeg` and `speedFalloff`; compare sampled angle curve over the same speeds |
| Tire lateral and longitudinal grip | Lateral curve: input 0→1, 1→0.2. Handbrake lateral: 0→0.1; longitudinal: 0→0.5, 1→0.9. Without handbrake, longitudinal multiplier is 1 in RocketSim | Normalized curve input/output | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim Car.cpp](https://github.com/ZealanL/RocketSim/blob/main/src/Sim/Car/Car.cpp), retrieved 2026-11-04 | Medium-low; curve input interpretation is simulator-specific | Sedan uses `mu=2.0`, peak slip ratio 0.14, peak slip angle 0.11, friction circle; no direct scalar conversion. Compare force-versus-slip sweeps |
| Powerslide ramp and grip | Analog value 0–1; rises 5/s, falls 2/s. Blends steering toward handbrake curve; reduces lateral grip and changes longitudinal grip by its own curves | s⁻¹; normalized grip | [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h) and [RocketSim Car.cpp](https://github.com/ZealanL/RocketSim/blob/main/src/Sim/Car/Car.cpp), retrieved 2026-11-04 | Medium; community reverse-engineered | MiMITA handbrake/tire behavior: no Rocket League baseline mapping yet; record input and slip/force response during drift tests |
| Jump and held-jump | Immediate impulse 52,500; sustained jump force 262,500 for at most 0.2 s; minimum hold 0.025 s | RocketSim engine impulse/force units; seconds | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium for constants; simulator units | MiMITA jump parameters: unset for Vehicle Arena; compare jump height and velocity trace rather than raw force |
| Second jump / dodge window | Second-jump impulse 52,500; maximum delay 1.25 s after jump ends. Dodge torque time 0.65 s; input threshold 0.5 | Engine impulse units; s; normalized input | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium; community reverse-engineered | MiMITA double-jump/dodge rules: unset for arena; measure valid window and impulse response |
| Boost amount, force, and drain | Capacity 100; standard spawn 33.3; consumption 33.3/s (3 s full tank); force 178,500 simulator units; ground acceleration 991.7, air 1,058.3 | Normalized points; simulator force units; UU/s² | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium; community reverse-engineered | MiMITA boost: unset for arena; compare acceleration and depletion over a timed full-throttle run |
| Air control, damping, recovery | Pitch/yaw/roll torque 130/95/400; damping 30/20/50. Auto-recovery force 100, torque 80; other flip-over timing/impulse constants are listed in RocketSim | Simulator torque/damping units; seconds for timing | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium-low; simulator constants and interpretation | MiMITA air control/recovery: unset for arena; compare angular acceleration, damping, and time to wheels-down |
| Ball visible and collision radius | Visual radius 95; Soccar collision radius 91.25 (rest height 93.15 includes mesh margin) | UU (0.95 m; 0.9125 m; 0.9315 m) | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium; keep visual and collision radii distinct | MiMITA ball visual/collision radii: not specified; author separately and measure contact onset |
| Ball mass, friction, restitution, drag, and cap | Mass 30; max speed 6,000; RocketSim ball friction 0.35 and restitution 0.6; car-ball friction 2.0 and restitution 0.0; drag 0.03 | Community mass units; UU/s; engine coefficients | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium-low; collision coefficient meanings differ by contact pair | MiMITA mode data has no selected ball values; compare roll-down, wall bounce, and car-ball contact separately |
| Car-to-ball impulse | Extra push forward scale 0.65, vertical scale 0.35, max relative speed 4,600; push factor curve: 0→0.65, 500→0.65, 1,400→0.60, 2,300→0.55, 4,600→0.30 | Normalized scale; UU/s | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium; community extracted | MiMITA should retain ordinary shared contact; compare 50/50, centered hit, and off-center hit impulse/velocity outcomes |
| Kickoff/reset and match conditions | Soccar warmup 3 s, match 300 s; standard boost start 33.3/100. Car kickoff positions, ball initial state, and pad layout must be pinned to the chosen game build/map | Seconds; boost points; transforms/velocities unknown here | [rl.source.gs](https://rl.source.gs/physics/) and [RocketSim RLConst.h](https://github.com/ZealanL/RocketSim/blob/main/src/RLConst.h), retrieved 2026-11-04 | Medium for timers/boost; kickoff state needs measurement | MiMITA kickoff positions, reset delays, boost start, gravity, arena scale, and pad layout must be fixed in each comparison record |

Use a repeatable standard-Soccar comparison condition: identify the Octane
reference, simulator/source revision, map and arena scale, 60 Hz MiMITA fixed
tick, input sequence with timestamps, initial car/ball transforms and
velocities, boost amount, gravity, and reset state. RocketSim uses its own
simulation tick and engine scales; compare time-based outcomes and measured
trajectories, not raw per-tick impulses. Record the exact source URL and
retrieval date, source build/commit when known, MiMITA build identity, each
MiMITA parameter/deviation, and playtest outcome. For 50/50 tests, also record
contact point, approach velocities, and first-contact tick. Unknown or
conflicting fields remain `unknown` until measured; do not silently substitute
an estimate.

Officially published facts, extracted values, and community reverse-engineered
estimates must remain labeled as above. These values are tuning inputs, not
mode-specific physics branches. Arena tuning and 50/50 collision tests should
link their result to this baseline and state the deviation and playtest result.

## 10. Acceptance

1. A car can enter the arena and drive normally using the shared vehicle path.
2. The ball spawns, moves, collides with cars and walls, and resets after a goal.
3. A ball crossing the goal volume scores exactly once for the correct team.
4. Kickoff, score, timer, goal flash, resets, and results agree across two
   clients through a complete match.
5. A heavy and a light car produce visibly different ball contact without
   mode-specific physics code.
6. A contested 50/50 car-ball hit reconciles to one authoritative outcome on
   both clients, with no duplicate impulse or goal.
7. Replay playback preserves vehicle movement, ball contacts, goals, and match
   timing for the same compatible content.
8. Human playtesting accepts the arcade control feel; record control profile,
   tuning revision, build identity, playtest scenario, and outcome.
9. Missing arena data produces a visible diagnostic and no invented goal.
10. The mode remains coherent when a vehicle, the ball, or a client disconnects.

## Related specifications

- [Vehicles](../vehicles/vehicles.md)
- [Gamemodes](gamemodes.md)
- [Objective and round HUD](objective-hud.md)
- [Moving physical objects](../moving-physical-objects/moving-physical-objects.md)
- [Networking](../networking/networking.md)
- [Replay](../replay/replay.md)
- [Destructible World](../destructible-world/destructible-world.md)

# Vehicle Arena

**Purpose:** define the ball, goal, scoring, and match rules for a
Rocket-League-style vehicle arena mode.
**Audience:** gamemode, vehicle, physics, networking, map, HUD, and replay
contributors.
**Status:** desired behavior with a working first slice. Physics, match, and bot
probe evidence is recorded in the milestone ladder; presentation parity,
opponent quality, and the remaining physics parity rows are still open.
**Does not:** add a second vehicle physics solver, require vehicle-mounted
weapons, or replace the shared gamemode lifecycle.

## The dream

Two cars, one ball, one arena. The controls, the car, the ball, and the camera
behave the way Rocket League players already have in their hands. A mechanic is
never "close enough, we will tune it later": each one is proven in a live
journal before the next one starts, and the numbers come from documented Rocket
League references (§9) instead of invented values.

## Milestone ladder

The mode ships as a ladder, not a pile of features. Each milestone closes only
on journal evidence from the exact run that exercised it; a successful build is
not evidence of behavior. The `state` column is the current truth and belongs in
the same commit as any change to it.

| # | Milestone | State | Proof |
|---|---|---|---|
| M1 | Input contract: one logical action, any device. Profile data owns the keyboard mapping; the pad owns its own device mapping; nothing hardcodes a key. | partial | Server-side `vehicle.airroll` after a transmitted roll input: run `20261125_233342`, game generation 656. Keyboard rebind path is source-only, not yet runtime-proven |
| M2 | Air handling: jump, double jump, dodge, air roll (all four), stall, flip reset, angular cap | partial | Jump and air roll proven in run `20261125_233342` (`vehicle.jump`, `vehicle.airroll`, `air_control=true`, 184 samples). Roll and yaw direction proven in run `20261126_014900`: intent sign matched the measured `roll_rate_local`/`yaw_rate_local`, zero `vehicle.aircontrol.mismatch`. Dodge is a full 360 (`rocket_dodge_360_v1`, 359.85 deg) and cancel stops it (`rocket_dodge_cancel_v1`, `cancelled=true`, 162 deg). Stall and NPC flip reset are unproven |
| M3 | Ground handling: throttle curve, 23 m/s cap, supersonic, powerslide, wall drive, landing, ground attitude | partial | Drive at 17.5-20 m/s proven earlier. Run `20261126_014657`: 3,726 grounded samples with max \|pitch_rate_local\| and \|roll_rate_local\| of 0.000 and max tilt 0.00 deg under throttle and reverse. Run `20261126_014117`: jump lands with `up_dot 1.0000`, `wheel_mask 15`, zero penetration. Run `20261126_013852`: hard landing plus wall ram, zero penetration events, zero resting penetration. Run `20261126_014423`: a 90-degree roll landing touches down on the chassis (`wheel_mask 0`), never reports a tilted rest, and holds 0.005 m resting overlap. Powerslide effect still unproven |
| M4 | Ball: roll-down, wall/ceiling bounce, car-ball impulse curve, 50/50 convergence | partial | Speed cap 60 m/s, spin cap 66 rad/s, floor restitution 0.6, spin-dependent bounce, and rest-height fix are proven in §9.0. Clean rolling drag, wall bounce isolation, car-ball impulse curve, 50/50 convergence, and dribble remain unproven (§20) |
| M5a | HUD: arena-only scoreboard, clock, phase, goal flash, boost meter, ball-cam indicator; debug readouts default off | partial | Run `20261127_080234`: `vehicle.hud hud_mode=arena`, `hud_speed=false`; command `hud_speed on` logged and the HUD reported `hud_speed=true`. Run `20261127_080258`: `arena.hud.snapshot` reports phase/score/clock. Goal flash still needs a scored goal; visual boost-meter placement is a human check |
| M5b | Camera and audio: one RL camera JSON, ball-cam/swivel, goal/touch cues | partial | Ball cam on/off/on recorded in run `20261125_233213` from the command surface; camera distance/height/FOV/pitch and audio are not implemented (§18.6-18.7) |
| M5c | Tick VFX and debug views: touch/boost/jump/dodge/powerslide markers, hitbox view, speed readout | not started | Spec §18.5, §18.8 |
| M6 | 1v1 versus an NPC: kickoff, goal, reset, score, timer, full match | partial | A full NPC-vs-NPC match is proven (run `20261126_200435`, 7 goals, 4-3, time limit). A human 1v1 against a non-probe opponent, and kickoff behavior, are unproven |
| M7 | 3v3 and SSL-level bots | not started | later, data-driven rosters |

Rules for the ladder:

- A mechanic with no journal proof is described as unproven, never as working.
- A proof names its run, build identity, and journal path.
- Moving a row forward requires the proof event, not a playtest feeling alone;
  human acceptance is recorded separately from `runtime_proven`.
- New mode work extends an existing milestone or adds one. It does not create a
  parallel controller, physics, or ball path.
- The repeatable proofs live in `devscripts/scenarios/rocket_*_v1.json`
  (`rocket_input_rotation_v1`, `rocket_jump_airroll_v1`,
  `rocket_ballcam_v1`, `rocket_landing_flat_v1`, `rocket_landing_tilted_v1`,
  `rocket_ground_tilt_v1`, `rocket_airroll_dir_v1`,
  `rocket_no_underground_v1`). Their commands accept `local` as the vehicle id
  and resolve the seated vehicle, so a scenario never depends on a spawn slot.
- `event_field_all_at_most` / `event_field_all_at_least` assertions require every
  matching sample to satisfy the bound (and at least one sample to exist), which
  is how "the car never pitched while grounded" is expressed.

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
- A player entering the arena spawns into a vehicle, not on foot. The server
  seats an active player in the nearest free authored team vehicle and selects
  the arena profile's automatic transmission/engine-on defaults. This reuses the
  shared seat and vehicle owners; it is not a mode-only vehicle solver.
- An unseated actor that touches the ball uses the shared actor-to-entity push
  path. `arena.ball.contact` records the actor, ball network id, tick,
  pre/post-contact velocity, contact point, and normal.
- The first NPC slice is one human versus one NPC-driven car. The NPC car uses
  the same `VehicleInput` surface and shared vehicle tick as a human driver; it
  is not a separate vehicle solver. Later 2v2/3v3 rosters remain data-driven.
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

### 9.0 Ball measurement record (2026-11-26)

Ball behaviour is measured, not felt. The ball owner exposes a test surface so a
run can be reproduced without a human:

- `ball_set <x> <y> <z> [vx vy vz]` places the arena ball with a velocity. It is
  server-authoritative: a client-side placement is overwritten, so the command
  belongs on the server (a dev launch mode may pass it as a startup command).
- `ball_status` logs `arena.ball.status` on request.
- `arena.ball.state` samples position, velocity, linear and angular speed at
  10 Hz; `arena.ball.contact` and `arena.ball.extra_impulse` record the
  pre/post contact velocities and the added impulse.

Each row below is filled from a run and compared with §9.1. Empty cells are
unmeasured, not "fine".

| Behaviour | Scenario | Reference | Measured | Delta |
|---|---|---|---|---|
| Car→ball impulse, slow hit (~5 m/s) | `rocket_ball_hit_v1` | curve 0.65, Z 0.35 | | |
| Car→ball impulse, fast hit (~23 m/s) | `rocket_ball_hit_v1` | curve 0.55 | | |
| Ball roll-down on flat ground | clean-roll mode `74`, run `20261127_074605_01`, gen 786; prior mode `62` run `20261127_052258` | drag 0.03/s (0.9 m/s^2 at 30 m/s) | 2.9902 -> 2.2638 m/s over 9.185 s = **k 0.03030 /s**; angular 3.2769 -> 2.4809 = **k 0.03030 /s**; `omega / v = 1.0959 = 1/r`; z exactly 0.90250 | **matches**; prior 0.052 was wall-contaminated and included the 10 mm slop bounce |
| Ball spin during roll | run `20261127_052258` | angular speed consistent with rolling at `v/r` | ratio 0.95-0.98 at 1.08-3.20 m/s | rolling; old 6 rad/s cap no longer limits it |
| Ball resting height | clean-roll mode `74`, run `20261127_074605_01`, gen 786 | radius 0.9125 m -> centre at 0.9125 above the floor | centre `0.90250` with floor top `-0.01` = **0.91250 above floor**; constant for 160 samples; prior 0.9093-0.9175 sawtooth gone | **exact**; no lift, no sawtooth |
| Floor rebound at 20 m/s | run `20261127_052258` | restitution 0.6 | -13.95 -> +8.37, -8.17 -> +4.90, -4.84 -> +2.90 m/s | matches 0.6 |
| Wall rebound at 20 m/s | `rocket_ball_bounce_v1` | restitution 0.6 | 0.6 configured; not isolated | unmeasured |
| Ball speed cap | any | 60 m/s | 60 m/s (proven earlier) | 0 |
| Ball spin cap | any | 65.7 rad/s is rolling at the 60 m/s cap | 66 rad/s | raised from 6 so the ball can roll instead of slide |
| 50/50 convergence | `rocket_ball_50_50_v1` | one authoritative result | | |
| Dribble carry | `rocket_ball_dribble_v1` | friction 2.0, restitution 0.0 | | |

2026-11-27 measurement note: the 2026-11-26 "inscribed tessellation"
explanation for the 0.10 m rest was wrong. Sphere entities used the shared
physical-entity 0.1 m rounded mesh shell; they now use analytic sphere-vs-world
contact. arena2 uses one solid floor/ceiling box because the 10 m tile seams
caught the rounded shell and launched the ball. The ball carries friction 0.35
and restitution 0.6 from arena1. The remaining drag excess is contact friction,
not the authored 0.03 drag. See
`docs/regressions/collisions/static-map-double-scale-REG-2026-11-27.md`.

### 9.0.1 Rolling contact model and clean-roll probe (2026-11-27)

The 0.052/s drag fit above was taken over a path that had already bounced off
walls and ramps, so it is not a clean rolling measurement. It also exposed a
real contact bug: the shared depenetration solver builds its target as
`penetration + slop` (10 mm), so every sphere contact lifted the ball by up to
10 mm. A rolling ball therefore fell, was teleported back up, and fed that
energy through the friction solve. `arena.ball.state` shows the cycle directly:
rest `z` oscillates `0.90933 -> 0.91750` and speed rises inside each cycle
(`0.70 -> 0.79 m/s`, run `20261127_052258`, while angular speed decays at the
authored 0.03/s).

The sphere contact path now:

- uses the authored collision radius with no added skin,
- passes zero correction slop (actors keep their 10 mm allowance),
- keeps the authored restitution 0.6 and friction 0.35,
- keeps one decay constant (`linearDamping = angularDamping = 0.03/s`).

`devscripts/dev-launch-modes.json` mode `74` is the clean probe: it starts the
ball at the arena centre at rest height with a pure rolling velocity and spin
(`v = 3 m/s`, `omega = v / r`), so the first 9 s never touch a wall. Acceptance
for the row below is a fitted late-roll `k` of `0.030 +/- 0.003 /s` over at least
6 s with no wall contact, plus a stable rest height (no `z` sawtooth) and
`|omega| = |v| / r` within 2 percent. The earlier contaminated fit is retained
as history, not as the acceptance value.

**Measured 2026-11-27, run `20261127_074605_01`, generation 786, mode 74:**
2.9902 -> 2.2638 m/s over 9.185 s gives `k = 0.03030 /s`; angular speed
3.2769 -> 2.4809 rad/s gives the same `0.03030 /s`; `omega / v = 1.0959`, exactly
`1 / 0.9125`; centre `z = 0.90250` with floor top `-0.01` is exactly one radius
above the floor and constant across all 160 samples. Acceptance met.

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

## 11. Arcade controls and boost (2026-11-05)

The mode selects a named data profile (`vehicle_profile`) from
`config/vehicle-profiles/`. The profile maps device inputs to the shared vehicle
input and physics owners; it does not add an arena-only car solver or an
`if (arena)` branch.

| Input | Ground | Air |
|---|---|---|
| `W` / `S` | throttle / brake-reverse | pitch forward / backward |
| `A` / `D` | steer left / right | roll left / right |
| `Left Shift` | powerslide, friction x 1/3 | hold to enable air roll |
| `Left Mouse` | boost | boost |
| `Right Mouse` | jump; after leaving a surface, next press is double jump/flip | same |
| `Space` | ball cam / car cam toggle | same |

- Controls are car-relative, never camera-relative.
- The keyboard mapping in this table is data: each profile's `bindings` block in
  `config/vehicle-profiles/` maps the logical action to the key or mouse button,
  and the pad mapping lives in `config/input/gamepad.json`. The vehicle input
  path reads those two owners only; it never falls back to a hardcoded key, so a
  rebind always wins.
- Air control applies pitch, yaw, and roll torque from the profile `airControl`
  block. The horizontal stick is yaw; the free-air-roll modifier (Shift, or R1
  on the pad) remaps it to roll, and Square/Circle are directional air roll.
  Opposing yaw and roll cancel that axis immediately, which is the Rocket League
  stall.
- Diagonal air inputs combine; opposing inputs cancel that axis immediately while
  existing angular momentum damps.
- Air roll keeps angular momentum and decays with profile damping.
- The arena profile auto-starts the engine and uses automatic transmission.
- Boost is a separate meter from the engine turbo field. Its capacity,
  drain, and force are profile data. A full 100-point tank drains in 300 server
  ticks (5 s) at 60 Hz; this is a MiMITA choice, not the 3 s comparison baseline.
- Wall driving uses a data-driven adhesion force on near-vertical contacts and
  slides slowly under gravity. Ceiling contacts do not adhere, so gravity pulls
  the car off.
- Chassis contact uses shared restitution. Wheel-to-ball contact is much weaker
  than chassis-to-ball contact but still counts as a wheel surface for flip
  resets.
- A jump or flip reset requires all four wheels to touch any surface, including
  the ball.

### 11.1 Boost pads

Boost pads are authored map data, not mode code:

```json
"boost_pads": [
  { "id": "pad_big_1", "kind": "big", "position": [0,0,0],
    "radius": 2.0, "rechargeTicks": 600, "enabled": true },
  { "id": "pad_small_1", "kind": "small", "position": [10,0,0],
    "radius": 1.0, "rechargeTicks": 240, "enabled": true }
]
```

- A small pad adds 12 boost, clamped to the profile maximum.
- A big pad sets boost to the profile maximum.
- Pads use cooldown timers and are visible in the world with a recharge timer.
- The server owns pad availability; clients may predict pickup and reconcile.
- Pad collection is logged as `arena.boostpad.collected` with pad id, kind,
  boost before/after, and tick.

## 12. Arena authoring (2026-11-05)

Arena geometry, ball spawn, goals, kickoffs, and boost pads are authored through
the map editor into `config/maps/<mapId>.json`. Blender is not required.

- Blocks carry a `shape`. `box` is the rectangle primitive; `profile`,
  `cylinder`, `cone`, `sphere` and `wedge` are curved primitives. One tessellator
  feeds render and collision, so the visible surface and the physics surface are
  the same triangles (`src/map/map-authored-geometry.cpp`).
- `profile` is the measured geometry primitive: a 2D polyline in the plane
  perpendicular to `profile_axis`, extruded along `size[profile_axis]`. A curved
  wall ramp is 8 or 9 measured points, not an invented radius. Points are in
  metres relative to the block position. `profile_flip` reverses the winding, and
  the normal is auto-oriented toward the playfield.
- `cylinder` (radius, `arc_degrees`, `convex`, `segments`) covers fillets, post
  rounding and the goal back. `cone` (`radius_top`), `sphere` (dome) and `wedge`
  cover tapered transitions, caps and plain ramps.
- Editor: `mapeditor add block [id] [shape]`, `block_shape <shape>`,
  `block_profile <u1> <v1> <u2> <v2> ...`, and
  `block_shape_param <arc|segments|convex|axis|radius_top|flip> <value>`.
- There is no Bezier path/ribbon/tube implementation. An earlier version of this
  section claimed one; the maps in the repository only ever used boxes.
- Curves are tessellated into the shared triangle collision mesh; there is no
  analytic quarter-sphere collider.
- Optional `mapeditor bake_glb` writes a derived `assets/maps/<mapId>.glb`.
  The JSON remains authoritative.
- Missing ball spawn or goal data fails closed with a diagnostic and never
  creates a default scoring zone.
- The arena map must expose at least one world spawn point (the kickoff marker
  is the first implementation). The shared match runtime rotates away from a
  map with no spawn points; that rotation must not silently replace `arena1`.
- Launching the mode must verify the effective `map=arena1`,
  `mode=vehicle_arena`, `gamemode=vehicle_arena`, `weapon_set=1`, and
  `npcs=0`. A mismatch is a launch-identity failure, not a playable arena
  session.

## 12a. arena2 (2026-11-26): generated from the measured Rocket League mesh

`config/maps/arena2.json` is generated by `devscripts/gen-arena2.py` from
`assets/maps/rl/pitch.obj`, the standard Soccar collision mesh (4,384 vertices,
8,028 triangles, Unreal units). The reference record is
`config/reference/arena2-geometry.json`; every value is labelled DOCUMENTED,
MEASURED, DERIVED or ESTIMATED, and the mesh itself is never shipped or loaded.

Plan dimensions are **documented** (81.92 x 102.40 m, ceiling 20.48 m, 45 degree
corner cuts with `|x|+|y| = 80.64`, goal mouth 17.8551 x 6.42775, goal depth
8.80, net back at 60.00). Curvature is **measured** from the mesh:

| Profile | Measured points (metres, inward offset u from the wall plane, z) |
|---|---|
| Lower ramp | (-2.560, 0.000) (-2.044, 0.052) (-1.564, 0.201) (-1.129, 0.438) (-0.750, 0.750) (-0.438, 1.129) (-0.201, 1.564) (-0.052, 2.044) (0.000, 3.550) |
| Upper ramp | (-6.243, 20.440) (-4.089, 20.376) (-3.128, 20.077) (-2.259, 19.605) (-1.501, 18.979) (-0.875, 18.221) (-0.403, 17.352) (-0.104, 16.391) |

The lower ramp starts 2.56 m inside the wall plane, so the flat floor is
inscribed; the upper ramp spans 6.14 m horizontally from y = 34.72 to the wall,
which is why the ceiling curve matters for wall and ceiling play. Both ramps are
`profile` blocks on every straight wall and every diagonal corner, with the ramp
stopped at the goal mouth (no ramp across the opening).

Floor and ceiling are tiled at 10 m rather than one field-sized slab: a single
quad pair dropped the ball through the floor (run `20261126_231000`, ball resting
at z = -1.31).

2026-11-27 wall gap fix: `gen-arena2.py` built the straight long walls with span
`2*corner_plane - 2*half_length = 58.88 m`, which ended at `x=±29.44` while the
corner walls begin at `x≈±39.99`. That left a ~10 m opening on each long wall;
the ball escaped through it at `x≈30, y≈-40.9` at only ~7 m/s (runs
`20261127_080709`, `20261127_080920`). The span is now the playfield corner cut,
`2*(corner_plane - half_width) = 79.36 m`, so the long wall reaches the corner.

The arena declares materials per surface (`grass` floor, `wallv2` walls,
`wallv4` ceiling and goal) with tints above 1, because arena1's blocks had
`material: ""` and rendered untextured. arena1 now declares `grass`, `wallv2` and
`wallv4` as well.

## 13. Arena events (2026-11-05)

```text
arena.config.loaded
arena.ball.spawned
arena.ball.contact
arena.goal.detected
arena.goal.reset
arena.boostpad.collected
arena.prediction.divergence
arena.presentation.goal_flash
arena.presentation.vfx
arena.hud.snapshot
arena.freeplay.throw
vehicle.npc.kickoff
vehicle.npc.recover
vehicle.camera.applied
vehicle.jump
vehicle.jump.double
vehicle.surface.reset
vehicle.boost
vehicle.powerslide
vehicle.airroll
vehicle.ballcam
vehicle.landing
vehicle.attitude
vehicle.aircontrol.mismatch
vehicle.penetration
vehicle.rest
vehicle.rest.tilted
vehicle.settle
```

Diagnostics are categorized, bounded, and rate-limited where they can repeat.

Landing and orientation proofs read these fields:

- `vehicle.landing` carries `up_dot_world_up`, `tilt_deg`, `wheel_mask`,
  `wheels_contact`, `settled_on_wheels`, `impact_speed_mps`, `impact_vz`,
  `angular_speed`, `pitch_rate_local`, `roll_rate_local`, and
  `max_penetration_m` at the touchdown tick. A flat landing is
  `up_dot_world_up > 0.97` with `wheel_mask` at its full value.
- `vehicle.attitude` is a 10 Hz sample of `grounded`, `wheel_mask`,
  `up_dot_world_up`, `tilt_deg`, `pitch_rate_local`, `yaw_rate_local`,
  `roll_rate_local`, the decoded `pitch_input`/`yaw_input`/`roll_input`, and
  `max_penetration_m`. `yaw_rate_local` and `roll_rate_local` are signed so
  positive is always the driver's right, which makes "intent sign equals
  rotation sign" a direct assertion.
- `roll_meaningful`/`yaw_meaningful` gate that comparison to samples where the
  input and the response are both significant and the other axis is quiet.
  `vehicle.aircontrol.mismatch` fires when a meaningful sample disagrees, so
  its absence plus a meaningful sample proves the direction contract.
- `vehicle.penetration` carries `max_penetration_m` from the contact solver for
  the car's real collision shape. A compressed suspension is not penetration.
- `vehicle.rest.tilted` is emitted instead of `vehicle.rest` when a body sleeps
  out of the upright band; the vehicle never sleeps while tilted, so any
  occurrence is a regression.

## 14. Ground attitude contract (2026-11-26)

A Rocket League car is rigid on the ground. Tyre and suspension forces are
applied at the contact patch, which produces weight-transfer pitch and roll that
the real game does not have; that also let a car freeze on a corner. The
profile's `ground.attitude` block owns the rule:

- `attitudeLock` (default false, enabled by the Rocket League profiles): while
  all four wheels are grounded, not flipping, and not resting on the ball, the
  chassis keeps only its yaw rate and its up axis follows the averaged wheel
  contact normal at `alignRateDegPerSecond`. It is skipped when the car is not
  roughly upright, so a torso pose is never snapped back.
- No other code pitches or rolls a grounded car. The old per-wheel angular
  damping is gone; only vertical bounce damping remains for a four-wheel stance.
- A body may not sleep while `up.z <= 0.90`, so a half-finished tip can always
  finish instead of being recorded as a resting pose.
- The collision box is the car's whole envelope and its bottom face is the
  wheel contact plane (`collisionCenter` in the vehicle template), so the
  wheels cannot sit below the surface at rest and the wheel mesh follows
  suspension travel instead of sinking into geometry.

## 15. Presentation preferences (2026-11-26)

Three local-seat presentation elements are console-toggled and **disabled by
default**. Defaults are written once at startup as `vehicle.preferences`
(`hud_speed=false`, `hud_car_assist=false`, `vehicle_exit_prompt=false`,
`exit_key=F`) so a missing element is explained by the journal instead of looking
broken.

| Preference | Command | Default | Effect when on |
|---|---|---|---|
| Speed / rpm readout | `hud_speed on\|off` (no argument toggles) | off | Draws `43 km/h   1234 rpm` |
| Engine / gear / diff readout | `hud_car_assist on\|off` (no argument toggles) | off | Draws `ENGINE ON/OFF  AUTO/MANUAL  GEAR n  DIFF x` above the speed line |
| Enter/exit vehicle prompt | `vehicle_exit_prompt on\|off` (no argument toggles) | off | Shows the `enter seat` / `exit vehicle` prompt |

- **F always exits the vehicle**, independently of the prompt. It logs
  `vehicle.exit.key` with the prompt state so the behaviour is visible.
- Each change logs `vehicle.preference` with the preference name, the new value,
  and the compiled default.
- Turning a preference on is presentation only; it never gates driving,
  possession, or the exit action.

## 16. Ground steering, dodge and match evidence (2026-11-26)

### 16.1 Grounded steering owns the yaw rate

A car cannot rotate in place. While a car is grounded and upright, the steering
input sets the yaw rate directly:

```text
targetYawRate = steer * ground.maxYawRateRadPerSecond
                * clamp(speed / 1.5, 0, 1)
yawRate += clamp(targetYawRate - yawRate, -ground.yawAccelRadPerSecond2 * dt, ...)
```

- `ground.maxYawRateRadPerSecond` (default 3.4) and
  `ground.yawAccelRadPerSecond2` (default 14) live in the vehicle profile.
- Rocket League's turn rate is roughly constant with speed, so full rate is
  reached at 1.5 m/s rather than a long ramp.
- Residual rates below 0.02 rad/s are zeroed: a parked car does not creep.
- Before this, contact friction decided the yaw and integrated a tiny
  asymmetric ground torque into **4 degrees of drift while parked** and **225
  degrees of curve over 9 s while driving** (measured log `slip_deg 0.08`). A
  straight drive now holds `yaw_deg = 0.00` from `x = -39` to `x = +32`
  (run `20261126_190324`).
- The lateral-velocity bleed is skipped while powersliding: the slide is the
  point of a powerslide.

### 16.2 Normal grip versus powerslide grip

`bodyFriction` 0.65 and `bodyWallFriction` 0.30 are normal driving. Powerslide
remains `powerslide.gripScale` 0.10. The previously slippery feel is now the
powerslide feel; ordinary driving grips.

### 16.3 Dodge is a full turn, and cancelling stops the turn

- The dodge's angular limit is the profile `flip.maxAngularVelocity`, and the
  vehicle template's `maxAngularSpeed` must be at least as large (24). The
  template clamp used to cap every dodge at 5.5 rad/s (about 205 degrees).
- Flip torque stops once `flipRotationRad` reaches 2*pi, so a dodge is 360
  degrees every time: `rocket_dodge_360_v1`, `rotation_deg_total = 359.85`,
  `cancelled = false`.
- Pushing the stick opposite the dodge direction cancels it. Cancelling removes
  the flip-axis angular velocity, not just the torque, so the body stops
  rotating instead of coasting: `rocket_dodge_cancel_v1`,
  `cancelled = true`, rotation stops at 162 degrees.
- `vehicle.flip` reports `rotation_deg_total`, `cancelled`, `flip_limit`,
  `air_limit` and `flip_seconds`, so a future clamp regression is attributable.

### 16.4 Match clock and goal attribution

`config/gamemodes/vehicle_arena.json` owns `time_limit_seconds: 300`. The limit
is enforced and observed:

```text
arena.match.clock        every 30 s and at the limit
arena.match.over         reason = time_limit | score_limit, winner_team, scores
arena.goal.detected      goal_id, goal_team, scoring_team, scorer_vehicle_id,
                         scorer_team, scorer_driver_id, match_seconds,
                         remaining_seconds, server_tick, scores
arena.goal.received      client observation of the same score change:
                         score_a, score_b, server_tick, client_tick
```

- `match_seconds` and `remaining_seconds` are match-relative, not process-relative.
- A goal is a ball that **touches** the goal volume: the test subtracts the ball
  radius, because the ball's centre stops one radius short of the back net. A
  centre-only test let a shot into the net rest 0.6 m outside the volume and
  score nothing.
- The scorer is the car that **last touched** the ball
  (`PhysicalEntity.lastTouchedVehicleNetworkId`, set by the entity pair
  contact), not the car nearest the goal line when the ball arrives.
- `arena.match.over` with `reason = time_limit` ends the match: the higher score
  wins, and a goal after the whistle does not count or change the score.

Full run `20261126_185723` (mode 66, `MIMITA_ARENA_BOT_VS_BOT=1`): clock 0 to
300.0 s, five goals from both bots, final 3-2, `arena.match.over`
`reason = time_limit` at `server_tick` 18974, and all five score changes
observed by the client with `client_tick`. Twelve landings, all twelve on the
wheels, zero `vehicle.rest.tilted`.

### 16.5 The ball keeps rolling

The ball has **one** decay constant. `angularDamping` used to be 0.4/s while
travel decayed at `linearDamping` 0.03/s; contact friction converted the missing
spin into a 0.25 m/s^2 linear drag and a 3.6 m/s roll died in 14 s. Spin and
travel now decay together at the ball's own drag constant.

### 16.6 Bot controller (probe only)

`MIMITA_ARENA_BOT_VS_BOT` seats a second bot on the other team so two NPCs play
each other with no human. The controller aims at a point **behind** the ball
relative to the opponent goal, reverses toward a target that is behind it, and
reverses out of geometry when it has throttle applied but is not moving. It is a
probe: it is how the 1v1 evidence is produced, not a production AI.

## 17. Aerial play, saves and probe evidence (2026-11-26)

### 17.1 The aerial program

`MIMITA_ARENA_BOT_VS_BOT` also drives an aerial: when a ball is above the car
(> 2.0 m, or > 1.1 m while defending) within 18 m and the bot has boost, it

1. plants the car and jumps (`vehicle.jump`, `jump_count = 1`),
2. points the nose at the ball with a **body-rate** controller and boosts
   (`vehicle.boost` with the car airborne),
3. air-rolls on the climb while the ball is still more than 10 m away
   (`vehicle.airroll`),
4. dodges through the ball inside the double-jump window (`vehicle.dodge`).

An attacking aerial only starts from **behind** the ball relative to the target
goal; flying at it from the goal side headed the ball into the bot's own net.

### 17.2 Aerial goal evidence

Launch mode 67 (`MIMITA_ARENA_BALL_KICK="0,0,0,42,0,5"`) places the ball at 5 m
over the orange goal. One run, `20261126_195128`:

| time | event | field |
|---|---|---|
| 0.28 s | `vehicle.npc.aerial` | phase start, `ball_height_m = 5.0` |
| 0.28 s | `vehicle.jump` | `jump_count = 1` |
| 0.49 s | `vehicle.boost` + `vehicle.airroll` | airborne boost, roll |
| 0.74 s | `vehicle.dodge` | the NPC dodged |
| 0.93 s | `arena.ball.contact` | car touched the ball |
| 1.79 s | `arena.goal.detected` | `aerial = true`, `scorer_ball_height_m = 2.74`, `scorer_vehicle_id = 2`, `scorer_team = 0`, `server_tick = 98` |

`arena.goal.detected` gains `aerial` and `scorer_ball_height_m`; the aerial flag
is taken from the ball height at the scoring touch
(`PhysicalEntity.lastTouchedHeightM`).

### 17.3 Aerial save evidence

A save is computed from real state, never declared: the ball's previous velocity
must point at the toucher's own goal at more than 5 m/s, the ball must be within
16 m of that goal, and the touch must turn it away (dot to the goal below 0.35).
`arena.save` carries the goal, team, vehicle, speed before and after, distance,
both dots, the touch height and `aerial`.

Launch mode 68 (`MIMITA_ARENA_BALL_KICK="8,0,0,36,0,3"`) lobs a shot at the
orange goal. Run `20261126_194927`:

```text
arena.save goal_id=orange_goal team=1 vehicle_id=2
           ball_speed_before_mps=8.34 ball_speed_after_mps=7.05
           distance_to_goal_m=12.53 aimed_at_goal=0.93 still_at_goal=-0.55
           ball_height_m=2.58 aerial=true
```

The ball was turned from aimed-at-goal (0.93) to heading away (-0.55) by a car
whose touch happened 2.58 m above the floor.

### 17.4 Spin changes the bounce (measured)

`ball_set` and `MIMITA_ARENA_BALL_KICK` accept a spin
(`ball_set <x> <y> <z> [vx vy vz [wx wy wz]]`; the probe takes three, six or
nine values). Dropping the ball straight down with opposite spin about y:

| run | spin about y | bounce velocity |
|---|---|---|
| `20261126_201529` (mode 69) | +60 rad/s | (+0.62, +0.59, 11.53) |
| `20261126_201615` (mode 70) | -60 rad/s | (-0.62, -0.59, 11.53) |

Same vertical rebound, mirrored horizontal deflection: spin changes the bounce
direction.

Re-confirmed 2026-11-27 on generation 786 after the exact-radius/zero-slop
sphere change. Mode 69 run `20261127_074747` and mode 70 run `20261127_074833`
both start with a -8.67 m/s vertical impact and rebound at +5.47 m/s
(restitution 0.63). Horizontal deflection is exactly mirrored: `+5.10` and
`-5.10` m/s. The contact change did not remove the spin response.

### 17.5 Roof or wheels, proven by the fuzz probe

Launch mode 65 (`MIMITA_VEHICLE_FUZZ=7`) throws the car with random velocity and
spin every few seconds; `vehicle.fuzz.report` classifies each settle just before
the next kick. `rocket_settle_fuzz_v1` (run `20261126_201305`) passes:

```text
kicks=55 wheels_landings=52 tilted_rests=0 worst_settled_up_dot=1.0
settled_up_dot=0.99999 settled_wheel_mask=15
```

A body thrown onto a corner or its side always finished on all four wheels. No
scripted righting rule is involved; the low centre of mass does it.

### 17.6 The probe bot is not a production AI

It exists to produce evidence. It aims at a point behind the ball, reverses when
the target is behind it, reverses out when it has throttle applied without
moving, self-rights with air control when its up axis is off vertical, and
aerials as described above. Its team comes from the car it actually occupies
(`VehicleState::origin` is the zero vector before the first tick), so a bot never
attacks its own goal.

Full 5-minute bot-vs-bot run `20261126_200435` on generation 729: clock 0 to
300.0 s, 7 goals, final 4-3, `arena.match.over reason=time_limit`
`winner_team=0` at `server_tick` 19047, all 7 score changes observed by the
client with `client_tick`, 23 aerials, 2 dodges, 91 air rolls, 34 landings with
28 on wheels and 0 tilted rests.

### 17.7 Settling contract (2026-11-27, RocketSim reference)

A body is **settled** when its speed is below 0.5 m/s and its angular speed is
below 0.5 rad/s. Wheels (`up_dot_world_up > 0.95` with the full wheel mask) and
roof (`up_dot_world_up < -0.90`) are the only accepted settled poses. A body may
not sleep in any other pose; sleep is gated by the accepted-pose test, not by
`up.z <= 0.90`.

The mode uses the RocketSim model, not a scripted righting rule:

- **Auto-roll** runs only while throttle is applied and the car has partial wheel
  contact (1-3 wheels) or chassis contact. It uses the contact normal, not world
  up:
  - `groundUp = normalize(sum grounded wheel contact normals)` or the body
    contact normal,
  - force `-groundUp * autoRollForceMps2 * mass * dt` (`1.0 m/s^2`),
  - torque toward wheels about the two surface-relative axes
    (`autoRollAngularAccel`, 7.68 rad/s^2).
  It is not triggered by boost alone, matching Rocket League.
- **Auto-flip** runs when jump is pressed while touching a floor-like surface
  (`normal.z > 0.7071`) with `|roll| > 2.8 rad`:
  - impulse `-up * 2.0 m/s`,
  - forward-axis torque `50 rad/s^2` for `0.4 s` scaled by `|roll| / pi`.
- **Sticky force** runs when any wheel contacts:
  - `scale = 0.5`, plus `1 - |upwards.z|` when throttle or forward speed is
    present (up to `1.5 g` into a wall/ceiling),
  - impulse `upwards * scale * gravity * mass * dt`.
- **Non-sticky wheel friction**: with no throttle, tire friction is scaled by
  `0.1` on a wall, `0.5` at 45 degrees, `1.0` on the floor, so roof/wall sliding
  does not catch.
- **Body friction** is the Rocket League car-world value `0.3`, restitution
  `0.3`. Tire grip remains in the separate tire model.
- **Boost** always acts along the car's local forward axis, independent of
  orientation, so upside-down boost thrusts forward.

The old manual righting impulse is retained in code but disabled in the RL
profiles (`settle.manualRightingEnabled=false`). A guaranteed no-input
side/front/back rest correction is a later option, not current behaviour; Rocket
League itself allows a no-input side rest.

The fuzz probe kicks with random velocity, random spin, random roll/pitch/yaw,
random dodge (jump then second jump), random boost on/off, and a late random
throttle window so auto-roll has input after landing. It classifies only after a
supported settle has held for 0.5 s (30 updates). Acceptance with throttle:
`side_rests == 0`, `nose_rests == 0`, `tail_rests == 0`, `tilted_rests == 0`,
`wheels_landings + roof_rests == kicks`, and every escape is logged.

`vehicle.fuzz.report` carries `wheels_landings`, `roof_rests`, `side_rests`,
`nose_rests`, `tail_rests`, `tilted_rests`, `worst_settled_up_dot`, and
`escapes`. `vehicle.drop.fuzz` records `boost_on`, `throttle_on`,
`jump_at_tick`, and `dodge_at_tick`.

## 18. Presentation contract (2026-11-27)

Presentation is a client-only consumer of replicated state and authoritative
events. It never decides a goal, score, boost amount, or contact outcome.

### 18.1 One arena HUD, debug off by default

- While a local player is seated in `vehicle_arena`, the arena HUD replaces the
  combat HUD. Health, ammo, crosshair, hitmarker, damage numbers, killfeed, and
  the generic `Speed: n m/s` line are not drawn. The same rule applies to
  `vehicle_freeplay`, which is the practice surface for the mode.
- The arena HUD reads one client snapshot: phase, score A/B, match start tick,
  server tick, time limit, goal value, local team, boost charge, ball-cam state.
- Layout and strings live in `config/gui/gamemode-meta-gui.json` under a
  `vehicle_arena` section. The generic overlay owner draws it; a second score
  model is not allowed. A missing section is a visible HUD failure, not a reason
  to invent a default scoreboard.
- Debug readouts are console commands and default off. The km/h + rpm line is
  controlled by `hud_speed on|off` (no argument toggles), default **off**. The
  engine/gear line remains `hud_car_assist`, also default off (§15).
- Every toggle writes `vehicle.preference` with the name, new value, and compiled
  default.

### 18.2 Scoreboard, clock, phase, goal flash

- Top centre: `BLUE 0 - 0 ORANGE` from the mode's ordered team names, with the
  left team matching the blue goal owner.
- Under it: the authoritative match clock in `MM:SS`, counting down from
  `time_limit_seconds`. It freezes at 0 and never runs backwards. It is derived
  from `serverTick - matchStartTick`, not from a local wall clock.
- Phase banner: `INTERMISSION`, `GET READY`, `GO!`, `ACTIVE`, `MATCH OVER`.
  Countdown is the generic `countdownText` path.
- Goal flash: a client-observed score change shows the scoring team's name and
  the new score for 2.0 s. It is deduplicated by `(duelId, serverTick, scoreA,
  scoreB)` so a duplicated packet cannot flash twice. It logs
  `arena.presentation.goal_flash` with the observed score and client tick.
- Results: winner, final score, and `reason` from `arena.match.over`.

### 18.3 Boost meter and ball cam

- Boost is a gauge, not text: a circular/segmented meter in the bottom-right
  screen corner, with the numeric value inside it. It reads replicated
  `boostCharge / profile.boost.max`.
- Empty, normal, and full states are visually distinct. A full meter does not
  pulse forever; it marks full once.
- Ball cam has a small icon next to the meter (`BALL CAM` / `CAR CAM`), driven by
  the replicated `ballCam` flag. The pad toggle remains the input owner.
- Neither element is drawn on foot or when the local player is not seated.

### 18.4 World presentation

- Boost pads: authored pad position, kind (small/big), radius, and availability.
  Ready pads are bright; recharging pads are dim and show the remaining fraction.
  Pad availability is server-owned and replicated; the client may predict a
  local pickup for immediate feedback but reconciles to the server mask.
- Nametags: reuse the existing player-nameplate owner, projected at the car body.
  They show the driver name and team colour, never a health bar.
- Goal volumes are authored and match the scoring volume; the visual net may be
  larger. A missing goal or ball definition fails closed with a diagnostic.

### 18.5 Tick VFX

All markers are one-tick, alpha 0.5, presentation-only, and rate-limited per
source. They are consumed from a replicated presentation event (sequence-numbered,
exactly once) or a local vehicle-state transition; a client-only proximity guess
is not used for ball contact.

| Cue | Colour | Size / anchor | Source |
|---|---|---|---|
| Ball touch | red | 0.1 m at contact point | `arena.ball.contact` (server) |
| Boost | yellow | 0.3 m at exhaust | local `vehicle.boost` transition |
| Powerslide | dark purple | 0.3 m under the car | local `vehicle.powerslide` transition |
| Dodge | bright pink | 0.3 m at car centre | `vehicle.flip` / `vehicle.dodge` |
| Jump | turquoise | 0.3 m above the roof | `vehicle.jump` |

The server event packet carries `kind`, `tick`, `vehicle_id`, `team`, `point`,
and a monotonically increasing `serial`. The client drops a serial it has
already rendered and never replays an old serial after reconnect.

### 18.6 Camera

One file, `config/cameras/vehicle-camera.json`, owns the vehicle camera. Combat
camera settings do not apply while seated in the arena. Values use Rocket League
units converted to metres/degrees:

| Setting | Value |
|---|---|
| Distance | 2.20 m |
| Height | 0.90 m |
| Pitch | -7 deg |
| FOV | 110 deg |
| Stiffness | 1.0 (0.0-1.0, see below) |
| Swivel speed | 10.0 (scaled to deg/s in the file) |
| Transition speed | 10.0 |
| Ball-cam snap | 0.5 |
| Look-behind | 180 deg |

**Stiffness semantics (Rocket League).** Stiffness is the trailing distance the
camera allows behind the rigid chase pose at the profile top speed, measured in
Octane body lengths (1.18 m each):

| Stiffness | Extra trailing at 23 m/s | Behaviour |
|---|---|---|
| 0.0 | 7.08 m (6 car lengths) | Loose, maximum lag |
| 0.5 | 3.54 m (3 car lengths) | Half lag |
| 1.0 | 0 m | 1:1, no trailing lag, no sway/jitter |

The implementation is a first-order follow of the chase target with time
constant `tau = (1 - stiffness) * stiffnessLagMaxM / topSpeedMps`, so the
steady-state extra distance is `speed * tau`. `stiffnessLagMaxM` is 6 x 1.18 m =
7.08 m and `topSpeedMps` is 23.0 from `rl_soccar.json`. Collision push is kept
rigid so the stiffness value owns all lag and 1.0 cannot acquire a hidden spring.
A jump larger than 20 m (respawn, teleport, replay seek) snaps the follow target.
`vehicle.camera.follow` samples every 3 s with the effective `stiffness`,
`lag_tau_s`, `speed_mps`, `extra_distance_m`, and `expected_extra_m`.

The camera path applies these values for the local seated vehicle only. It logs
`vehicle.camera.applied` once per load with every effective value. Ball cam eases
toward the ball; car cam follows momentum, not the nose. Swivel returns to
centre when the stick is released.

### 18.7 Audio

- Goal: a short whistle/explosion cue, once per confirmed goal.
- Ball touch: impact volume and pitch scale with relative speed, rate-limited to
  one cue per 100 ms per pair.
- Boost, jump, dodge, powerslide: reuse the vehicle audio owner; do not create a
  second engine/RPM scheduler.
- All cues are client-side. A missing asset logs once and disables that cue; it
  never blocks gameplay.

### 18.8 Debug views

- `hud_speed on|off`: km/h + rpm line, default off.
- `hud_car_assist on|off`: engine/mode/gear/diff line, default off.
- `vehicle_hitbox [on|off]`: per-face colours front red, back blue, right green,
  left yellow, top white, bottom magenta, plus a forward arrow. Local vehicle
  only, default off.
- Debug views never change the collision shape or the physics tick.

## 19. NPC opponents (2026-11-27)

The probe bot in §16.6/§17 is an evidence tool. An opponent must be able to
finish a 1v1 against a human without the probe's reverse-into-geometry escape
being its main strategy.

### 19.1 Ladder

| Step | Behaviour | Acceptance |
|---|---|---|
| N1 | Kickoff: drive to the ball from the authored kickoff point, hit it toward the opponent goal | `vehicle.npc.kickoff` with a first touch inside 2.5 s of GO |
| N2 | Ground rotation: attack from the ball side facing the opponent goal, recover to the own half when the ball is behind | 3 minutes of 1v1, no own-goal from a goal-side approach, zero stuck ticks over 2 s |
| N3 | Closed-loop aerial: target position + target velocity + brake, not nose-at-target | `rocket_aerial_arrive_v1`, final distance to the target < 1.0 m, vertical overshoot < 1.5 m |
| N4 | Freestyle reset: pop, track above the ball, four-wheel touch, dodge at goal | `vehicle.flip.reset` from an NPC, then `arena.goal.detected` with `freestyle=true` |

Freestyle progress 2026-11-27: the tracking phase now holds the jump for six
ticks, starts only from the ground, applies the aerial 0.25 boost floor, and
self-right no longer overwrites the freestyle input. Run `20261127_081833` on
generation 794 reaches car `z = 2.18` against a ball at `z = 1.18` (target above
the ball was ~2.16) and logs `vehicle.boost` during the climb. It still ends
`landed` with zero resets: the remaining gap is the reset/descent controller, not
the climb.
| N5 | Defense: meet a shot from the goal side, do not fly through the ball into the net | `arena.save` from an NPC, own-goal rate below 10 percent in a 5-minute run |

### 19.2 Controller rules

- The NPC uses the same `VehicleInput` surface as a human. No second vehicle
  solver.
- Aerial control is a position/velocity controller: compute the desired velocity
  from `(target - position) / timeToArrive`, clamp it, compute the body-rate error
  from the velocity error, and brake when closing fast. The old nose-at-target
  law overshot to 7 m above a 1 m ball (`issue-1ffbfffaefa0`).
- Aerials only start from behind the ball relative to the target goal. Starting
  from the goal side is an own-goal path.
- The NPC may reverse out of geometry only after it has throttle applied and has
  not moved for 45 ticks, and it must log `vehicle.npc.recover` with the reason.
- Team identity comes from the occupied car's `communityTeams` entry, never from
  spawn order.

### 19.3 Roster

1v1 is the first opponent slice. 2v2/3v3 remain data-driven rosters that reuse the
same controller; no per-slot AI code.

## 20. Physics parity matrix (2026-11-27)

Each row is a scenario and an acceptance value. A build is not evidence; the
named scenario's journal is. Values are compared with the §9.1 baseline and the
deviation is recorded.

| # | Scenario | Measurement | Acceptance |
|---|---|---|---|
| P1 | `rocket_ball_roll_v1` (mode 74, clean roll) | fitted late-roll `k` over >= 6 s, no wall contact | 0.030 +/- 0.003 /s |
| P2 | `rocket_ball_rest_v1` | rest centre height and z stability | radius +/- 2 mm, no sawtooth > 1 mm |
| P3 | `rocket_ball_bounce_v1` | floor and wall rebound at 20 m/s | restitution 0.6 +/- 0.05 on each surface |
| P4 | `rocket_ball_50_50_v1` | opposing cars, simultaneous approach | one authoritative first-contact tick and one ball outcome on both clients |
| P5 | `rocket_ball_hit_v1` | car-to-ball impulse at ~5 and ~23 m/s | follows the 0.65 -> 0.55 push curve, no double impulse |
| P6 | `rocket_ball_dribble_v1` | ball carried on the car roof | ball stays within 0.5 m of the roof for 1.5 s at > 8 m/s |
| P7 | mode 73 cruise | throttle + boost for 60 s on arena2 | no jitter, no fall-out-of-world, only wheels/roof settlements |
| P8 | `rocket_settle_fuzz_v1` | 55 random kicks | `side_rests == 0`, wheels landings dominate |

Rules:

- One decay constant for the ball: linear and angular damping are both the
  authored 0.03/s. Contact friction may not add rolling drag.
- The sphere contact uses the authored radius and zero positional slop. The
  shared depenetration solver's `penetration + slop` target is an actor rule and
  must not lift a sphere.
- A 50/50 is resolved by the server; the client never applies a confirmed
  impulse twice.
- Re-measure every row after a contact, ball, or vehicle-physics change and
  append the run ID. A stale row is unproven.

## 21. Free play and command surface (2026-11-27)

- Mode 60 is free play on arena2. D-pad left throws the ball at the local car:
  `speed = clamp(12 + 0.35 * distance, 12, 50)`, aimed at the car's nose. It logs
  `arena.freeplay.throw` with the distance, speed, and tick.
- `ball_set <x> <y> <z> [vx vy vz [wx wy wz]]` is server-authoritative. A
  client-side placement is overwritten; the command belongs on the server.
- `ball_status` logs one `arena.ball.status` sample on request.
- `hud_speed on|off`, `hud_car_assist on|off`, `vehicle_hitbox on|off`,
  `vehicle_cam_reload`, and `vehicle_profile` are presentation/configuration
  commands. None changes the physics tick.
- The free-play HUD is the arena HUD without score/clock (no match) plus the
  boost meter and ball-cam indicator.

## 22. Evidence ledger (2026-11-27)

| Area | Scenario / run | Required evidence | State |
|---|---|---|---|
| Ball clean roll | mode 74 | `arena.ball.state`, fitted `k` | pending build |
| Ball rest | mode 74 | `arena.ball.state` z stability | pending build |
| Spin bounce | modes 69/70 | mirrored horizontal deflection | proven (prior build) |
| Dodge 360 | `rocket_dodge_360_v1` | `rotation_deg_total`, `cancelled` | proven |
| Landing | `rocket_landing_flat_v1` | `up_dot`, `wheel_mask`, penetration | proven |
| Settle fuzz | mode 65 runs `20261127_165709`, `20261127_171939`, `20261127_180452` | `side_rests`, `wheels_landings`, `upright_partial_rests` | partial: 31/31 and 22/25 kicks clean; run `20261127_180452` found one real rest at up_dot 0.399 because throttle ended before the late settle. Recovery-window probe in gen 805 is the re-test. |
| NPC vs NPC | mode 66 | clock, goals, `arena.match.over` | proven (prior build) |
| NPC aerial | mode 67 | jump, boost, air roll, dodge, aerial goal | proven (prior build) |
| NPC freestyle | mode 72 run `20261127_081833`, gen 794 | `vehicle.flip.reset`, freestyle goal | partial: car reaches `z=2.18` above a `z=1.18` ball and boosts, but ends landed with 0 resets |
| Arena HUD | mode 60 run `20261127_080234`; mode 67 run `20261127_080258` | `vehicle.hud hud_mode=arena`, `arena.hud.snapshot` with phase/score/clock | HUD active and scoreboard data present; goal flash pending a scored goal |
| Speed/rpm default | mode 60 run `20261127_080234` | `vehicle.preferences hud_speed=false`; `vehicle.preference` on command; `vehicle.hud hud_speed=true` after | proven |
| Camera | mode 60 run `20261127_182931` (stiffness 1.0); mode 65 run `20261127_183456` (stiffness 0.0) | `vehicle.camera.follow`: `lag_tau_s`, `speed_mps`, `extra_distance_m`, `expected_extra_m` | 1.0 proven: lag_tau 0.0 and extra 0.000 m at 11-12 m/s. 0.0 proven: lag_tau 0.3078 s, extra 1.29 m at 4.23 m/s (expected 1.30). 0.5 has the measured tau 0.1539 s but still needs a moving sample. |
| Tick VFX | mode 60 | one event per cue, rate limit | not started |

## Related specifications

- [Vehicles](../vehicles/vehicles.md)
- [Gamemodes](gamemodes.md)
- [Objective and round HUD](objective-hud.md)
- [Moving physical objects](../moving-physical-objects/moving-physical-objects.md)
- [Networking](../networking/networking.md)
- [Replay](../replay/replay.md)
- [Destructible World](../destructible-world/destructible-world.md)

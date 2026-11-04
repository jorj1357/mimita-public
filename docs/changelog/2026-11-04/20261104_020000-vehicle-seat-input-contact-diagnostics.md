# Vehicle seat, input, and contact diagnostics

Date: 2026-11-04

## Purpose

Make the authored vehicle usable from the shared F interaction and make the
live behavior provable from `events.jsonl`:

- F/`vehicle_enter` reaches the authoritative server and seats the local actor.
- The seated origin is anchored to one seat owner and released on exit.
- The seated pose holds the authored crouch clip.
- Every input axis/edge is logged locally and after server application.
- Wheel contact, contact triangle, orientation, and bounce are logged numerically.
- Wheel size is data-driven and documented.

## Code

- `src/vehicles/vehicle-types.h`
  - Added seat `riderOffset` (default upward offset).
  - Added per-wheel compression velocity and contact triangle id.
- `src/vehicles/vehicle-config.cpp`
  - Parse/save `seats[].riderOffset`.
- `src/vehicles/vehicle-system.cpp`
  - `seatWorld` applies `anchor + riderOffset`; `localSeatWorld` now delegates
    to `seatWorld` so client, server, camera, and exit share one owner.
  - Wheel rays also query the world collision tree.
  - Anti-roll bar now uses compression + compression-velocity coupling.
  - Rebound damping is no longer clamped to zero; tire load stays non-negative.
  - Added an all-wheel-grounded stability/idle settle pass.
  - Added `vehicle.wheel.spec` (spawn) and `vehicle.bounce` (10 Hz) telemetry
    with per-wheel hub, contact point/normal, triangle id, load, suspension,
    compression, and slip.
- `src/sim/simulate-tick.cpp`
  - Seated branch holds crouch, sets grounded state, drives the shared
    procedural animation, and logs `vehicle.seat.anchor`.
  - Seated actors skip on-foot pose containment and actor-pair position writes.
- `src/network/multiplayer-reconcile.cpp`
  - Seated local players skip on-foot position reconciliation.
- `src/network/vehicle-networking.cpp`
  - Added `vehicle.input.local` and `vehicle.input.server` telemetry.
  - Added explicit server seat rejection reasons.
  - Authored spawns are ray-settled so all wheels start above the surface.
- `src/vehicles/vehicle-hud.cpp`
  - HUD now shows throttle, brake, steer, clutch up/down, handbrake, gear, mode.
- `src/vehicles/vehicle-commands.cpp`
  - `vehicle_spawn` ride height is derived from the template wheel data.
- `config/vehicles/car_sedan.json`
  - Larger wheels (radius 0.55, width 0.38), higher spring rate, new hubs,
    seat `riderOffset` **1.11 m** (cushion + capsule half-height).

## Evidence (live build 2026-11-04T01:44:33Z unless noted)

- F/`vehicle_enter` seat path:
  - `vehicle.seat.request.sent` -> `vehicle.seat.request.received` ->
    `vehicle.seat.entered` -> `vehicle.seat.server_enter` ->
    `vehicle.entered` -> `vehicle.seat.replicated`.
- Denied seat path:
  - `vehicle.seat.rejected` with `reason="seat index is invalid for this vehicle"`
    was received and cleared by `vehicle.seat.rejected.replicated`.
- Input:
  - `vehicle.input.server` showed `throttle=1.0`, `gear=1`, changing `speed`
    and position while a command-driven throttle was active.
- Seat:
  - `vehicle.seat.anchor` reports `has_local_driver=true`,
    `seated_entity_id` non-zero, and `crouching=true`.
  - The final capsule-center `riderOffset=1.11 m` reduced the logged seat snap
    from ~1.11 m to **~0.008 m** (8 mm), which is ordinary suspension motion.
    This also proved the earlier snap was the on-foot capsule-center offset
    being re-applied after the seat write.
- Wheels/contact:
  - `vehicle.wheel.spec` shows the loaded `edit_file` and per-wheel geometry.
  - `vehicle.bounce` shows `groundedCount`, `allWheelsGrounded`, each wheel's
    `triangleId`, `contactNormal`, `normalUpDot`, `compression`, `load`, and
    `suspensionLength`.
  - On flat triangle 8106 all four wheels were grounded with
    `normalUpDot=~0.99999`, while chassis compression was still bottomed,
    which isolates the remaining bounce to suspension/chassis clearance rather
    than missing wheel contact.

## Follow-up: local `vehicle_spawn` F regression (fixed)

- `activateVehicleInteraction`, `vehicle_enter`, and `vehicle_exit` now route to
  the authoritative seat request only for `serverMirror` vehicles. A local
  `vehicle_spawn` car has no server id, so it uses the local seat path.
- `interaction_activate` registration was moved out of the `interaction_list`
  loop.
- Added `interaction.key` telemetry per F edge.
- Live proof: `vehicle.spawned` → `interaction.key` (`selected=true`,
  `vehicle:1`, `activated=true`) → `vehicle.entered`, with no server seat
  request. Journal:
  `logs/2026-11-04/20261104_023511_01/events.jsonl`.
- Regression record:
  `docs/regressions/2026-11-04/vehicle-spawn-local-f-does-not-enter-REG.md`.

## Known gaps

- The debug spawns used for this session were on uneven map steps. The vehicle
  can still sink to the bump stop or land on its side on those authored points,
  so the live driving acceptance is not closed.
- The vehicle still shows a 60 Hz vertical/spring limit cycle on the test
  spawns. `vehicle.bounce` proves all four wheel rays hit the same flat
  triangle (`triangleId=8106`, `normalUpDot≈0.99999`) while
  `maxCompression` is at travel and `suspensionLength` is negative, so the
  chassis is below the bump stop. `serverSimulated=true`, `motion=Dynamic`,
  `sleeping=false`, and the entity mass is correct, ruling out skipped
  integration. The next step is a proper bump-stop / implied-spring
  integration rather than more force clamping.
- No clean two-client drive/weapon round trip was completed in this session.
- Agent gate was not run per operator instruction.

## Reproduction

1. Run `python devscripts/dev-loop.py` with a mode whose map has an authored
   vehicle.
2. `teleport` near it, press F, inspect `events.jsonl` for the seat chain.
3. Inspect `vehicle.seat.anchor`, `vehicle.input.local`, `vehicle.input.server`,
   `vehicle.bounce`, and `vehicle.wheel.spec`.


## Follow-up: template tuning, speed targets, coast and surface bounce

- All chassis/suspension/wheel/tire tuning now has template fields:
  `linearDamping`, `angularDamping`, `maxAngularSpeed`, sleep thresholds,
  `maxSuspensionForce`, `bumpStopStiffness`, `bumpStopDamping`,
  `antiRollForceScale`, stability/idle-settle values, `bodyFriction`,
  `bodyRestitution`, `coastDeceleration`, per-wheel `spinInertia`,
  `maxSpinVelocity`, `antiRollDamping`, and tire `lowSpeedGrip`,
  `rollingResistance`, `maxLoadScale`.
- `speedTargets` documents and validates 0-60 km/h and top speed. The live
  `vehicle.config.validation` event computes the same gearing/torque math:
  `config/vehicles/car_sedan.json` currently reports estimated 0.982 s to
  60 km/h and 999.2 km/h top speed, with no warnings.
- `vehicle_spawn` cars now use the template's automatic mode and start with
  the engine on, so W drives them immediately.
- Body-vs-world friction was lowered to 0.18 and restitution set to 0.10 so
  the car slides off ledges instead of snagging and gets a small bounce.
- Coast-down uses `coastDeceleration` plus rolling resistance, so a released
  throttle eventually stops the car instead of coasting forever.
- Live evidence: `logs/2026-11-04/20261104_033701/events.jsonl`
  (`vehicle.tuning`/`vehicle.config.validation`, no warnings) and
  `logs/2026-11-04/20261104_033944_01/events.jsonl`
  (`vehicle.bounce` speed ~18.8 m/s after a one-second throttle command,
  i.e. above the 16.67 m/s 60 km/h target).


## Follow-up: engine off start, HUD key hints, gear shift, coast/bounce

- Cars now spawn with the engine off. The existing bottom-left vehicle HUD
  shows `V START ENGINE` (or `V ENGINE OFF`), `B AUTO/MANUAL`,
  `X SHIFT UP`, `Z SHIFT DOWN`, clutch, throttle/brake/steer, handbrake, and F.
- Sequential X/Z shifts now work in automatic mode as well as manual; a
  successful shift sets a 1.5 s auto-shift lock so the automatic selector does
  not immediately downshift back.
- Added `coastDeceleration` and `coastSpinDamping`: releasing throttle/brake
  applies a fixed deceleration and decays residual wheel spin, so the car
  comes to rest instead of coasting forever.
- Idle settle now also runs near the bump stop and zeroes most residual wheel
  spin.
- `bodyFriction=0.18`, `bodyRestitution=0.10` are template fields so the car
  slides off surfaces instead of snagging and has a small 0.1 bounce.
- Live checks: `vehicle.bounce` showed engine off at spawn, then engine on
  after `vehicle_press 1 engine`, gear 2 after shift-up, and a one-second
  throttle moving the car to about 18.8 m/s (above the 16.67 m/s 60 km/h
  target). Template validation reports estimated 0.982 s to 60 km/h and
  999.2 km/h top speed with no warnings.


## Follow-up: HUD live gear updates and all gears

- The bottom-left HUD is drawn every render frame from the live local
  `VehicleState`, so its gear label (`R`, `N`, `1`-`6`) updates on the same tick
  as the drivetrain.
- Manual X/Z shifts are now allowed in automatic mode and hold for a 1.5 s
  auto-shift lock, so automatic mode cannot immediately downshift them back.
- Neutral and reverse are held during that lock; automatic re-selection runs
  only after the lock expires.
- Added `vehicle.gear.changed` telemetry with vehicle id, gear, mode, engine
  state, RPM, speed, and remaining auto-shift lock.
- Live proof: `vehicle.gear.changed` walked `1 -> 2 -> 3 -> 4 -> 5 -> 6 ->
  5 -> 4 -> 3 -> 2 -> 1 -> 0 -> -1` on the client. Journal:
  `logs/2026-11-04/20261104_040647_01/events.jsonl`.


## Follow-up: drift tuning, differentials, wall slide, wheel caps

- Wheel rendering: fixed bottom/top cap triangle winding, radial cap UVs,
  opaque wheel alpha, and cull-state save/restore. Hub faces now render with
  the wheel texture instead of being culled.
- A/D steering: `steering.directionSign` now makes positive input steer right;
  added Ackermann and per-wheel toe. `vehicle.steer` logs input, front wheel
  angles, yaw, and yaw delta.
- Manual is the default. Cars spawn in `Manual`, clutch up, engine off, gear 1;
  `V` starts, `Left Shift` disengages, X/Z shift while clutch is held.
- Bounce: added `reboundScale=0.55`, `bodyRestitution=0.18`, and wall-specific
  `bodyWallFriction=0.05`; `vehicle.wall.contact` logs shallow wall hits.
- Drift schema: per-wheel `camberDeg`, `toeDeg`, `casterDeg`; tire
  `slipStiffness`, `camberGain`; steering `directionSign`, `ackermann`;
  engine `clutchKickBoost`; brakes `handbrakeGripScale`.
- Differentials: `welded` (default), `race_lsd`, and `open` with preload,
  power/coast lock, lock strength, and bias ratio. Welded forces both driven
  wheels on an axle to one spin speed; race LSD transfers torque toward the
  slower wheel. `vehicle.bounce` logs differential type and wheel spin.
- HUD now includes differential type and remains live at 1024x768 design
  coordinates; `vehicle.hud` logs gear/mode/engine/screen size once per second.
- Clutch kick emits `vehicle.clutch.kick` and applies a temporary torque boost.

Evidence:
- `logs/2026-11-04/20261104_043035/events.jsonl`:
  manual `vehicle.gear.changed` to gear 2 with clutch held, `vehicle.steer`
  input +1 with directionSign -1 and negative yaw delta (right), welded diff
  rear spins equal in `vehicle.bounce`.
- `logs/2026-11-04/20261104_043629_01/events.jsonl`: live `vehicle.hud` with
  `mode=MANUAL`, `engine_on=false`, `gear=1`, `differential=welded`.
- Builds: `2026-11-04T04:30:26Z`, `2026-11-04T04:37:33Z`.

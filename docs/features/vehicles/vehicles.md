2026-10-27

# Vehicles (general physical library)

## Purpose

Provide one general, data-driven vehicle library that any mode can use: a
chassis physical body, N suspension/wheel units, a sim-lite drivetrain, seats,
player control, and reusable generalized constraints (point, hinge,
slider/spring, motor). A car, truck, payload vehicle, bike, skateboard, or
future air vehicle must differ by data, not by a separate physics
implementation.

## Desired behavior

- Vehicles drive with suspension, weight transfer, tire grip, engine inertia,
  turbo lag, gears, and clutch behavior at the fixed 60 Hz tick.
- Templates live in `config/vehicles/*.json`; mass, density, wheels, tires,
  engine, gearbox, brakes, seats, and steering are tunable and hot reloadable.
- `F` enters/exits the seat the player is aiming at. A valid seat wins over
  revive.
- Driving controls replace normal locomotion: `W` throttle, `S` brake,
  `A`/`D` steer, `Space` handbrake, `V` engine on/off, `B` automatic/manual,
  `Left Shift` clutch, `X`/`Z` shift up/down in manual.
- Manual is PRNDL-style: `R`, `N`, `1`-`6`; shifting requires the clutch.
- Automatic uses `W` forward and `S` brake/reverse.
- Holding clutch and throttle produces a physics clutch kick; the emergency
  brake locks rear wheels and produces physics slip. Neither is a scripted mode.
- The HUD shows engine state, mode, gear, RPM, speed, clutch, brake, and
  handbrake while seated.
- Ragdoll releases the seat, stops driving input, and lets the body collide and
  bounce inside the vehicle. Weapons still use the physical aim body while
  seated.
- Constraints are first class and reusable: hinge, slider/spring, point, motor.
- The hinge demo proves the constraint primitive: spawn two blocks, anchor one,
  hinge the other so it swings.

## Current behavior

Implemented foundation:

- Generalized `Constraint` record and fixed-tick solver for point, hinge
  (including axis alignment and motor), and slider/spring (including limits).
  Integrated into the physical-entity fixed tick.
- Constraint console surface and two-block hinge/spring demo.
- Vehicle template registry with hot reload and a neutral sedan template.
- Vehicle chassis as a dynamic `PhysicalEntity`; suspension raycast against the
  cached world broadphase (including large ground triangles); spring, damper,
  bump stop, and tire slip model; engine torque curve, turbo, manual/automatic
  gearbox, clutch, brakes, handbrake, engine reaction torque, drag.
- Console commands for spawn, list, despawn, enter/exit, input injection,
  one-shot control edges, and reload.
- F seat interaction targeting, seat-origin lock, seated aim-body update, camera
  follow, and vehicle HUD.
- Structured `vehicle.*` and `constraint.*` events in `events.jsonl`.

Not implemented yet:

- Server-authoritative vehicle networking and driver input replication.
- Map editor vehicle placement and constraint persistence to
  `config/maps/<mapId>.json`.
- Rendering the wheel cylinders or a vehicle model (chassis draws through the
  existing physical-entity box renderer).
- Destruction hooks and detachable wheels.
- Ragdoll-versus-chassis collision (ragdoll currently collides with world
  geometry only, so a ragdolled occupant can fall through the car).
- Full driving-feel acceptance on a known clean flat surface. Runtime state
  proves the car spawns, grounds, loads suspension, starts, and produces engine
  torque; it does not yet prove a polished lap.

## Current status, from working best to not working at all

- Implemented and building: constraint primitive, hinge demo, vehicle template
  load, spawn, suspension/tire/drivetrain math, console surface, seat/camera/HUD.
- Implemented but unverified for feel: clutch kick, weight transfer, tire
  saturation, automatic shifting.
- Not implemented: networking, map-editor authoring, destruction, wheel render.
- Known issue: ragdoll occupants do not collide with the chassis.

## Decisions

- Reuse `PhysicalEntity` and `PhysicalEntitySystem`; do not add an external
  physics engine or a second rigid-body owner.
- Wheels are unsprung suspension units with scalar spin, not separate rigid
  bodies, in the first slice.
- `F` is the interaction key; a seat target wins over revive.
- Manual selector is `R, N, 1..6`.
- `V` toggles engine; `B` toggles automatic/manual.
- `Left Shift` is clutch; `X`/`Z` shift only when the clutch is held.
- The first template is `car_sedan`; trucks/bikes/skateboards are later data.

## Ownership

- Primary code owner: `src/physics/constraints/*`, `src/vehicles/*`.
- Configuration owner: `config/vehicles/*.json`.
- Runtime/event owner: `StructuredLogger` category `Physics`, events
  `vehicle.*` and `constraint.*`.
- Network owner, if applicable: not implemented yet.
- Animation/physics owner, if applicable: `PhysicalEntitySystem` fixed tick,
  `simulateTick`, `engineTickCamera`, `engineTickUIGameHUD`.

## Related authoritative documents

- Specification: `docs/specs/vehicles/vehicles.md`
- Parent specification:
  `docs/specs/moving-physical-objects/moving-physical-objects.md`
- Architecture: `docs/architecture/collision/collision.md`,
  `docs/architecture/terminal-commands/terminal-commands.md`
- Workflow: `docs/workflows/runtime-scenario-validation.md`
- Focused review skill: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`,
  `docs/skills/terminal-command-checker-v1.md`
- Regression record: `docs/regressions/regressions-v1.md` (none added)

## Relevant files

- `docs/specs/vehicles/vehicles.md`
- `config/vehicles/car_sedan.json`
- `src/physics/constraints/constraint.h`
- `src/physics/constraints/constraint-system.{h,cpp}`
- `src/physics/constraints/constraint-commands.{h,cpp}`
- `src/vehicles/vehicle-types.h`
- `src/vehicles/vehicle-config.{h,cpp}`
- `src/vehicles/vehicle-system.{h,cpp}`
- `src/vehicles/vehicle-commands.{h,cpp}`
- `src/vehicles/vehicle-seat.{h,cpp}`
- `src/vehicles/vehicle-hud.{h,cpp}`
- `src/physics/physical-entity.{h,cpp}` (fixed-tick callback, shared body
  queries)
- `src/input/input-commands.{h,cpp}` (raw key query)
- `src/input/input-poll.cpp` (F interaction)
- `src/sim/simulate-tick.cpp` (seat lock)
- `src/engine/engine-tick-camera.cpp` (seat camera)
- `src/engine/engine-tick-setup.cpp` (hot reload)
- `src/engine/engine-tick-ui-game-hud.cpp` (HUD call)
- `src/main-systems.cpp` (registration/init)

## Tests and evidence

- Automated tests: none added. Pure tire/gearbox math tests are still a
  follow-up.
- Runtime commands:
  - `vehicle_reload`
  - `vehicle_spawn car_sedan`
  - `vehicle_list`
  - `vehicle_input 1 1 0 0 1 0`
  - `vehicle_press 1 engine`
  - `vehicle_input 1 1 0 0 0 0`
  - `constraint_demo hinge`
  - `constraint_list`
- Logs: `logs/2026-10-27/20261027_045816/events.jsonl` and
  `logs/2026-10-27/20261027_051322/events.jsonl`
- Build: dev-loop build 3444, `Status: SUCCESS`.
- Human playtest: not performed.

## Acceptance criteria

- `vehicle_spawn car_sedan` reports `vehicle.spawned`. (pass)
- `vehicle_list` reports four wheel entries with `rayHit`/`grounded` true on
  flat ground. (observed on one dust2 position; pass)
- `vehicle_press <id> engine` sets `engineOn=true`; `vehicle_input ... 1 ...`
  raises RPM/torque and moves the car. (observed; feel not accepted)
- `constraint_demo hinge` reports `constraint.created`. (pass)
- `constraint_list` shows the created constraint. (pass)
- F enters/exits the aimed seat, F does not revive while a seat is targeted.
  (implementation present; human review required)
- HUD shows engine/mode/gear/RPM/speed while seated. (implementation present;
  human visual review required)
- Server-authoritative multiplayer driving. (not implemented)
- Map editor vehicle placement/constraint persistence. (not implemented)
- Destruction and detachable wheels. (not implemented)

## Changelog and regression links

- Changelog: `docs/changelog/2026-10-27/20261027_051000-vehicles-general-library.md`
- Regression: none appended

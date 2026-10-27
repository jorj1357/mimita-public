// 2026-10-27T05:10:00Z

# Task

- Task ID: vehicles-general-library-foundation
- Summary: Add the general vehicle library foundation: reusable point/hinge/
  slider constraints, a data-driven sedan, suspension/tire/drivetrain
  simulation, seat interaction, HUD, and console surface.
- Status: partial implementation; build and runtime diagnostics pass; driving
  feel, networking, editor authoring, and destruction remain unverified or
  unimplemented.
- Date, time, timezone: 2026-10-27T05:10:00Z, ISO 8601
- Branch: 20261011zt
- Base commit: 380396b0
- Final commit: not committed (working tree)

# Pre-existing changes

- `git status --short` contains many unrelated modified and untracked files from
  other work, including `config/accounts/default.json`, `src/network/*`,
  `src/combat/*`, `config/weapons.json`, and new weapon assets.
- Files this session created: `src/physics/constraints/*`,
  `src/vehicles/*`, `config/vehicles/car_sedan.json`,
  `docs/features/vehicles/vehicles.md`, and this changelog.
- Files this session modified: `docs/specs/vehicles/vehicles.md`,
  `src/physics/physical-entity.{h,cpp}`,
  `src/input/input-commands.{h,cpp}`, `src/input/input-poll.cpp`,
  `src/sim/simulate-tick.cpp`, `src/engine/engine-tick-camera.cpp`,
  `src/engine/engine-tick-setup.cpp`,
  `src/engine/engine-tick-ui-game-hud.cpp`, `src/main-systems.cpp`.
- Other modifications in those directories were not made by this session and
  were preserved.

# Requested behavior

- Implement vehicles as a general library usable by every mode.
- Sim-lite BeamNG-like feel: suspension, weight transfer, mass, inertia, grip,
  engine/turbo/gearbox, JSON tuning, strong C++ core.
- Reuse the physical-object system and destructible objects where practical.
- Add generalized entity attachment/constraints and a hinge demo.
- Add console commands, in-game HUD, F enter/exit, and car-only controls.
- Controls: `W` throttle, `S` brake, `A`/`D` steer, `Space` emergency brake,
  `V` engine toggle, `B` automatic/manual, `Left Shift` clutch, `X`/`Z` shift.
- Manual PRNDL: `R, N, 1..6`.
- F seat target wins over revive.

# Specification alignment

- Current specification: `docs/specs/vehicles/vehicles.md` (rewritten this
  session) and `docs/specs/moving-physical-objects/moving-physical-objects.md`.
- Exact requirements: vehicles are a general assembly of physical bodies,
  analytic shapes, and constraints; constraints are first-class; vehicle
  behavior is data-driven and server-authoritative eventually.
- Why this change follows the specification: it adds the missing generalized
  constraint owner and the first vehicle assembly without creating a
  car-specific physics branch.
- Conflicts or decisions: server networking, editor persistence, and
  destruction are explicitly deferred in the feature record.

# Exact implementation changes

## File: `docs/specs/vehicles/vehicles.md`

- Replaced the 30-line wishlist with a full specification: library rule,
  primitives, template JSON, suspension/tire/drivetrain, exact controls, seats,
  HUD, networking, authoring, destruction hooks, commands, ownership, phases.

## File: `src/physics/constraints/\*`

- New `Constraint` record (`Point`, `Hinge`, `Slider`), stable IDs, anchors,
  axes, limits, spring/damper, motor, break fields.
- New `ConstraintSystem` solves at the fixed tick using `PhysicalEntity` mass,
  center of mass, velocity, and inverse inertia.
- New terminal commands: `constraint_create`, `constraint_spring`,
  `constraint_motor`, `constraint_list`, `constraint_delete`,
  `constraint_clear`, `constraint_demo`.

## File: `src/physics/physical-entity.{h,cpp}`

- Added public body queries used by constraints
  (`physicalEntityCenterOfMassWorld`, `physicalEntityPointVelocity`,
  `physicalEntityInverseInertiaWorld`, `applyPhysicalEntityAngularImpulse`).
- Added a fixed-tick callback list and call it before entity integration.
- Constraint solve runs after entity contacts.

## File: `src/vehicles/\*`

- Added template/runtime types, JSON registry with hot reload, spawn/simulation
  system, seat ray target, HUD, and commands.
- Suspension uses a cached broadphase ray (including large world triangles),
  spring/damper/bump stop, and applies forces at the chassis hub.
- Tire model: slip ratio/angle, load sensitivity, friction circle, low-speed
  regularization, contact force and wheel spin reaction.
- Drivetrain: torque curve, RPM follow, turbo spool, manual/automatic gearbox,
  clutch, brakes/handbrake, engine torque reaction, drag.
- Seat origin lock, camera follow, and physical aim-body update while seated.

## File: `src/input/input-commands.{h,cpp}`, `src/input/input-poll.cpp`

- Added `isKeyHeld` for context-specific keys without stealing existing binds.
- F interaction: enter/exit a targeted seat; revive suppressed while seated or
  when the seat interaction consumes F.

## File: `src/sim/simulate-tick.cpp`

- Seated actors bypass locomotion, lock to the seat pose, keep the aim body
  active, and re-assert the seat origin after contact passes.

## File: `src/engine/\*`, `src/main-systems.cpp`

- Camera follows the occupied vehicle; HUD draws vehicle state; vehicle config
  polls in hot reload; commands and fixed-tick callback are registered.

# Diagnostics

- Owner/category: `StructuredLogger`, category `PHYSICS`.
- Input: terminal commands and fixed-tick vehicle state.
- Decision: log spawn/config/state/enter/exit and constraint creation.
- Output: `vehicle.config.loaded`, `vehicle.spawned`, `vehicle.state`,
  `vehicle.entered`, `vehicle.exited`, `constraint.created`.
- Failure or rejection reason: commands log unknown templates/ids.
- Rate limiting: state events only emit on `vehicle_list`, not per frame.

# Validation

- Focused skill paths and results:
  - `docs/skills/spec-behavior-review-v1.md`: spec rewritten, decisions
    recorded, unresolved items listed.
  - `docs/skills/logging-checker-v1.md`: owner-level events added; no per-frame
    spam.
  - `docs/skills/terminal-command-checker-v1.md`: commands have one owner,
    usage, and reuse the shared VehicleSystem/ConstraintSystem actions.
- Tests and exact commands:
  - `python devscripts/dev-loop.py --request build --build-mode debug --wait-for-build`
  - `python devscripts/dev-loop.py --command "vehicle_reload"`
  - `python devscripts/dev-loop.py --command "vehicle_spawn car_sedan"`
  - `python devscripts/dev-loop.py --command "vehicle_list"`
  - `python devscripts/dev-loop.py --command "vehicle_input 1 1 0 0 1 0"`
  - `python devscripts/dev-loop.py --command "vehicle_press 1 engine"`
  - `python devscripts/dev-loop.py --command "vehicle_input 1 1 0 0 0 0"`
  - `python devscripts/dev-loop.py --command "constraint_demo hinge"`
  - `python devscripts/dev-loop.py --command "constraint_list"`
- Build status: dev-loop build 3444, `Status: SUCCESS`.
- Runtime or hot-reload evidence: runs `20261027_045816` and
  `20261027_051322`, `logs/2026-10-27/<run>/events.jsonl`.
- Output files: `events.jsonl`.

# Measured evidence

- Before values: no vehicle or constraint code existed.
- After values:
  - `vehicle.spawned` with `template=car_sedan`, `groundHit=true`.
  - `vehicle.state` with four grounded wheels and suspension compressions in
    travel on one dust2 position (compression ~0.135, load ~24k per wheel).
  - `vehicle.state` after `vehicle_press 1 engine` and throttle shows
    `engineOn=true`, non-zero RPM/torque, and position change over time. A later
    position bottomed the suspension against uneven terrain, so feel is not
    accepted.
  - `constraint.created` with `type=hinge`, `entityA=2`, `entityB=3`.
- Timestamps: 2026-10-27T05:08Z to 2026-10-27T05:16Z UTC.
- Tick/frame/network measurements: fixed-step vehicle state; no network
  replication yet.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: no previously working behavior
  was reported broken by this work; existing gameplay paths were only extended
  with additive calls and key queries.
- Related regression paths: none.

# Human acceptance

- Visual review: not performed. The chassis renders through the existing
  physical-entity box renderer; wheels are not rendered.
- Gameplay review: not performed. The car spawns, grounds, starts, and produces
  wheel torque in the runtime journal, but driving feel, weight transfer,
  clutch kick, and HUD readability need human review on a clean flat area.
- Multiplayer review: not performed; vehicle networking is not implemented.
- Still unverified: map-editor placement/constraint persistence, destruction,
  ragdoll-versus-chassis collision, automatic shifting feel, tire tuning.

# Related feature record

- Feature path: `docs/features/vehicles/vehicles.md`

---

# Follow-up implementation (same session)

- Final build: dev-loop build **3470**, `Status: SUCCESS`.
- Runtime journal: `logs/2026-10-27/20261027_072146/events.jsonl`.
- Scenario: `devscripts/scenarios/vehicle_drive_v1.json` passed (request
  `f8bb8a65`) with 25 `vehicle.tick.summary` samples and a 4.4 m origin move.

## Additional changes

- New general `InteractionSystem` and `interaction_list` /
  `interaction_activate` commands. Large world-anchored `Press F to ...`
  billboard using `DebugVis::projectToScreen` + `uiDrawText`/`uiDrawRect`.
  Seats register targets; a valid target wins over revive; the seated exit
  target is sticky; prompt shown/hidden is logged on change with a debounce.
- Car template is 5 m x 3 m x 1 m with 0.3 m smooth cylinder wheels, body
  texture `240sx.png`, wheel texture `dark1.png`, and hot-reloadable per-wheel
  `spinAxis`/`steerAxis`.
- New shared `src/render/procedural-cylinder.*`; `src/vehicles/vehicle-render.*`
  draws the wheels from runtime state.
- Void death: `VehicleSystem::checkVoidDeath` despawns a car below killZ,
  unseats and kills an occupant through the existing void-death path, and logs
  `vehicle.void`. Seat reassertion is skipped when dead/void/gone.
- New `src/physics/runtime-objects.*` and `runtime_clear`/`runtime_status`.
  Cleanup is called on listen-server start/stop, map change, main menu, network
  shutdown, and client map switch. Vehicles/constraints are never persisted.
- `PhysicalEntitySystem` now has an id→index lookup; `clear()` releases
  generated meshes.
- Seated player skips actor-vs-occupied-vehicle contacts and player pushes via
  `Player::seatedVehicleId`.
- One broadphase gather per vehicle per fixed tick covers all wheel rays.
- `vehicle.tick.summary` and `vehicle.performance` telemetry include origin,
  spawn origin, distance, velocity, angular velocity, mass, weight, and full
  per-wheel center/axes/spin/load/slip data.
- Tick-bound `vehicle-audio` pulse scheduler: 0.8 s at low RPM to 0.02 s at high
  RPM, floor one 60 Hz tick, owner/retrigger, stopped on engine off/despawn/void.
- Constraint debug renderer `constraint-debug` is gated by `entity_visibility`
  and draws anchors, axes, slider limits, and labels.
- `vehicle_tune`, `vehicle_teleport`, and scripted input override.
- Scenario runner now supports `event_field_at_least` / `event_field_at_most`.

## Additional validation

- `interaction.activated` observed manually with label `enter driver seat` and
  a subsequent `vehicle.state` showing `localDriver=true`.
- `vehicle.tick.summary` observed wheel centers, spin axes (~lateral Y rotated
  by yaw), grounded flags, mass 1400, weight 98000, and origin distance.
- `vehicle.performance` observed `updateMs` ~0.008 ms for one vehicle and one
  physical entity.
- `vehicle.void` observed for an unoccupied car spawned below killZ; the car was
  despawned.
- Scenario passed after fixing one-shot input edges for local scripted control.

## Remaining human review

- Visual review of the 5 x 3 x 1 body, wheel cylinders, and the big interaction
  billboard.
- Audio review of the RPM pulse cadence.
- Occupied void-death visual respawn flow.
- Multiplayer/server-authoritative driving, map-editor authoring, destruction,
  and ragdoll-vs-chassis collision remain unimplemented.


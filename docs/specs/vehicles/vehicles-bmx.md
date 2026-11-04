# BMX Vehicles

**Purpose:** define the deferred BMX direction on MiMITA's shared vehicle
platform.
**Audience:** vehicle, animation, input, map-editor, NPC, networking, and
Skatepark contributors.
**Status:** deferred desired behavior; no first-playable BMX slice yet.
**Does not:** require a BMX-specific physics engine, duplicate the skateboard
input contract, or move BMX ahead of the online car.

## Relationship to skateboard

BMX shares the arcade trick-vehicle contract in
`docs/specs/vehicles/vehicles-skateboard.md`. The first BMX template should use
the same control profile unless a measured handling difference requires a
separate profile. Board and bike templates may tune balance, traction, gravity,
jump, and steering independently.

The BMX is an assembly on the general vehicle platform, not a new subsystem.
It uses the same fixed-tick simulation, analytic wheel path, collision world,
seat/attachment ownership, server authority, and replication as the car and
skateboard.

## Intended feel

The first BMX target is an arcade playground bike:

- easy to accelerate, steer, brake, and jump,
- readable lean and air control,
- ramps, gaps, bowls, and rails remain usable,
- the rider stays attached during ordinary riding,
- a missed landing costs momentum, not control of the entire game.

Simulation-grade bicycle dynamics, wheelie physics, and drivetrain simulation
are not required for the first slice.

## Authoring model

- A BMX template is a reusable `config/vehicles/<id>.json` definition.
- The chassis uses primitive parts (`box`, `slope`, `sphere`, `cylinder`).
- Front and rear wheels are analytic wheel records, not body parts.
- The fork, handlebar, crank, and rider attachment points are named
  attachments.
- The map editor places a BMX instance; the vehicle builder edits the template.
- Rendering may merge coplanar primitives; collision uses generated triangles.
- The rider seat/attachment uses the shared occupancy layer.

## Controls

The initial profile matches the skateboard/BMX arcade profile:

| Input | Grounded | Airborne |
|---|---|---|
| W | Accelerate | Limited forward influence |
| S | Brake, then reverse | Brake/air control as tuned |
| A / D | Steer and lean | Rotate about world Z |
| Space held | Crouch/compress and charge | No extra action |
| Space released | Jump; longer hold is higher | No extra action |

All actions must be remappable and usable from controller or touch bindings on
supported platforms. A mode may select the arcade profile without changing the
BMX template's physical values.

## Online and NPC requirements

- The server owns BMX spawn/despawn, chassis motion, wheel spin, steering, seat
  occupancy, and jump state.
- Clients send the shared vehicle actions and interpolate the authoritative
  mirror; they do not simulate authoritative BMX forces.
- A client joins an existing BMX through reliable spawn state and continues
  receiving compact visual state.
- NPC riders use the same vehicle input surface and behavior profiles as
  players. A BMX must not need a separate NPC physics path.
- Missing templates, seats, or attachments fail closed with diagnostics.

## Acceptance (future slice)

1. A BMX can be placed in the map editor, spawned, entered, ridden, and exited.
2. Short and long Space holds produce visibly different jump heights.
3. The rider stays aligned through ordinary riding and jumps.
4. A/D gives clear ground lean and controllable airborne rotation.
5. Ramps and triangle seams do not cause persistent snagging or uncontrolled
   bounce.
6. Two clients agree on BMX position, wheel state, and rider occupancy.
7. Tuning runs through the shared vehicle owner and template JSON.

## Deferred

- wheelie/manual physics,
- full bicycle drivetrain and chain simulation,
- advanced tricks and scoring,
- rail grinding,
- BMX-specific damage and destruction,
- procedurally generated skatepark content.

## Related specifications

- [Vehicles](vehicles.md)
- [Skateboard and BMX controls](vehicles-skateboard.md)
- [Skatepark](../gamemodes/skatepark.md)
- [Moving physical objects](../moving-physical-objects/moving-physical-objects.md)
- [Networking](../networking/networking.md)

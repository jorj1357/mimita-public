# Skateboard and BMX Vehicles

**Purpose:** define the arcade trick-vehicle direction for skateboards and BMX
bikes.
**Audience:** vehicle physics, animation, input, map, and gamemode contributors.
**Status:** deferred desired behavior; tuning and visual acceptance are pending.
Skateboards and BMX are not part of the first online car slice (2026-11-03).
**Does not:** require simulation-grade skateboarding, define a complete trick
scoring system, or make a separate physics engine for each vehicle.

## Design goal

Skateboards and BMX bikes should be fast to understand and fun to ride. The
initial feel is an arcade movement playground inspired by Tony Hawk Pro Skater,
Skate, and the playful handling of Roblox vehicles. It should support ramps,
gaps, bowls, rails, and improvised tricks without requiring advanced controls.

The skateboard and BMX are assemblies in the general vehicle system. Shared
physical entities, collision, constraints, fixed-tick simulation, and
replication remain the foundation. Each template has separate balance,
traction, gravity, steering, and jump tuning.

## Initial keyboard controls

| Input | Grounded behavior | Airborne behavior |
|---|---|---|
| W | Accelerate forward | Maintain or influence forward drive as tuned |
| S | Brake; continue input to reverse | Brake/air control as tuned |
| A / D | Steer and lean left/right | Rotate about world Z (up) |
| Space held | Crouch/compress and charge a jump | No extra action in the first profile |
| Space released | Jump; a longer grounded hold produces a higher jump | No extra action in the first profile |

The same action map applies to skateboard and BMX at first. Tuning may make
pedaling, braking, balance, and jump response feel different without changing
the input contract. All actions must be remappable and accessible through
controller or touch bindings on supported platforms.

## Handling and tricks

- Keep the rider attached to the board or bike during ordinary riding.
- Use the shared actor animation and pose systems for leaning, crouching, and
  airborne poses.
- Preserve enough air control to make ramps and jumps expressive; gravity and
  rotation are tunable rather than dictated by simulation realism.
- Basic steering and jump release should work without a trick-combo input.
- Future mode-specific tricks may add score or timed power, but the vehicle
  physics system does not award points or special-case a mode.
- Collisions with scenery use the shared physical collision world.

The Skatepark mode is the tuning and practice home. It should make repeated
spawn, ride, jump, fall, and restart cycles easy.

## Acceptance criteria

1. A new player can accelerate, steer, brake, and jump without reading a
   control manual.
2. A short and long Space hold produce visibly different jump heights.
3. Board and rider remain aligned through ordinary ground riding and jumps.
4. A/D produces clear ground lean and controllable airborne rotation.
5. The player can ride ramps and cross triangle seams without persistent
   snagging or uncontrolled bounce.
6. All tuning runs through the shared fixed-tick vehicle owner and can be
   observed and adjusted without changing mode-specific physics code.

## Future work

Trick combinations, rail grinding, manuals, spins, scoring, replays, NPC
riding, and recorded-input NPC performances are later slices. A long recorded
performance matching across machines is an aspirational replay research goal,
not an acceptance requirement for the first board or bike.

## Related specifications

- [Vehicles](vehicles.md)
- [BMX](vehicles-bmx.md)
- [Skatepark](../gamemodes/skatepark.md)
- [Replay](../replay/replay.md)
- [Moving physical objects](../moving-physical-objects/moving-physical-objects.md)

## Shared vehicle-platform requirements (2026-11-04)

The skateboard is a future assembly on the same general vehicle platform as the
car. The online-car slice established these requirements for it:

- Wheels remain analytic suspension units with scalar spin; do not model a
  skateboard wheel as an ordinary body primitive.
- The board/rider assembly must use server-authoritative motion and the shared
  physical-entity replication path.
- Seat/attachment occupancy, entering, and exiting must use the shared
  occupancy and interaction owners, not a skateboard-only seat path.
- The primitive builder (`box`, `slope`, `sphere`, `cylinder`) provides the
  visual/collision body; wheels and trucks are authoring attachments.
- The map editor places board instances; the builder edits the reusable
  template. Neither path may create a second physics world.
- A client mirror never simulates authoritative board forces. It interpolates
  the replicated chassis and visual wheel/truck state.
- A missing template or malformed board spawn fails closed with a diagnostic.

These requirements prevent the skateboard/BMX work from duplicating networking,
seat, or collision code when their first playable slice begins.

# Need for Madness

**Purpose:** specify an arcade vehicle race where tricks create speed and
vehicle collisions can decide the race.
**Audience:** vehicle, race-mode, NPC-driving, map, damage, networking, and
replay contributors.
**Status:** first-prototype direction; tuning and competitive rules are
unverified.
**Does not:** replace the general vehicle system, promise a procedural world,
or require a large vehicle roster for the first map.

## Core loop

Players and NPC drivers line up, see a 3-2-1-GO countdown, and race through a
course. A racer can follow the route and hit checkpoints, perform tricks for a
temporary power increase, or use physical collisions to damage, disable, or
total opponents' vehicles. The combination of racing and contact is
intentional; both paths must remain viable.

## First playable slice

- Use one authored map with a start line, ordered checkpoints, ramps, and
  optional airborne hoop volumes. Crossing a hoop's authored volume records
  its checkpoint or bonus once for the current race; merely passing near it
  does not count.
- The server owns the race phase, checkpoint progress, trick rewards, vehicle
  damage, and results.
- The round starts with the shared countdown and ends when the first eligible
  racer completes all required checkpoints and crosses the finish, or the
  configured match limit resolves the results.
- NPC drivers use the same vehicle controls and collision systems as players.
  Their route and recovery behavior must respect the same checkpoints.
- Use the shared fixed-tick vehicle physics and physical-entity collision
  owners. Keep vehicle state authoritative and replicate it through the
  established vehicle/networking path.

## Tricks and power

Jumping and airborne movement are available through the shared arcade trick
control profile. A confirmed trick awards a short configurable power increase;
more difficult validated tricks may award more power. Power affects vehicle
acceleration or available drive force through the mode's configured reward,
not a separate Need for Madness physics solver. Define a clear scoring window
so a single airborne pose cannot score repeatedly every tick.

The exact trick catalog, score values, and boost duration are balancing
decisions for playtests. A racer must still be able to finish without tricks.

## Vehicle contact and damage

Vehicle collisions transfer momentum through the shared physical system.
Damage and disablement use the shared impact/damage owner; they must not be
implemented as a race-only collision rule. A vehicle may be totaled and
removed from the race through shared damage and destruction behavior; this
does not imply that its occupant is killed. Vehicle templates may differ in
mass, density, durability, acceleration, and speed. A very heavy, durable,
slow construction vehicle is a desired roster archetype; the old “Masheen”
idea is a tuning reference, not a fixed stat contract.

Destructible scenery follows the shared destructible-world policy. Each mode
may explicitly disable world destruction if its map or performance budget
requires it.

## Controls

The mode selects the arcade vehicle control profile. W/S drive and brake,
A/D steer and lean, and Space charges/releases a ground jump. The same profile
is used by players and NPCs; NPCs provide actions through the shared vehicle
input surface.

## Acceptance

1. Player and NPC racers complete the same ordered checkpoints on the one
   prototype map.
2. A missed checkpoint cannot count as a race finish.
3. A single valid trick produces one boost event; repeated grounded or
   airborne frames do not farm the same trick.
4. Vehicle impacts transfer motion and apply damage through shared owners.
5. Heavy and light vehicle templates show meaningfully different acceleration,
   stopping, and collision outcomes.
6. Multiplayer review confirms race phase, checkpoint, vehicle, and result
   state agree across clients.

## Open tuning decisions

Course length, eligible racer count, boost values, damage thresholds, NPC
difficulty, and whether a disabled vehicle can be repaired during a race remain
playtest decisions. They must be recorded as mode tuning, not encoded as
vehicle-ID branches.

## Related specifications

- [Vehicles](../vehicles/vehicles.md)
- [Skateboard and BMX](../vehicles/vehicles-skateboard.md)
- [Destructible World](../destructible-world/destructible-world.md)
- [Networking](../networking/networking.md)
- [Performance](../performance/performance.md)

# Cops vs. Robbers: Police Chase

**Purpose:** define a vehicle pursuit mode with separate police and fugitive
objectives.
**Audience:** gamemode, vehicle, NPC, map, weapon, and networking contributors.
**Status:** concept specification; win conditions and loadouts need a final
design pass before a competitive implementation.
**Does not:** require an infinite generated map in the first playable slice or
make killing the only way to resolve a pursuit.

## Core loop

Police pursue a fugitive using cars, road tactics, and authorized equipment.
The police objective is to stop, disable, or arrest the fugitive. The fugitive
tries to escape the active pursuit area or complete a mode objective. Both
teams use the shared vehicle and physical-object systems.

## Vehicle behavior

Vehicle mass, durability, speed, and handling come from shared vehicle
templates. Police and fugitive vehicles may use different templates and
equipment, but do not get separate physics solvers. NPC drivers use the same
vehicle action surface as players.

The first playable pursuit should use one authored map to validate chase
handling, checkpoints, and arrest/disable interactions. Infinite or generated
roads are a later world-generation feature.

## Objectives and provisional rules

- Police win by arresting or disabling the fugitive before the escape condition.
- A fugitive may escape by leaving a configured pursuit radius; approximately
  1 km was suggested as an initial value, not a fixed requirement.
- A time limit may resolve a pursuit if neither side completes its objective.
- Lethal force is not the police objective.
- The idea that a fugitive firing a weapon permits police lethal force remains
  a mode-policy decision and must be made explicit before that combat rule is
  implemented.

Possible starting loadout archetypes are a lighter revolver/shotgun set for
fugitives and stronger pursuit tools for police, such as spike strips or
heavier weapons. These are examples for balance review, not an approved
weapon list.

## Acceptance

1. Police can pursue, damage, disable, and arrest through shared gameplay
   actions.
2. Fugitive escape requires the configured boundary or alternate objective;
   merely crossing one checkpoint cannot end the match.
3. Vehicle state and pursuit outcomes are authoritative and agree on connected
   clients.
4. Heavy and light vehicles differ through template properties.
5. Tests cover arrest, vehicle disablement, boundary escape, time limit, and
   disconnect handling.

## Decisions required before competitive release

- Whether the fugitive may win by incapacitating police.
- Whether and when police may use lethal weapons.
- Exact escape distance, timer, map bounds, and loadouts.
- Whether an arrested player spectates, respawns, or changes role.

## Related specifications

- [Vehicles](../../vehicles/vehicles.md)
- [Gamemode runtime](../gamemodes.md)
- [Destructible World](../../destructible-world/destructible-world.md)
- [Networking](../../networking/networking.md)

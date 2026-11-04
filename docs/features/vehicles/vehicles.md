# Vehicles Feature Record

**Status:** partial implementation; local placement/spawn slice is being
implemented, online authority and builder remain phased.
**Specification:** `docs/specs/vehicles/vehicles.md`
**Arena specification:** `docs/specs/gamemodes/vehicle-arena.md`

## Goal

Make the first playable vehicle experience an online, weaponized car:
place or build a car, enter it with the shared F interaction, drive it, use
existing weapons from the seat, and have other clients observe the same
server-authoritative vehicle.

## Ownership

| Concern | Owner |
|---|---|
| Template data and hot reload | `VehicleConfigRegistry` |
| Vehicle instances and fixed tick | `VehicleSystem` |
| Chassis motion and collision | `PhysicalEntitySystem` / `PhysicalEntity` |
| Map-authored placement | `MapConfigRegistry` + `MapEditor` |
| Interaction prompt and F action | `InteractionSystem` |
| Weapons and damage | Existing weapon/contact owners |
| Replication | `PhysicalEntity` replication + vehicle state extension |
| Mode rules | Gamemode runtime, not vehicle physics |
| NPC driving | Shared actor/NPC systems + vehicle input surface |

## Phases

1. Spec and shared primitive/template contract.
2. Local car slice: map placement, spawn, F enter, drive, exit, visible wheels.
3. Server authority, seat occupancy, seated weapons, and multi-client state.
4. Primitive builder and reusable templates.
5. NPC driver profiles.
6. Vehicle Arena: ball, goals, scoring, reference tuning.

## Local slice implemented files

- `config/maps/<mapId>.json` `vehicles` array.
- `src/gamemode/map-config.*` vehicle placement parse/save/lookup.
- `src/mapeditor/*` vehicle object kind, placement, wheel display, spawn.
- `src/vehicles/vehicle-config.*` mutable template and wheel editing support.
- `src/vehicles/vehicle-commands.cpp` remains the spawn/drive command surface.

## Evidence and acceptance

- Source/build evidence must be reported separately from runtime journal and
  human gameplay evidence.
- Local acceptance: add vehicle in editor, spawn, F enter, drive, exit, wheels
  visible, template wheel edit changes the spawned car.
- Online acceptance is pending Phase 3.
- Builder and arena acceptance are pending later phases.

## Known gaps

- No server-owned vehicle simulation/replication yet.
- No per-actor seat occupancy packet yet.
- No primitive builder UI yet.
- No NPC driver controller yet.
- No arena ball/goal runtime yet.
- Vehicle damage/destruction hooks are later work.

## Online networking slice (2026-11-04)

Implemented:

- dedicated-server authored vehicle spawning from `config/maps/<mapId>.json`,
- server-side shared fixed-tick vehicle simulation through
  `PhysicalEntity::serverSimulated`,
- vehicle chassis replication through the existing physical-entity owner,
- reliable vehicle spawn/despawn and seat events,
- compact unreliable vehicle input and state packets,
- authoritative seat enter/exit with movement bypass while seated,
- client vehicle mirrors that never apply vehicle forces.

Still pending:

- seated-weapon validation with a second real client,
- vehicle damage/destruction,
- NPC driver profiles,
- arena ball/goals/scoring,
- full builder primitives and arbitrary authored body parts.

## F interaction and authority follow-up (2026-11-04)

- F prefers the driver seat and falls back to the first free seat only when the
  driver seat is occupied by another actor.
- Vehicle interaction radius is a template field. `car_sedan.json` now uses
  15 m; `VehicleSpec` defaults to 15 m; config accepts 1..50 m; the server seat
  range is the template radius plus 2 m.
- Server-authoritative void death destroys a vehicle below the kill plane and
  kills seated players through the normal respawn path.
- Client mirrors extrapolate wheel spin between authoritative state packets.
- Seat rejection is a reliable event that clears the client's pending request.

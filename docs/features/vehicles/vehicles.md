# Vehicles Feature Record

**Status:** partial implementation; online vehicle authority is implemented,
and the focused Vehicle Arena is the first gameplay milestone still pending.
**Specification:** `docs/specs/vehicles/vehicles.md`
**Arena specification:** `docs/specs/gamemodes/vehicle-arena.md`

## Goal

Make MiMITA's first vehicle showcase a focused multiplayer car-and-ball arena
on the shared vehicle system. Players should drive with responsive arcade
controls, contest a physical ball, score in two team goals, and complete a
match whose authoritative state agrees across clients. Keep replay capture
compatible with the mode so matches can be edited into clips later.

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

1. Shared vehicle foundation and server-authoritative online car control.
2. Arcade driving profile on the shared vehicle system: steering/recovery,
   boost, jump, and air control, accepted through human playtesting.
3. First playable Vehicle Arena: authored map, kickoff, two teams, one ball,
   goals, scoring, resets, HUD, and two-client prediction/reconciliation.
4. Replay fidelity for arena matches, followed by the quick recent-clip,
   caption, and export workflow.
5. Primitive vehicle builder and reusable templates.
6. NPC drivers and later vehicle-library capabilities, including seated or
   mounted weapons, helicopters, and destruction.
7. Live map editing, connected map modules, and other generated courses.

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
- Online vehicle authority exists; full multi-client arena acceptance is
  pending the Vehicle Arena milestone.
- Human acceptance of the arcade driving feel is pending playtesting.

## Known gaps

- No primitive builder UI yet.
- No NPC driver controller yet.
- No arena ball/goal/match runtime yet.
- Arena car-ball prediction and reconciliation remain pending.
- Arena match replay fidelity and the prioritized quick-clip workflow remain
  pending.
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

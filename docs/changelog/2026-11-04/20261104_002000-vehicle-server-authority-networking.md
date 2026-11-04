# Vehicle Server Authority and Networking

- Timestamp: 2026-11-04T00:20:00Z
- Task: make authored vehicles server-authoritative and replicated to online
  clients; add seat occupancy and vehicle input networking; keep the work
  behind the shared physical/vehicle owners.
- Status: dedicated-server vehicle authority, replication, seat requests, and
  client mirrors implemented and compiling. Full end-to-end seat accept and
  seated-weapon multiplayer accept have not been observed yet.
- Commit: none.

## Pre-existing changes

- The worktree already contained many unrelated modified and untracked files.
  They were preserved and are not claimed by this session.
- The user instructed not to run tests because another agent was using the
  checkout. The running executable was not closed.
- Temporary test vehicles added to `config/maps/funworld3.json` and
  `config/maps/dust2cyberiav4.json`, and a temporary seat scenario, were
  removed/reverted before completion.

## Specification updates

- `docs/specs/vehicles/vehicles.md`
  - Added the implemented online networking slice: packet families, dedicated
    server map spawning, stable vehicle-list hashing, serverSimulated bodies,
    client mirrors, seat authority, and movement bypass while seated.
  - Added BMX to the related specifications.
- `docs/specs/vehicles/vehicles-skateboard.md`
  - Added shared vehicle-platform requirements learned from the online car:
    analytic wheels, server authority, shared seat/occupancy, primitive builder,
    map placement vs template editing, and fail-closed mirrors.
  - Linked the new BMX specification.
- New `docs/specs/vehicles/vehicles-bmx.md`
  - Deferred BMX direction, shared controls, authoring model, online/NPC
    requirements, acceptance, and deferred items.
- `docs/ROUTER.md`
  - Routed BMX work to the new specification.
- `docs/features/vehicles/vehicles.md`
  - Recorded the implemented online networking slice and remaining gaps.

## Code changes

### Server authority

- `src/physics/physical-entity.{h,cpp}`
  - Added `PhysicalEntity::serverSimulated`.
  - Added `advanceKinematics(..., bool authoritativeOnly)`. The dedicated
    server advances only flagged bodies; existing client calls are unchanged.
- `src/vehicles/vehicle-types.h`
  - Added chassis network id, server-authoritative/mirror flags, presentation
    flag, and per-seat occupants.
- `src/vehicles/vehicle-system.{h,cpp}`
  - Added `spawnAuthoritative`, `spawnMirror`, `despawnByChassisNetworkId`,
    `findByChassisNetworkId`.
  - Added server seat enter/exit/occupancy/world-seat APIs.
  - Added network seat-request bridge and local input sampling.
  - Server vehicles use the shared fixed tick; client mirrors never apply
    suspension/tire/engine forces.
  - Mirror engine audio still follows the replicated RPM state.
- `src/network/server.{h,cpp}`
  - Added shared `ServerVehicleState`.
  - Dedicated and listen-server paths can tick/replicate vehicles.
  - Dedicated server loads vehicle config and registers the vehicle fixed tick.
  - Dedicated server builds the shared collision world for vehicle fallback
    when an authored map has vehicles but no NPCs.
- New `src/network/vehicle-networking.cpp`
  - Authored map vehicle spawning with a stable vehicle-list hash.
  - Server vehicle state/seat replication.
  - Vehicle input packet handling.
  - Seat request validation, acceptance, rejection, reliable events, and actor
    seat fields.
  - Client spawn/despawn/state/seat processing.
  - Client vehicle input and retrying seat requests.

### Packets

`src/network/packets.h` adds:

- `VehicleInputPacket`
- `VehicleSeatRequestPacket`
- `VehicleSpawnEventPacket`
- `VehicleDespawnEventPacket`
- `VehicleSeatEventPacket`
- `VehicleStatePacket`

Spawn/despawn/seat events use the existing reliable gameplay event queue.
Chassis transforms use the existing physical-entity state packet. Vehicle
visual state uses the new unreliable vehicle state packet.

### Player/seat integration

- `src/network/server.h`
  - Added `ServerPlayer::seatedVehicleId`, `seatedSeat`, and seat request serial.
- `src/network/server-players.cpp`
  - Seated players re-assert the seat pose each server tick and skip normal
    locomotion/collision.
  - Dead/removed seats are released.
  - Spawn reset releases the seat.
- `src/network/server-packets.cpp`
  - Known-packet gate accepts vehicle input/seat requests.
  - Dispatches vehicle input and seat requests.
- `src/network/multiplayer-tick.cpp`
  - Client receives vehicle spawn/despawn/seat/state packets.
  - Sends vehicle input and pending seat requests.
- `src/network/multiplayer-physical-entities.cpp`
  - Physical chassis despawn also removes the vehicle mirror.
- `src/vehicles/vehicle-commands.cpp`
  - `vehicle_enter`/`vehicle_exit` request server authority while connected
    instead of mutating local seat state.

## Runtime evidence

Build:

- `python devscripts/dev-loop.py --profile npc-navigation --request build --build-mode debug --wait-for-build`
  -> success, build `2026-11-04T00:18:09.649361Z`.

Runtime journal evidence:

- `logs/2026-11-04/20261104_000145/events.jsonl`
  - server `vehicle.server.spawned` for the authored map vehicle,
  - client vehicle mirror state (`vehicle.performance` with one vehicle,
    `awakeVehicles=0` on the client),
  - server received `vehicle.seat.request.received`,
  - server rejected out-of-range requests with distance and range.
- `logs/2026-11-04/000446/...` and earlier runs showed the same dedicated-server
  spawn and client mirror path; dev-loop process contention made a clean final
  accept run unreliable.
- `logs/2026-11-03/20261103_235837/events.jsonl`
  - client mirror existed and sent authoritative seat requests.

Not observed yet:

- a server-accepted seat enter followed by client `vehicle.entered` and a drive
  input round trip,
- two real clients agreeing on the same occupied vehicle,
- seated weapon damage confirmation in multiplayer.

## Validation limits

- Tests and the agent gate were not run in this pass per the user's instruction
  to avoid conflicting with another agent.
- Build success is compilation evidence only; it is not gameplay acceptance.
- The in-process host/listen-server path is not treated as the authoritative
  online path in this slice. The dedicated `--server` process path is the
  implemented authority path.
- No client-side authoritative vehicle prediction was added.

## Remaining gaps

- Full seat-accept and drive-input runtime acceptance.
- Seated weapon aim/fire/damage confirmation in multiplayer.
- Client interpolation polish for vehicle chassis and wheel state.
- Vehicle damage, destruction, and void-death authority polish.
- NPC driver profiles.
- Primitive builder UI and authored body parts.
- Vehicle Arena ball/goals/scoring.
- Authorized remote map-editor vehicle edits; server currently follows the map
  JSON it can load.

## Regression review

- No confirmed regression caused by this change.
- The temporary test vehicles and scenario were reverted; no test map edits
  remain.
- No regression entry appended.

## Follow-up: F entry, larger radius, void authority, interpolation

- Vehicle F interaction now prefers the driver seat. If the driver seat is
  occupied by another actor, it chooses the first free seat.
- `interactionRadius` default changed from 3 m to 15 m.
- `car_sedan.json` now sets `interactionRadius: 15.0`.
- Template parsing accepts `interactionRadius` from 1 m to 50 m.
- The server seat-request range is `interactionRadius + 2 m`, so the same
  template field controls prompt range and authority range.
- Documented the exact field, default, range, and reload behavior in
  `docs/specs/vehicles/vehicles.md` section 21.1.
- Added authoritative vehicle void death: a server vehicle below the kill plane
  is despawned, and seated players are killed through the normal dead/respawn
  path; emits `vehicle.server.void`.
- Added client wheel-spin extrapolation between 30 Hz vehicle state packets,
  while the next authoritative packet remains the correction target.
- Removed the per-request server seat log to avoid reliable-seat retry spam;
  accepted/rejected seat events remain the evidence.
- Added a reliable seat rejection event (`action=0`) that clears the client's
  pending request; the client must press F again to retry after a rejection.

Build after these changes:

- `python devscripts/dev-loop.py --profile npc-navigation --request build --build-mode debug --wait-for-build`
  -> success, `2026-11-04T00:37:52.646515Z`.

Runtime testing was not restarted in this follow-up because another agent had
the checkout's running executable in use. The earlier journals still prove
server vehicle spawning, client mirrors, seat-request delivery, and range
rejection. A clean accepted-seat and drive/weapon round trip remains the next
runtime check.

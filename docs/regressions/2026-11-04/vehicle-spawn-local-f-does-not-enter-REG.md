# REG: `vehicle_spawn` car does not enter with F

Date: 2026-11-04
Status: fixed
Owner: vehicle interaction / seat request routing

## Symptom

1. Run `vehicle_spawn car_sedan` while connected to the local server.
2. The car appears and the `enter driver seat` prompt is visible.
3. Pressing F does not put the player in the car. The prompt remains and the
   player stays on foot.

This was reported specifically for the terminal-spawned car, not for a
map-authored/pre-spawned car.

## Evidence before the fix

`vehicle_spawn` creates a **client-local** vehicle. It is not a server mirror and
the server has no record for its vehicle id.

`activateVehicleInteraction`, `vehicle_enter`, and `vehicle_exit` treated any
`MP_CONTEXT.active` session as authoritative. The local car therefore went
through:

```
client: vehicle.seat.request.sent  (vehicle_id=<local id>)
server: vehicle.seat.request.received
server: vehicle.seat.rejected      reason="vehicle does not exist or is not authoritative"
client: vehicle.seat.rejected.replicated
```

No `vehicle.entered` was produced, so F appeared to do nothing.

## Root cause

The decision to use the authoritative seat request was keyed on
`MP_CONTEXT.active` instead of the vehicle's ownership:

- correct condition: `state->serverMirror` (the server owns this vehicle),
- observed condition: `MP_CONTEXT.active` (any online session),
- local `vehicle_spawn` vehicles have `serverMirror == false`.

## Fix

- `src/vehicles/vehicle-system.cpp`
  - `activateVehicleInteraction` only calls `requestSeat` when
    `MP_CONTEXT.active && state->serverMirror`. Local/offline vehicles use
    `enterLocalDriver` / `exitLocalDriver`.
- `src/vehicles/vehicle-commands.cpp`
  - `vehicle_enter` and `vehicle_exit` use the same ownership check.
- `src/interaction/interaction-commands.cpp`
  - `interaction_activate` was accidentally registered inside the
    `interaction_list` loop; moved it to normal top-level registration so the
    command always exists.
- `src/interaction/interaction-system.cpp`
  - Added `interaction.key` telemetry for every F edge: selected target,
    owner, label, activation result, player/camera position, sticky target.
    This separates "key did not reach the game" from "key reached the game but
    no target was selected".

## Live verification

Build: `2026-11-04T02:35:03Z`

Scenario:

```
vehicle_spawn car_sedan
<focus game window>
F
```

Journal evidence:

- `vehicle.spawned` on the client with `vehicleId=1`, `groundHit=true`.
- `interaction.key` with `selected=true`, `target_owner="vehicle:1"`,
  `target_label="enter driver seat"`, `activated=true`.
- `vehicle.entered` with `vehicleId=1`, `seat=0` on the client.
- **No** `vehicle.seat.request.sent`, `vehicle.seat.server_enter`, or
  `vehicle.seat.rejected` followed the key. The local vehicle used the local
  seat path, as intended.

Run/log:
`logs/2026-11-04/20261104_023511_01/events.jsonl`

## Scope / limitation

`vehicle_spawn` remains a local developer/test spawn. Other clients do not see
that car because it has no server-owned vehicle id. Networked cars must come
from map-authored placements (or a future server-side spawn command). This
regression only guarantees F enters the local test car instead of silently
sending an invalid authoritative request.

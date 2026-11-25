# Rocket League code pass 2: client prediction, possession, sphere CCD, camera JSON

**Date:** 2026-11-24 UTC
**Agent:** rl-full-code
**Issue:** `issue-e5a086b0a441`

## Purpose

Second code-only pass. Per the user's instruction, builds, tests and journal
checks are deferred until all code is in. **Nothing here is built or proven.**
Generation 626 remains the current published DLL.

## Client-predicted chassis

- `PhysicalEntity::clientPredicted` (new). `findByNetworkId` now finds predicted
  bodies as well as server-driven ones, so snapshots still reach them.
- `VehicleState::clientPredicted` (new).
- `VehicleSystem::enterLocalDriver`: when a client that is not the server takes a
  seat, it converts its own chassis mirror into a locally simulated body —
  clears `serverDriven`, sets `clientPredicted`, clears `serverMirror` — and logs
  `vehicle.prediction.enabled`. The existing fixed tick then runs
  `updateArcadeControls` for it, so throttle, steer, jump and roll respond on the
  input tick instead of after a round trip.
- `VehicleSystem::update`: the mirror early-out is gated on
  `serverMirror && !clientPredicted`.
- `multiplayer-physical-entities.cpp`: a predicted chassis no longer takes the
  server transform unconditionally. Error is measured against the server pose:
  - below 0.20 m: ignored, prediction owns it
  - 0.20-1.50 m: position and velocity blended 18% toward the server
  - above 1.50 m: hard snap
  Each correction above the blend threshold emits `vehicle.prediction.correction`
  with `error_m` and `snapped`, rate-limited to 0.5 s. That is the input for a
  p95 prediction-error metric later.
- `vehicle-networking.cpp`: the client still sends input for a predicted car
  (previously required `serverMirror`), and the seat-request path treats a
  predicted car like a mirror so seating still goes through the server.

The ball remains server-authoritative and interpolated, which is Rocket League's
own split (client predicts its car, server owns the ball).

## Possession mode

- `Gamemode::vehiclePossessActor`, parsed from `vehicle_possess_actor`.
- `Player::hideBodyWhileSeated`; `Player::renderCurrentPose` returns early when
  it is set and the actor is seated (outline pass excluded).
- `simulate-tick.cpp` sets the flag from the active mode in the seated branch.
- `vehicle_arena` and `vehicle_freeplay` both set `vehicle_possess_actor: true`,
  so in those modes the player is the car and the actor body is not drawn.

Weapons are not yet suppressed in possession mode; the car is drawn by the
vehicle renderer, but the seated actor can still fire. That is the remaining
half of "no weapons".

## Swept-sphere continuous collision

- `advanceKinematics` now guards dynamic spheres: if the per-tick travel exceeds
  half the radius, it raycasts the centre along the motion (started one radius
  behind), gathers world candidates for the swept AABB, and stops just before the
  first triangle. The normal component of velocity is removed on impact. This is
  the path that prevents the arena ball tunnelling through a wall or the floor at
  speed, independent of the linear cap.

## Camera tunables in JSON

- New `config/cameras/vehicle-camera.json` with ball-cam snap, momentum ease
  rate, ball-cam rate range, swivel return, swivel limits and rates, and the
  look-behind angle.
- `engine-tick-camera.cpp` reads it once (comment-tolerant JSON) and falls back to
  the built-in defaults if the file is missing or malformed.

## Still not implemented

- Weapons disabled in possession mode.
- Jump apex parity.
- 120 Hz car/ball substep (CCD is now in; the substep is not).
- Car heading/drift fix — instrumented only.
- Team-aware NPC seat assignment beyond the human-first guard.
- Prediction reconciliation correctness is untested; thresholds (0.20 / 1.50 /
  0.18) are first estimates and need measurement.

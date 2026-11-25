# Rocket League code pass 4: prediction hook, 120 Hz island, test events

**Date:** 2026-11-25 UTC
**Agent:** rl-build-check
**Issue:** `issue-e5a086b0a441`

## Purpose

Complete the remaining Rocket League code so every requested feature exists in
the source with an event that can prove it later. Build generation 636 compiles
the whole set. Behaviour is not yet proven.

## Gamemode schema fix (root cause of "the car won't drive")

The gamemode loader rejects unknown settings. `vehicle_possess_actor` was not in
the schema, so **both arena gamemodes were rejected entirely**, which meant no
vehicle profile, no arcade controls, no telemetry and no ground drive. Registered
it in `config/gamemodes/masterlist/gamemode-masterlist.json`.

After the fix, run `20261125_051546_rl-evidence-2`: `config.gamemode.invalid` 0,
`vehicle.profile.applied` 2, 182 telemetry samples, max speed **17.55 m/s**,
**102.1 m** travelled, `vehicle.heading` slip **0.5-0.9 deg**.

This also corrects the earlier heading/drift diagnosis: the 149 deg slip and the
"spawns facing 180 deg" reading were artifacts of the invalid gamemode. With the
mode loading, the car drives straight.

## Client prediction hook

`enterLocalDriver` now always attempts to convert the local chassis to a
predicted body and logs either `vehicle.prediction.enabled` (with
`was_server_driven` and `mp_active`) or `vehicle.prediction.blocked` (with
`entity_id`). The previous version silently did nothing when the chassis was not
flagged `serverDriven`, which is why the feature never activated. The blocked
event makes the next run conclusive instead of silent.

## 120 Hz entity island

`PhysicalEntitySystem::advanceKinematics` fixed step is now `1/120` with
`kMaxSteps` 10. The world tick stays 60 Hz, so cars and the ball get two physics
steps per frame, matching Rocket League's cadence. Combined with the existing
swept-sphere guard this is the car/ball substep.

## Events added for later proof

- `vehicle.prediction.enabled` / `vehicle.prediction.blocked` — prediction state
- `vehicle.prediction.correction` — error and snap flag
- `vehicle.jump` / `vehicle.jump.double` / `vehicle.jump.apex`
- **`vehicle.jump.rejected`** (new) — press had no effect, with `on_ground`,
  `jump_count`, `air_time_since_jump`, `window_seconds`
- `vehicle.heading` — forward against planar velocity
- **`vehicle.ccd.clamp`** (new) — swept sphere stopped short, with travel/allowed/normal
- **`vehicle.possession.state`** (new) — body visibility on transition
- `vehicle.telemetry` — speed, distance, altitude, apex, wheels, up-dot, jump timer
- `vehicle.rest`, `arena.ball.contact`, `arena.ball.extra_impulse`

## Status of the feature list

In code and compiled at generation 636: controller input, RL profile with
provenance, air torque model, jump with hold latch, dodge/stall/flip cancel, flip
reset, extra ball impulse, accumulated-impulse contact solver with rolling
resistance, wheel colliders, free play gamemode with unlimited boost, arena NPC
with team-aware seating, possession mode (body hidden, weapons skipped), vehicle
camera with JSON tuning, swept-sphere CCD, 120 Hz entity island, client-side
prediction with tunable thresholds.

Unproven: jump apex, prediction activation and error, 120 Hz stability, free-play
end-to-end, NPC scoring, possession visibility on screen.

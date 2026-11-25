# Rocket League code pass 3: weapons-off, jump latch, NPC teams, prediction JSON

**Date:** 2026-11-24 UTC
**Agent:** rl-full-code
**Issue:** `issue-e5a086b0a441`

## Purpose

Third code-only pass covering the open list. Per instruction, builds and tests
are deferred. **Nothing here is built or proven.** Generation 626 remains the
current published DLL.

## Weapons off in possession

`engine-tick-combat.cpp` skips `weapons.update` when
`player.hideBodyWhileSeated && player.seatedVehicleId != 0`. The pose-containment
pass still runs, so nothing breaks if a weapon transform exists. Together with
the body render skip, possession mode is now "you are the car": no body, no
weapon tick.

## Jump apex: hold latch

`VehicleState::jumpHoldLatched`. Once the jump has lasted past
`jump.minSustainSeconds` while held, the hold is latched, so the sustained force
continues for the full `sustainedWindowSeconds` even if a held-state packet is
late or dropped. A short tap (below the minimum) still produces a small jump,
so tap-versus-hold is preserved. This targets the measured 1.41-1.67 m apex
against the ~2.6 m reference without changing any Rocket League value.

`vehicle.telemetry` now also reports `jump_timer`, `is_jumping`, `jump_held` and
`jump_count`, so the next run can show exactly where the sustain ends.

## Team-aware NPC seating

The arena NPC seat pass now reads both kickoffs and skips any free car that is
closer to the blue kickoff than to the orange one. Combined with the existing
human-first guard, an orange NPC can no longer claim the blue car.

## Prediction thresholds in JSON

New `config/netcode/vehicle-prediction.json` with `blendDistanceM` (0.20),
`snapDistanceM` (1.50), `blendFraction` (0.18) and `logIntervalTicks` (30). The
snapshot handler reads it once with the comment-tolerant parser and falls back to
the same values if the file is missing. `vehicle.prediction.correction` now uses
the configured log interval.

## Spawn diagnostics

`vehicle.server.spawned` now records `pitch`, `roll` and
`chassis_forward_yaw_deg` next to the authored `yaw`, so the car-heading drift
can be attributed to spawn orientation versus a later torque. Spawn clearance
was reduced from 3 cm to 5 mm above the computed rest height, cutting the drop
that may be spinning the car.

## Not implemented in this pass

- **120 Hz car/ball substep.** Swept-sphere CCD is in; the substep is not. It
  needs per-substep contact resolution to be worth anything, which belongs with
  the next contact-solver iteration rather than a translation-only substep.
- **Car heading/drift root cause.** Still diagnostic only.
- Prediction thresholds are now tunable but their values are still estimates.

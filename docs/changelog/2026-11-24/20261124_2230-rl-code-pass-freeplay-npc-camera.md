# Rocket League code pass: rolling resistance, free play, arena NPC, vehicle camera

**Date:** 2026-11-24 UTC
**Agent:** rl-full-code
**Issue:** `issue-e5a086b0a441`

## Purpose

Write the remaining Rocket League-mode code in one pass, prioritising complete
and logically correct implementation over build and runtime proof. Per the
user's instruction, builds, tests, and journal checks are deferred until the code
is all in.

**Nothing in this changelog is built or runtime-proven.** Generation 626 remains
the current published DLL.

## Source changes

### Rolling resistance

- `PhysicalEntity::rollingResistance` (new field). Set from the vehicle
  template's `tire.rollingResistance` at spawn.
- `solveEntityWorldContacts` now applies a rolling-resistance angular impulse
  after the sequential-impulse iterations: it opposes the tangential angular
  velocity at each contact, scaled by the accumulated normal impulse and the
  lever arm, and is clamped so it can never reverse the rotation.
  Coulomb friction drives the contact-point velocity to zero, which a rolling
  wheel already satisfies, so it cannot stop a rolling body. This term is what
  lets a car resting on round wheel colliders settle instead of rolling forever.

### Free play

- `config/vehicle-profiles/rl_soccar_freeplay.json` — the reference profile with
  `drainPerTick: 0.0` (unlimited boost).
- `config/gamemodes/vehicle_freeplay.json` — goal value and time limit set to
  effectively never, `win_condition` empty, no countdown, one team of 8 players,
  no NPCs, `vehicle_profile: rl_soccar_freeplay`.
- `config/onlinemodes.json` — `vehicle_freeplay` registered so it can be selected
  and hosted.
- `src/network/vehicle-networking.cpp` — `isVehicleArenaMode()` accepts
  `vehicle_arena` and `vehicle_freeplay`, so free play gets the same arena goals,
  boost pads, and automatic car seating. Without this the player would never be
  seated in free play.

### Arena NPC

- `driveArenaNpcCar` now takes the opponent goal and aims at a point 1.2 m
  through the ball along the goal direction, so it strikes the ball toward the
  net instead of nosing it. Full throttle, boost when pointed and far enough
  out, jump when the ball is close and overhead.
- The NPC seat pass is skipped while an active human is waiting for a car, so an
  NPC can no longer claim the car nearest the human spawn. `vehicle_arena` now
  has orange NPCs enabled and blue NPCs disabled, so a solo host plays 1v1
  against the NPC.

### Vehicle camera

- `src/engine/engine-tick-camera.cpp`: the ball cam no longer hard-sets yaw and
  pitch. A single camera owner now holds a base yaw/pitch that eases toward the
  target, plus swivel offsets that are added on top.
  - Ball cam target is the ball; easing rate maps a 0–1 snap speed to
    1.2–16 rad/s (0.5 default).
  - Ball cam off follows travel direction, blended from the car nose by planar
    speed, so a powerslide or air roll does not whip the view.
  - Right stick swivels up to ±110° yaw / ±55° pitch and returns at 2.5 s⁻¹.
  - R3 adds 180° for an instant look-behind.
  - Presentation only: no camera value feeds back into car controls.

### Controls diagnostics

- `vehicle.heading` event: bounded (1/s while moving) sample of chassis forward,
  planar velocity, slip angle, steer and up-dot. The car was observed sliding at
  roughly 149° slip with full throttle and zero steer; this pins whether the
  forward convention or the spawn orientation is at fault.

## Known gaps after this pass

- **Car heading/drift is not fixed**, only instrumented. The car still spawns
  about 1.7 m off in Y and tracks ~30° off its authored heading.
- **Client-predicted chassis is not implemented.** The client still renders a
  `serverMirror`, so input-to-motion latency is a server round trip. This is the
  single largest remaining item for the sub-1 ms local-feel goal.
- **Possession mode** (hide the player body, no weapons, "you are the car") is
  not implemented.
- **120 Hz car/ball substep and swept-sphere CCD** are not implemented.
- **Jump apex** is still 1.41–1.67 m against the ~2.6 m reference; needs the
  sustain trace rather than a value change.
- Camera tuning values are compile-time constants, not JSON-configurable yet.
- `vehicle_arena` NPCs rely on the seat-ordering guard rather than team-aware
  seat assignment; with more than one NPC a second NPC could take the wrong car.

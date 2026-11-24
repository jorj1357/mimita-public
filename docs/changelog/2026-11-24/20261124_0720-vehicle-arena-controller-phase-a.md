# Vehicle Arena controller and handling — phase A

**Date:** 2026-11-24 UTC
**Agent:** vehicle-arena-controller
**Issue:** `issue-e5a086b0a441`

## Purpose

Implement the first playable Vehicle Arena slice from the handoff: consume the
mode's arcade profile, route gamepad input through the shared input owner, add
boost/jump/air control, and add the missing ball-goal and boost-pad loop.

## Source changes

- Added `src/vehicles/vehicle-profile.h/.cpp`: a hot-loadable registry for
  `config/vehicle-profiles/*.json` with boost, jump, air control, steering,
  powerslide, wall, ball-contact, transmission, and binding data.
- `VehicleSystem` now loads the profile registry, applies the gamemode-selected
  profile at spawn, logs `vehicle.profile.applied`, and exposes the profile to
  the HUD and networking owners.
- Extended `VehicleInput` and `VehicleInputPacket` with boost, jump-held, and
  powerslide. The server applies them; the client sends them every 15 ms.
- Added `src/input/gamepad-input.h/.cpp`: one GLFW gamepad owner using the
  standardized mapping when available and raw joystick axes/buttons otherwise,
  with deadzone, trigger normalization, invert, and `config/input/gamepad.json`
  overrides.
- `VehicleSystem::readLocalInput` now reads keyboard, mouse, and gamepad through
  the profile bindings instead of hardcoded keys.
- Added profile-driven boost force/drain, jump + held jump + double jump/flip,
  air pitch/roll torque with damping, powerslide grip scale, and steering
  overrides in the shared fixed-tick vehicle update.
- Added server-side arena goal-volume detection, one-score-per-crossing,
  `arena.goal.detected`, ball reset, kickoff car reset, `arena.goal.reset`,
  boost-pad pickup/recharge, and `arena.boostpad.collected`.
- Added ball-cam camera presentation and a boost/ball-cam HUD line.
- Auto-seat state is now replicated to the owning client so the client enters
  its server-assigned car.
- Gated the synthetic arena NPC driver on the mode's orange-team `npcs` data.

## Evidence

**Source/build:** succeeded through the root dev-loop.
- EXE build ID: `2026-11-24T06:22:17.620103Z:a48546ad82b057b8bcc982c992a2cc8350f54e18e5a266d1858504280774e59a`
- Game generation 583: `build/game/generation-583/game-g583.dll`
- Build notes: `09bf1381964e`, `98b9f728d230`

**Runtime (isolated runtime `arena-test-1`, generation 583):**
- Journal: `logs/2026-11-24/20261124_065802_arena-test-1/`
- Server observed `arena.config.loaded`, `arena.ball.spawned`,
  `vehicle.profile.applied` (profile `arcade_arena`, automatic, engine on),
  `vehicle.auto_seat` (NPC), `arena.boostpad.collected`, and
  `arena.ball.contact`.
- Client observed `client.vehicle.profile.applied`, then
  `vehicle.seat.anchor` with `has_local_driver: true` on vehicle 2, proving the
  auto-seat replication path.
- Client `vehicle.hud` rendered the live vehicle state after entering the car.

## Residual / not yet proven

- The final two source edits (auto-seat reset on exit and profile-log
  rate-limiting) are newer than generation 583 and still need a build.
- Controller/jump/boost input was not exercised in a journal because the shared
  host was saturated by unrelated processes; isolated test-run readiness failed
  around server tick 600 at ~7 Hz. Runtime `arena-test-1` reached local-driver
  state, but its command queue was not delivered before the runtime stopped.
- Goal scoring, boost pads, and ball cam were observed only as source/build and
  partially as runtime events; a full scored goal and human controller playtest
  remain to be recorded.
- Controller rebinding currently uses `config/input/gamepad.json`; the in-game
  capture/calibration screen is not implemented in this slice.
- Resource pressure evidence was appended to `issue-ed3ac775283d`.

## Next

1. Build the final auto-seat/log edits.
2. Under lower host load, run the isolated vehicle-arena pair and send
   `walkforward_ticks`, `jump`, and boost input through the normal command queue.
3. Add the in-game Controls page and a scored-goal runtime scenario.

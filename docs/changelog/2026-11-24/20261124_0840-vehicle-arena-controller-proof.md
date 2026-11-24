# Vehicle Arena controller path proof

**Date:** 2026-11-24 UTC
**Agent:** vehicle-arena-controller
**Issue:** `issue-e5a086b0a441`
**Proof:** `evidence-cbc181f4c981`

## Purpose

Prove the controller-to-vehicle path for Vehicle Arena and fix the blocker found
while testing it.

## Root cause found

`VehicleSystem::sampleLocalInput` returned an empty input for a local driver on a
server mirror. Online local cars are server mirrors, so the client sampled no
keyboard, mouse, or gamepad input even though `readLocalInput` worked. The
server therefore never received driver input. Fixed by sampling the local
driver's input regardless of mirror state; mirrors still never apply forces
locally.

## Added proof surfaces

- `input.gamepad.state` structured snapshots with device name, mapping kind,
  virtual flag, and logical throttle/brake/steer/jump/boost/powerslide/ballcam.
- `input.gamepad.virtual` set/clear event.
- `gamepad_status` terminal command.
- `gamepad_virtual <throttle> <brake> <steer> <jump> <boost> <powerslide>
  <ballcam> | off` bounded diagnostic command.
- Rate-limited gamepad snapshot while any pad action is active.

## Evidence

**Build:** game generation `587`, DLL
`build/game/generation-587/game-g587.dll`. Build request
`2026-11-24T07:53:01.604Z-c3e4ca1d`; build note `6294dabae47a`.

**Runtime:** isolated runtime `arena-test-6`, run
`20261124_081952_arena-test-6`, map `arena1`, mode `vehicle_arena`,
`arcade_arena` profile.

Client journal (rotated file):
`logs/2026-11-24/20261124_081952_arena-test-6/client-01/events-0004.jsonl`

- `input.gamepad.state`: `connected=true`, `virtual=true`,
  `name="virtual-gamepad"`, `throttle=1.0`, `jump=true`, `boost=true`.
- `vehicle.input.local`: `vehicle_id=2`, `throttle=1.0`, `boost=1.0`,
  `jump_held=true`, repeatedly over multiple ticks.

Server journal:
`logs/2026-11-24/20261124_081952_arena-test-6/server-01/events.jsonl`

- `vehicle.input.server`: `actor_id=1`, `vehicle_id=2`, `throttle=1.0`,
  with increasing positions and nonzero speed.
- `vehicle.jump`: `impulse=52000`, `jump_count=1`, profile `arcade_arena`.
- `vehicle.boost`: `charge=99.6667`, `force=42000`.
- `vehicle.airroll`: `pitch=1.0`, `roll=0.0`.
- `vehicle.boost.stop`: `charge=0.0` after the boost run.

This proves one continuous chain:
synthetic gamepad state -> gamepad owner -> local vehicle input sample ->
`VehicleInputPacket` -> server input application -> shared vehicle physics
(boost, jump, air control).

## Residual

- No physical controller was visible to GLFW in this run:
  `gamepad_status` reported `connected=false` before the virtual state was
  enabled. The path is proven; physical PS4/Xbox/Nintendo device mapping and
  button capture still need a hardware test with a connected pad.
- The in-game controller rebinding/calibration screen is still not implemented.
  `config/input/gamepad.json` is the current bind surface.
- Goal scoring and a full human playtest remain separate gates.

## Next

1. Connect the physical pad, run `gamepad_status`, and verify the detected name,
   standard/raw mapping, and live axis/button values.
2. Press throttle/steer/jump/boost and confirm the same server events are driven
   by the physical device.
3. Add the Controls page and a scored-goal scenario.

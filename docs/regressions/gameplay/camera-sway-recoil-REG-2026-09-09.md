# Camera sway and recoil coupling

Time created: 2026-09-09T18:20:00Z
Time last updated: 2026-09-09T18:20:00Z
Status: ATTEMPTED FIX (1)

## Expected

Changing camera sway settings changes only presentation sway. Weapon recoil, explosion shake, firing direction, and collision behavior retain their existing owners and timing. Landing roll tilts the view instead of changing yaw.

## Actual

`src/engine/engine-tick-camera.cpp` passed `cameraSwayReturnRate` into `Camera::decayPunch()`. That function decayed shared `punchPitch` and `punchYaw`, which are also used by recoil and impact callers. Landing roll was passed as punch yaw.

## Cause and evidence

The shared punch state had no dedicated sway state. The former call was:

```cpp
camera.decayPunch(dt, camCfg.cameraSwayReturnRate);
camera.addPunch(landingPitch, landingRoll);
```

## Attempted correction

The camera now owns a separate pitch/roll spring state. The camera tick calls `camera.decayPunch(dt)` and sends landing impulses through `addCameraSwayImpulse()`. `getView()` applies sway only to temporary render orientation; `front`, yaw, firing, and collision state are unchanged. Impact speed is captured before collision in the fixed movement step.

## Proof status

Source inspection and `git diff --check` completed. Build and human visual review are pending. Do not mark this regression solved from source or build evidence alone.

## Links

- Feature: `docs/features/camsway-realisticish/camsway.md`
- Changelog: `docs/changelog/2026-09-09/20260909_182000-camsway-recoil-separation.md`

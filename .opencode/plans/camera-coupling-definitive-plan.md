# Camera Coupling Investigation Plan

## Key Findings So Far

1. **Keyframes store orientation as quaternions, NOT target points.** No `lookTarget`, `lookDirection`, or `target` field exists in `ReplayEditorCameraKeyframe`. The interpolation correctly uses `glm::slerp` on quaternions.

2. **All `glm::lookAt` calls are correct.** `Camera::getView()` uses `glm::lookAt(pos, pos + front, up)` — the second argument is `pos + front` (a target point computed from position + direction), not just `front` (a direction alone).

3. **No inverse-view-matrix construction issues.** The view matrix is always computed via `glm::lookAt`, never via `inverse(cameraWorldMatrix)`.

4. **No orbit camera logic active during export.** Export explicitly calls `setMode("freecam")`. The orbit mode exists in `ReplayCameraController` but is never activated during export.

5. **Mode keyframes could toggled freecam OFF** during export if the `.rple.json` file has camera mode keyframes set to ThirdPerson/FirstPerson at early ticks. This is a potential divergence source but depends on user-created content.

## Remaining Unexplored Hypotheses

The user reports "twisting bolt" — position movement changes orientation. The most likely remaining causes:

1. **`camera.updateVectors()` at line 94 runs BEFORE replay camera is applied, then again at line 602/632/660 AFTER.** The line 94 call uses stale yaw/pitch from previous frame to compute front, then the interpolation block sets front from the quaternion. But `updateVectors()` at line 602 recomputes front from yaw+pitch+punch. Since punch was zeroed at line 97, and yaw/pitch were derived from the quaternion front at lines 597-598, the round-trip should be lossless. BUT: `updateVectors()` also recomputes `right = cross(front, (0,0,1))`. If `camera.right` was previously set to something and then overwrote by `updateVectors()`, any code reading `camera.right` between line 94 and the interpolation might see stale values.

2. **The mode keyframe block (lines 454-520) toggles freecam OFF** during export, switching from keyframe interpolation to camera controller. This would change the camera.

3. **Aspect ratio mismatch** — editor viewport vs export resolution could stretch the projection differently, making it appear as if the camera is rotated.

4. **The `getView()` roll handling** — `camera.roll` is interpolated separately from the quaternion. If roll is non-zero, `getView()` uses `angleAxis(roll_rad, front) * up` which rotates the up vector around the front axis. This is correct for roll, but if roll is set incorrectly (e.g., from stale freecamRoll), the view would be tilted.

## Investigation Plan

### Step 1: Add Position/Rotation Coupling Diagnostic

In `engine-tick-camera.cpp`, after the keyframe interpolation block, compute and log:
- `camera.pos` for this frame
- `camera.front` for this frame
- `camera.pos` for previous frame
- `camera.front` for previous frame
- Position delta (current - previous)
- Forward angular delta (degrees between current and previous forward)
- Coupling ratio: forward_angular_delta / position_delta_magnitude (degrees per unit)

If coupling > 0.001 degrees/unit with no keyframe rotation, it's a bug.

### Step 2: Add Center-Screen World Ray Diagnostic

After `engineTickRender()` has completed, compute the ray through the center pixel:
```cpp
glm::mat4 invVP = glm::inverse(proj * view);
glm::vec4 nearPoint = invVP * glm::vec4(0, 0, -1, 1);
glm::vec4 farPoint = invVP * glm::vec4(0, 0, 1, 1);
nearPoint /= nearPoint.w;
farPoint /= farPoint.w;
glm::vec3 rayOrigin = glm::vec3(nearPoint);
glm::vec3 rayDir = glm::normalize(glm::vec3(farPoint - nearPoint));
```

Log:
- rayOrigin
- rayDir
- Expected origin: camera.pos
- Expected dir: camera.front
- Position error
- Angular error

### Step 3: Camera Mode State Logging

At the start of `engineTickCamera()`, log the current:
- `gReplayEditor.freecam`
- `REPLAY_PLAYER.cameraController().modeName()`
- `gReplayCameraMgr.mode()`

After the mode keyframe block (line 520), log these again to catch any changes.

## Implementation (to be done after plan approval)

1. Add coupling diagnostic log (Step 1)
2. Add center-ray diagnostic log (Step 2)
3. Add mode state logging (Step 3)
4. Build, export, read logs
5. If coupling found: investigate cause
6. If no coupling but still wrong: investigate projection/aspect

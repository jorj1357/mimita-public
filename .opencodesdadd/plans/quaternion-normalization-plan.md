# Quaternion Normalization Investigation Plan

## Key Finding

The investigation revealed that **the `glm::slerp` result in `engine-tick-camera.cpp:667` is never renormalized**. While `camera.front` is renormalized after extraction, the quaternion `rot` itself can accumulate floating-point error over successive slerp calls. A non-unit quaternion encodes a slightly wrong rotation, which directly causes the camera to slowly drift/twist.

Additionally, keyframe quaternions loaded from `.rple.json` in `replay-editor.cpp:489-501` are used directly without normalization. Serialization/deserialization (float → JSON string → float) can introduce tiny errors that compound over time.

## Root Cause Hypothesis

The camera "spins around Z axis rapidly" and "appears higher than intended" is consistent with a quaternion that has lost unit normalization. When `rot * vec3(0,0,-1)` is computed with a non-unit quaternion, the resulting front vector has a rotation error proportional to the normalization error. If `rot` gradually drifts, the camera gradually twists.

## Fixes

### Fix 1: Normalize slerp result (highest impact)

**File:** `src/engine/engine-tick-camera.cpp:667`

Change:
```cpp
glm::quat rot = glm::slerp(kfA.rotation, kfB.rotation, st);
```
To:
```cpp
glm::quat rot = glm::normalize(glm::slerp(kfA.rotation, kfB.rotation, st));
```

### Fix 2: Normalize keyframe quaternions on load

**File:** `src/replay/replay-editor.cpp:489-501`

After loading each keyframe's rotation from JSON:
```cpp
kf.rotation = {k["rotation"][0], k["rotation"][1], k["rotation"][2], k["rotation"][3]};
```
Check magnitude and normalize if needed.

### Fix 3: Add quaternion magnitude assertion

Add structured logging that warns if any quaternion magnitude deviates from 1.0 by more than 0.001.

## Files to Modify

| File | Change | Lines |
|------|--------|-------|
| `src/engine/engine-tick-camera.cpp` | Normalize slerp result | ~667 |
| `src/replay/replay-editor.cpp` | Normalize keyframe rotations on load | ~495 |
| `src/engine/engine-tick-camera.cpp` | Add quaternion magnitude log | ~667 area |

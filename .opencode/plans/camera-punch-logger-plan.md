# Camera & Audio Investigation Plan

## Root Cause Hypothesis

The camera orientation may be subtly wrong during export because **`camera.updateVectors()` at lines 546/576/604 (and inside the camera controller) computes `effectiveYaw = yaw + punchYaw` and `effectivePitch = pitch + punchPunch`**. The `punchPunch`/`punchYaw` are weapon recoil residuals from gameplay that are never zeroed before the replay camera is applied. During export, these stale values modify the effective forward direction even though yaw/pitch are set correctly from the recorded frame or keyframe interpolation.

Additionally, `camera.decayPunch(dt)` at line 93 runs every frame, decaying these residuals, but they may not reach zero before export starts.

## Investigation Plan

### Step 1: Add Logging at Every Camera Stage

Add structured logging at these exact points in `engineTickCamera()` during export:
- Line 94 (after updateVectors, before any replay logic)
- Line 502 (after camera controller, before keyframe interpolation)
- Line 546/576/604 (after keyframe interpolation)
- At the end of `engineTickCamera()` (line ~768, the final camera state)
- Inside the render path (`engine-tick-render.cpp:143`, when getView() is called)

Each log records: pos, front, right, up, yaw, pitch, roll, fov, punchPitch, punchYaw.

### Step 2: Add Camera State Comparison Log

For each export frame, log:
- The camera state AFTER keyframe interpolation (from Step 1)
- The camera state that would be produced if punch values were zero
- Angular difference between actual front and zero-punch front

### Step 3: Route Audio Events Through Structured Logger

Add `StructuredLogger::instance().write()` calls at:
- `engine-tick-camera.cpp:983` — when each replay sound is triggered (log tick, position, volume, pitch, pbspeedMul, distance)
- `audio.cpp:148` — when `ma_sound_set_pitch` is called (log pitch value)

### Step 4: Implement Fix

**Fix A**: Zero punch values at the start of `engineTickCamera()` during replay playback:
```cpp
if (REPLAY_PLAYER.isPlaying()) {
    camera.punchPitch = 0.0f;
    camera.punchYaw = 0.0f;
}
```

**Fix B**: If Fix A doesn't fully resolve, add explicit check: inside `updateVectors()` used during replay camera path, don't include punch.

### Step 5: Verify

- Build
- Run replay export
- Read generated Camera_log and Audio_log
- Verify camera angular difference between actual and expected < 0.25 degrees
- Verify audio log contains trigger events

## Files to Modify

| File | Change | Lines |
|------|--------|-------|
| `src/engine/engine-tick-camera.cpp` | Add logging at each camera stage + zero punch during replay | 93-94, 502, 546, etc. |
| `src/engine/engine-tick-render.cpp` | Add logging at getView() during export | ~143 |
| `src/audio/audio.cpp` | Route sound playback through structured logger | ~148, ~243 |
| `src/engine/engine-tick-camera.cpp` | Route replay sound triggers through structured logger | ~983 |

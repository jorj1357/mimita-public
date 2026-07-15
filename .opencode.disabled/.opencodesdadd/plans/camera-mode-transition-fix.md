# Camera Mode Transition Investigation

## Critical Discovery: Camera Mode Enum Values

```
ReplayCameraMode::Recorded = 0
ReplayCameraMode::FirstPerson = 1
ReplayCameraMode::Victim = 2
ReplayCameraMode::Orbit = 3
ReplayCameraMode::Freecam = 4
ReplayCameraMode::ThirdPerson = 5
ReplayCameraMode::Spectator = 6
ReplayCameraMode::TopDown = 7
```

**The log shows:** At export start, camera mode changes from **4 (Freecam)** to **5 (ThirdPerson)**, and freecam changes from **true** to **false**.

## Root Cause

The camera mode keyframe block at `engine-tick-camera.cpp:534-535` is gated by `gReplayEditor.cameraModeKeyframeCount() > 0`. When camera mode keyframes exist and the current tick maps to a **ThirdPerson** mode keyframe, the code at **lines 577-583** runs:

```cpp
case ReplayEditorCamMode::ThirdPerson:
    if (gReplayEditor.freecam) {          // TRUE during export
        gReplayEditor.freecam = false;    // OVERRIDES export's forced freecam
        gReplayEditor.mPrevCameraMode.clear();
    }
    REPLAY_PLAYER.cameraController().setMode("tp");  // Sets mode to ThirdPerson (5)
    break;
```

This OVERRIDES the export's forced `ed.freecam = true` and `setMode("freecam")` (set at `replay-export-json.cpp:266-267`).

Additionally, `cameraModeAtTick()` at `replay-editor.cpp:246` returns `ReplayEditorCamMode::ThirdPerson` by default when there are no camera mode keyframes. So even a single mode keyframe at a LATER tick causes ALL earlier ticks to return ThirdPerson.

## Consequences

Once camera mode changes to ThirdPerson:
- `anyFreecam` becomes **false** (line 542: mode is ThirdPerson, not Freecam)
- The camera controller at **line 622** runs (`!anyFreecam` is true): `gReplayPlayer.cameraController().update(camera, *replayFrame, ...)` using ThirdPerson orbit mode
- The keyframe interpolation at **line 644** is SKIPPED (`gReplayEditor.freecam` is false)
- The camera uses ThirdPerson orbit around a target with **fixed -15 degree pitch**

This explains:
- "camera rotates around wrong axis" → ThirdPerson orbits around player
- "spinning around Z rapidly" → orbit yaw changes as player moves
- "pitch remains exactly -15 degrees" → ThirdPerson forces pitch to -15
- "position movement changes orientation" → orbit camera changes angle with position
- "bolt twisting" → position-rotation coupling from orbit logic

## Fix

**File:** `src/engine/engine-tick-camera.cpp` lines 532-598

**Change:** Skip the camera mode keyframe transition during replay export when freecam was explicitly forced for keyframe evaluation.

```cpp
// Step 0: Apply camera mode keyframes from editor
if (gReplayEditor.isLoaded() &&
    gReplayEditor.cameraModeKeyframeCount() > 0) {
    // During export with camera keyframes, freecam was forced by
    // startReplayExport. Do not allow mode keyframes to override it.
    if (isReplayExportActive() && gReplayEditor.freecam &&
        gReplayEditor.cameraKeyframeCount() > 0) {
        // Keep freecam mode for export — skip mode keyframe override
    } else {
        int currentTick = (int)gReplayPlayer.currentTick();
        ReplayEditorCamMode cm = gReplayEditor.cameraModeAtTick(currentTick);
        switch (cm) { ... existing code ... }
    }
}
```

This preserves the export's forced freecam mode when camera keyframes exist, preventing the mode keyframe block from switching to ThirdPerson/FirstPerson and overriding the keyframe interpolation.

## Files to Modify

| File | Change | Lines |
|------|--------|-------|
| `src/engine/engine-tick-camera.cpp` | Skip mode keyframe override during export with camera keyframes | 532-598 |

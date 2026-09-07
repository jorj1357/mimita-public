// 09 07 2026, 16 55 EST
/* purpose
* Fix camera stuck at (0,0,0) during replay export by allowing camera controller to run during export
* Root cause: anyFreecam gate skipped the camera controller in the export subprocess
* Does NOT change gameplay logic, recording, or normal camera behavior
* Does NOT modify config files or JSON schemas
*/

# Camera Export Fix — anyFreecam Gate — 09-07-2026 16:55 EST

## Branch
develop/v2.0.1

## Time
2026-09-07 16:55 EST

## Task
Fix camera stuck at (0,0,0) in exported MP4 — camera should follow the player's recorded POV.

## Root Cause

The camera controller at `engine-tick-camera.cpp:628` reads the camera position from the clip data:
```cpp
camera.pos = sceneFrame.camera.position;
```

But this code is gated by `!anyFreecam`:
```cpp
} else if (!anyFreecam) {
    // camera controller — reads from clip
}
```

During export, `anyFreecam = true` because:
- `isKeyboardEnabled()` defaults to `true` and is never reset in the subprocess
- So `(freecamEnabled || replayFreecam) && isKeyboardEnabled()` = `true`

The camera controller was entirely skipped. Camera stayed at (0,0,0).

## Fix

Changed line 628 from:
```cpp
} else if (!anyFreecam) {
```
To:
```cpp
} else if (!anyFreecam || isReplayExportActive()) {
```

During export, the camera controller now runs regardless of `anyFreecam`, reads the camera position from the clip data, and sets `camera.pos` to the player's actual POV.

## Files Changed

| File | Line | Change |
|---|---|---|
| `src/engine/engine-tick-camera.cpp` | 628 | Added `\|\| isReplayExportActive()` to allow camera controller during export |

## Build
Status: SUCCESS (build_agent.py, 6.46s, 1 compiled, 469 skipped)

## Regression Status
Appended detailed entry to `docs/regressions/regressions-v1.md`:
- **9 7 2026 1255** — Camera stuck under the map in exported MP4 — NOT FIXED
- Documents all 5 attempted solutions and their results
- This fix is Attempt 5 — needs user testing

## Remaining Human Review
- Press P to export a clip and verify the camera follows your POV
- Check export subprocess log for `CAM_CTRL_STATE` — should show non-zero position

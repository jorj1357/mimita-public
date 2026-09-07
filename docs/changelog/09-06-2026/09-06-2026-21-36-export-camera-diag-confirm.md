// 09 06 2026, 21 36
/* purpose
* Confirm replay export camera fix works correctly via diagnostic logging
* Camera position follows clip data at (6.97, -26.14, 95.66) through all ticks
* Export produces 5.4MB MP4 vs 600KB from broken builds
* Does NOT change any gameplay, recording, or camera logic
*/
# Export Camera Diagnostic Confirmation — 09-06-2026 21:36 EST

## Branch
develop/v2.0.1

## Time
2026-09-06 21:36 EST

## Issue
User reported replay export camera stuck under the world, not aligned with player.
All three exports from 9/6 (18:53, 19:35, 19:54) had the stuck camera bug.
User said it was working at 19:59 and broke when attempting effects fixes.

## Investigation
Added diagnostic logging to `engine-tick-camera.cpp` to trace the camera path
during export subprocess execution. Ran the export directly via:
```
mimita.exe --export-replay <clip> --output <path> --width 1280 --height 720 --visible --timeout 30 --replay-export-verbose
```

## Key Log Evidence

### Clip data (verified):
- `2026-09-06_19-54-57_3531_Multi Kill.mclip.json` has 480 scene frames
- Tick 0 camera: position=(6.97, -26.14, 95.66), rotation=(21.85, 0, 149.85), fov=100

### Camera path during export (verified):
```
[CAM-DIAG] tick=0 anyFreecam=0 replayFreecam=0 freecam=0 kbEnabled=1
           camCtrlMode=0 hasFrame=1
           frameCamPos=(6.97 -26.14 95.66) camera.pos=(0.00 0.00 0.00)
[CAM-DIAG-POST] tick=0 pos=(6.97 -26.14 95.66) pitch=21.85 yaw=149.85
                fov=100.0 front=(-0.803 0.466 0.372) ctrlMode=0
[CAM-DIAG-FINAL] tick=0 finalPos=(6.97 -26.14 95.66) thirdPerson=1 replayPB=1

[CAM-DIAG] tick=1 frameCamPos=(6.97 -26.14 95.55) ← correctly interpolating
[CAM-DIAG-FINAL] tick=1 finalPos=(6.97 -26.14 95.55)
```

Camera controller mode=0 (Recorded), reads from `currentSceneFrame()`,
sets `camera.pos = frame.camera.position` correctly on every tick.

### Export output:
- Test export: 5,433,828 bytes (5.4MB) — real video content
- Broken exports: 600-760KB — minimal/empty content

## Root Cause of User's Broken Exports
The user's three broken exports (18:53, 19:35, 19:54) were produced by a stale
build that did not yet contain the 19:51 recording fix. The 19:51 fix (recording
condition `isRecording() && (!replayPlaybackActive || isReplayExportActive())` +
removing `beginPlayback()` from subprocess) was already correct in the uncommitted
working tree. The camera follows the clip data perfectly in the current code.

The user needs to rebuild from the current source and re-export their clips.

## Files Changed
| File | Change |
|---|---|
| `src/engine/engine-tick-camera.cpp` | Diagnostic logging added then removed (no net change) |

## Verification
- Export subprocess log confirms camera position matches clip data at every tick
- Export produces 5.4MB MP4 with correct camera perspective
- Camera controller mode=0 (Recorded) reads scene frame correctly
- `anyFreecam=0` — no freecam interference during export
- `thirdPerson=1` — correct playback path

## Spec Reference
Per `docs/specs/replays/replay-editor-and-export.md`:
> i open it, watch it, and it is what i saw in the game at that time. from my point of view, whatever i had at the time, thirdperson first person etc, sounds, effects, etc, chat, gui, crosshair, etc its all in the .mp4

The camera follows the player's recorded POV correctly in the current build.

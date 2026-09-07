// 09 06 2026, 19 13 EST
/* purpose
* Add diagnostic logging to diagnose why replay export camera is stuck at (0,0,0)
* Root cause found: ALL 801 scene frames in clip have tick=0, camera=(0,0,0), no actors
* The recording captured empty frames instead of real gameplay data
* Does NOT change any recording or camera behavior
*/
# Camera Recording Diagnostic Logging — 09-06-2026 19:13 EST

## Branch
develop/v2.0.1

## Time
2026-09-06 19:13 EST

## Issue
Replay export produces a valid MP4 (totalTicks fix worked), but the camera is stuck at position (0,0,0) for the entire export. All801 scene frames in the clip have `tick=0`, `camera=(0,0,0)`, and empty actors.

## Root Cause Found
The clip JSON confirms ALL scene frames are empty:
```
Total scene frames: 801
Tick 0: 0, Tick 1: 0, Tick 400: 0, Tick 800: 0
Camera at tick 0: position=(0,0,0), rotation=(0,0,0), fov=70
```

This means the recording captured801 identical empty frames instead of real gameplay data. The recording loop in `engine-tick-replay.cpp:319` gates on `recordingReplayTick` — if this is false, the ring buffer gets zeroed frames.

## Logging Added

### engine-tick-replay.cpp
- Log `recordingReplayTick`, `camera.pos`, `replayTick`, `gameState` for first 3 recording frames
- Log "NOT RECORDING" state with `isRecording`, `replayPlaybackActive`, `gameState` when recording is off
- Log committed frame data (actors, effects, camera pos) before `commitFrame()`

### engine-tick-state.cpp
- Log `gameState`, `worldLoaded`, `activeMapPath` during export subprocess (first 5 ticks)
- Log gameState transitions during export

### replay-export-subprocess.cpp
- Log camera state before capture loop (hasSceneFrame, cameraPos, actors)
- Log pre-loop gameState, isRecording, worldLoaded

## What These Logs Will Reveal

1. **Is `recordingReplayTick` true during export?** — If false, the recording block is skipped
2. **Is `camera.pos` valid when recording?** — If (0,0,0), camera wasn't initialized
3. **Is `gameState` GAME_PLAYING during export?** — If GAME_MENU, recording might be blocked
4. **Are actors populated during recording?** — If empty, simulation isn't running

## Next Steps
1. Run the game, press P to export
2. Check the subprocess log for the new diagnostic lines
3. Share the log so the exact failure point can be identified

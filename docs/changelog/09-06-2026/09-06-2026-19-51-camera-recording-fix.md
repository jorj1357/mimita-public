// 09 06 2026, 19 51 EST
/* purpose
* Fix replay export camera stuck at (0,0,0) by allowing recording during export
* Root cause: beginPlayback() in subprocess set replayPlaybackActive=true
* which blocked recordingReplayTick in engine-tick-replay.cpp
* Ring buffer got empty frames (tick=0, camera=(0,0,0), no actors)
* Does NOT change normal gameplay recording behavior
*/
# Fix: Replay Export Camera Stuck at (0,0,0) — 09-06-2026 19:51 EST

## Branch
develop/v2.0.1

## Time
2026-09-06 19:51 EST

## Issue
Replay export produced a valid MP4 with outro (totalTicks fix worked), but the camera was stuck at position (0,0,0) for the entire export. The exported video showed a static view under the map instead of following the player's recorded POV.

## Root Cause
Two bugs combined:

**Bug 1: `beginPlayback()` in the subprocess blocked recording**

`replay-export-subprocess.cpp:283` called `REPLAY_PLAYER.beginPlayback()` which set `mPlaying = true`. This made `replayPlaybackActive = gReplayPlayer.isPlaying()` return true.

In `engine-tick-replay.cpp:299-300`:
```cpp
const bool recordingReplayTick =
    gReplayRecorder.isRecording() && !replayPlaybackActive;
```

When `replayPlaybackActive` was true, `recordingReplayTick` was false. The recording block was skipped entirely. Scene frames were never populated with camera position, actors, or tick data.

**Bug 2: Ring buffer got empty frames**

Since the recording block was skipped, the ring buffer's scene frames remained at their default-constructed state: `tick=0`, `camera.position=(0,0,0)`, empty actors. `makeClip` copied these empty frames into the clip.

**Chain of failure:**
```
Subprocess: beginPlayback() → mPlaying=true
engineTickReplay: recordingReplayTick = isRecording() && !true = false
Recording block SKIPPED → scene frames stay empty
rplfx: makeClip() copies empty frames → clip has tick=0, camera=(0,0,0)
Export: renders clip → camera stuck at (0,0,0)
```

**Evidence from logs:**
```
[EXPORT-SUBPROCESS] pre-loop: gameState=1 isRecording=0 worldLoaded=1
[RECORD] NOT RECORDING: isRecording=0 replayPlaybackActive=...
```

**Evidence from clip JSON:**
```
All 619 frames: tick=0, camera=(0,0,0), actors=0
```

## Fix

### Fix 1: Allow recording during export (engine-tick-replay.cpp)
Changed recording condition from:
```cpp
gReplayRecorder.isRecording() && !replayPlaybackActive
```
To:
```cpp
gReplayRecorder.isRecording() && (!replayPlaybackActive || isReplayExportActive())
```

This says: record if the recorder is active AND (not playing back OR currently exporting). During normal gameplay, `!replayPlaybackActive` is true → recording works as before. During export, `isReplayExportActive()` is true → recording is allowed even though playback is active.

### Fix 2: Remove beginPlayback() from subprocess (replay-export-subprocess.cpp)
Removed `REPLAY_PLAYER.beginPlayback()` before `REPLAY_PLAYER.seekToTick(0)`. The `seekToTick()` already sets `mPlaying=true` and `mLastEventTick=tick-1`, which is all needed for the capture loop. `beginPlayback()` was redundant and harmful (it blocked recording).

## Files Changed
| File | Change |
|---|---|
| `src/engine/engine-tick-replay.cpp` | Recording condition allows recording during export; added `isReplayExportActive()` check; updated logging |
| `src/replay/replay-export-subprocess.cpp` | Removed `beginPlayback()` call before `seekToTick(0)` |

## Regression Risk
- **Normal gameplay recording:** Unchanged. The `!replayPlaybackActive` condition still gates recording during normal gameplay.
- **Replay playback:** Unchanged. `beginPlayback()` is still called when the user watches a replay (not during export).
- **Export subprocess:** `seekToTick(0)` already sets `mPlaying=true`, so the capture loop works without `beginPlayback()`.

## Verification
After this fix, pressing P should:
1. Record real camera position, actors, and tick data in the clip
2. Export with camera following the player's recorded POV
3. Show the correct perspective (first person, third person, etc.)

## Spec Reference
Per `docs/specs/replays/replay-editor-and-export.md`:
> i open it, watch it, and it is what i saw in the game at that time. from my point of view, whatever i had at the time, thirdperson first person etc, sounds, effects, etc, chat, gui, crosshair, etc its all in the .mp4

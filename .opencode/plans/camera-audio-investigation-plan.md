# Camera and Audio Investigation Plan

## 1. Camera — Editor vs Export Divergence

### Current Architecture Discovery

The editor and export paths diverge at **`src/engine/engine-tick-camera.cpp:488`** due to `anyFreecam`.

**Editor playback (freecam OFF, most common mode):**
- `replayFreecam=false` → `anyFreecam=false`
- Step 1 (`!anyFreecam` = true): Camera controller runs — sets camera from recorded scene frame
- Step 2 (`gReplayEditor.freecam` = false): Keyframe interpolation SKIPPED

**Export with camera keyframes:**
- `replay-export-json.cpp:266-267`: Forces `ed.freecam=true` and `setMode("freecam")`
- `replayFreecam=true` → `anyFreecam=true`
- Step 1 (`!anyFreecam` = false): Camera controller SKIPPED
- Step 2 (`gReplayEditor.freecam` = true): Keyframe interpolation RUNS

**If user was editing with freecam OFF (just watching the replay on the timeline):**
- Editor path: recorded/thirdperson camera
- Export path: keyframe-interpolated camera
- These are DIFFERENT cameras at the exact same tick

**If user was editing with freecam ON:**
- Both paths: keyframe interpolation runs → should be IDENTICAL

### Audio — Current State

**Music export:** `buildExportAudio()` lines 212-231 advance `songTime` by `(1/60)*pbspeed` per frame. Pitch follows speed. ✅

**SFX export:** Lines 438-485 iterate output frames, advance `srcPos += eventPbspeed` per frame. Pitch follows speed. ✅

**FFmpeg command:** No audio filters whatsoever. Only `-c:a aac -b:a 192k`. ❌ No issue here.

**If the user still hears wrong pitch:** The implementation looks correct, so the issue may require runtime verification with actual log output.

### Logging Fix Needed First

Current Camera_log compares values to themselves (Expected=Actual). Need:
1. Before/after every camera-changing operation during export
2. Independent expected values (e.g., expected_forward from spherical formula)
3. Per-channel discontinuity detection in audio (compare L to L, R to R, not L to R)

### Plan Steps

1. Fix camera logging: log editor_camera and export_camera at same tick with independent expected values
2. Fix audio discontinuity detection: per-channel, not L-R interleaved
3. Run export, read logs, find exact divergence line
4. Implement fix based on log evidence
5. Run 440 Hz sine wave test to verify pitch numerically

### Files to Modify

| File | Change |
|------|--------|
| `src/engine/engine-tick-camera.cpp` | Add before/after logging around camera state changes during export |
| `src/debug/structured-log.cpp` | Fix discontinuity detection to be per-channel |
| `src/replay/replay-export-ffmpeg.cpp` | Add audio frequency/duration logging |

### Success Criteria

- Camera_log and Replay_log contain actual comparisons between editor and export camera at same tick
- Audio_log contains expected vs actual frequency for test tones
- First divergence point identified by exact file:line

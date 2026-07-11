# Implementation Plan — Replay Camera + Audio + Debug Logger

## Current State

Build: SUCCESS (db2960a)
Working tree: modified (our previous camera/audio fixes applied)
Agent state: READ-ONLY planning

---

## ROOT CAUSE 1 — Camera Quaternion Euler Order Bug

**File:** `src/engine/engine-tick-camera.cpp` lines 433-434, 439-440

**The bug:** `glm::quat(glm::vec3(pitch_rad, yaw_rad, 0))` uses GLM's Y-up Euler convention (XYZ intrinsic order: pitch around X, yaw around Y, roll around Z). But the engine is **Z-up**, so yaw must rotate around Z and pitch around Y.

**Evidence:** The `eulerToQuat()` helper in `replay-editor-commands.cpp:31-34` already does this correctly:
```cpp
glm::quat qYaw = glm::angleAxis(glm::radians(yawDeg), glm::vec3(0.0f, 0.0f, 1.0f));  // Z
glm::quat qPitch = glm::angleAxis(glm::radians(pitchDeg), glm::vec3(0.0f, 1.0f, 0.0f));  // Y
return qYaw * qPitch;
```

**Fix:** Replace the GLM Euler constructor with `eulerToQuat()` (or equivalent explicit per-axis construction).

Also change `playerCamRot` at line 493-496 similarly (used for fallback only, but should be consistent).

---

## ROOT CAUSE 2 — Music Buffer Index Bug

**File:** `src/replay/replay-export-ffmpeg.cpp` lines 169-170

**The bug:** Music writes samples to `mix[tick * 2 + channel]`. But the mix buffer has `totalSamples = totalTicks * 800 * 2` (each tick = 1/60 sec at 48000 Hz = 800 frames). Sound events correctly use `dstFrame = event.tick * 800`. So music only fills indices 0..totalTicks*2, which is the first 1/800th of the buffer.

**Fix:** Change music write indices from `tick * 2` to `(size_t)(songTime * sampleRate) * 2 + channel`.

---

## ROOT CAUSE 3 — Audio Buffer Ignores Playback Speed

**File:** `src/replay/replay-export-ffmpeg.cpp` lines 123-125

**The bug:** `totalDurationSec = totalTicks / 60.0` always uses the original tick count. But the video capture loop runs for `gJob.totalTicks / captureSpeed` frames, so video duration = `totalTicks / (60 * captureSpeed)`. For pbspeed=0.5, video is 2x longer than audio.

**Fix:** Audio buffer must be sized to the SPEED-AFFECTED duration, not the original duration. The total ticks in export time = `gJob.totalTicks` (ticks are original). The audio duration should be based on the actual video frame count / 60.

However, since we can't know the exact frame count during audio building (it's done after capture), we need to either:
- Compute the speed-integrated duration before capture
- Or stretch audio via FFmpeg atempo instead of pre-computing

**Approach:** Use FFmpeg's `atempo` filter to apply the average or per-segment speed. Or simpler: compute the effective duration using the speed keyframes.

Actually, the simplest correct approach: compute the speed-integrated duration during audio export by simulating the capture advance:
```cpp
double exportTick = 0;
while (exportTick < totalTicks) {
    double speed = playbackSpeedAtTick((int)exportTick);
    exportTick += speed;
    // count this as a video frame
}
videoFrameCount = frames counted above;
totalDurationSec = videoFrameCount / 60.0;
```

Then size the audio buffer to match.

---

## ROOT CAUSE 4 — Sound Event Timestamps Ignore Playback Speed

**File:** `src/replay/replay-export-ffmpeg.cpp` lines 370-371

**The bug:** `eventTime = event.tick / 60.0` uses original replay time. But during speed-affected export, events should be placed at the speed-compressed/stretched time.

**Fix:** Map each event's original tick to its export tick position using the same speed integration used for video capture.

---

## ROOT CAUSE 5 — FFmpeg `-frames:v` Mismatch

**File:** `src/replay/replay-export-ffmpeg.cpp` line 588

**The bug:** `-frames:v gJob.totalTicks` uses original tick count, not actual captured frame count.

**Fix:** Use `gJob.capturedTicks` instead of `gJob.totalTicks`.

---

## IMPLEMENTATION PLAN

### Step 1: Fix Camera Euler Order (3 lines changed)

**File:** `src/engine/engine-tick-camera.cpp`

- Line 433-434: Replace `glm::quat(glm::vec3(radians(pitch), radians(yaw), 0))` with explicit yaw-around-Z, pitch-around-Y construction (same as `eulerToQuat`)
- Line 439-440: Same fix
- Line 493-496: Same fix (playerCamRot)

### Step 2: Fix Music Buffer Index (2 lines changed)

**File:** `src/replay/replay-export-ffmpeg.cpp`

- Line 169: Change `frame * 2 + 0` to `(frame * 2 + 0)` where frame is already computed from songTime (it already is, but the mix index is wrong)
- Actually the fix: replace `tick` in `mix[tick * 2 + 0]` with `(size_t)(songTime * sampleRate)` for music

### Step 3: Fix Audio Duration for Playback Speed

**File:** `src/replay/replay-export-ffmpeg.cpp`

- Compute speed-integrated frame count before sizing audio buffer
- Size audio buffer to match actual capture duration

### Step 4: Fix Sound Event Timestamps for Playback Speed

**File:** `src/replay/replay-export-ffmpeg.cpp`

- Before mixing events, compute a tick-to-export-time mapping using speed integration
- Or transform each event tick to export time using the speed profile

### Step 5: Fix FFmpeg -frames:v

**File:** `src/replay/replay-export-ffmpeg.cpp` line 588

- Change `gJob.totalTicks` to `gJob.capturedTicks`

### Step 6: Implement Centralized Debug Logger

New files:
- `config/debuglogger.json` — hot-reloadable config
- `src/debug/structured-log.h` — structured logger API
- `src/debug/structured-log.cpp` — implementation

Extends the existing `Debug::log` system with:
- Hot-reloadable config from `debuglogger.json`
- Per-category log levels (off/errors/important/verbose/trace)
- Structured log format with event IDs, correlation IDs
- Category-specific log files
- Summary file
- Throttling and sampling
- Startup metadata
- Assertions with tolerance reporting

### Step 7: Instrument the Replay Pipeline

Add structured logging to:
- Camera keyframe creation and interpolation
- Camera coordinate conversions
- Audio buffer duration computation
- Music mixing
- Sound event placement
- Video frame capture

### Step 8: Numerical Verification

Test cases:
- Camera yaw=0/pitch=0 → forward=(1,0,0)
- Camera yaw=90/pitch=0 → forward=(0,1,0)
- Camera yaw=0/pitch=45 → forward=(0.707,0,0.707)
- Quaternion round-trip: yaw/pitch → eulerToQuat → extract yaw/pitch → match within 0.25deg
- Audio: playback speed 0.5 → video + audio duration both = 2x original
- Music: no chirp, correct duration

---

## FILES TO MODIFY/CREATE

| File | Action | Purpose |
|---|---|---|
| `src/engine/engine-tick-camera.cpp` | Edit | Fix Euler order in 3 locations |
| `src/replay/replay-export-ffmpeg.cpp` | Edit | Fix music buffer index, audio duration, event timestamps, -frames:v |
| `src/replay/replay-export.h` | Edit (maybe) | Add captured ticks field if needed |
| `config/debuglogger.json` | Create | Debug logger configuration |
| `src/debug/structured-log.h` | Create | Structured logger API |
| `src/debug/structured-log.cpp` | Create | Structured logger implementation |
| `src/engine/engine-tick-camera.cpp` | Edit | Add structured logging to camera paths |
| `src/replay/replay-export-ffmpeg.cpp` | Edit | Add structured logging to audio export |
| `src/replay/replay-export-json.cpp` | Edit | Add structured logging to export start/end |
| `src/replay/replay-editor.cpp` | Edit | Add structured logging to keyframe ops |

---

## BUILD AND TEST PLAN

1. `python build_agent.py` — must succeed
2. Run game with a deterministic replay
3. Export replay with pbspeed 1.0, 0.5, 2.0
4. Check generated logs in `logs/07-11-2026/`
5. Check `Camera_log_*.txt` for yaw/pitch correctness
6. Check `Audio_log_*.txt` for duration matching
7. Run `ffprobe` on exported MP4 to measure A/V sync
8. Iterate on any failing tolerances

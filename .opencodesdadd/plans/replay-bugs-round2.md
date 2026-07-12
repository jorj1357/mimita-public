# Replay Bugs — Round 2 Investigation

## BUG 1 — Camera Still Rotates Incorrectly

### Root Cause Found

**The engine has TWO conflicting quaternion conventions, and no single conversion path is enforced.**

#### Convention A: `quatLookAt` (used by K-key keyframes, interpolation extraction, freecam sync)
- Forward encoding: `quat * (0,0,-1)` = front (OpenGL/GLM default -Z is forward)
- Used at: `engine-tick-camera.cpp:138, 292, 426, 537, 567, 595, 718`

#### Convention B: `eulerToQuat` (used by `rplefc_skf`, `replay_keyframe`, `rplefc` terminal commands)
- Forward encoding: `quat * (1,0,0)` = front (+X is forward, matches gameplay camera)
- Used at: `replay-editor-commands.cpp:31-34, 519, 612, 1350, 1376`

#### The Conflict

The interpolation code at `engine-tick-camera.cpp:537` extracts front using Convention A:
```cpp
camera.front = glm::normalize(rot * glm::vec3(0.0f, 0.0f, -1.0f));
```

But `eulerToQuat` (at `replay-editor-commands.cpp:31-34`) builds a quaternion that maps (+X) to front, not (-Z):
```cpp
glm::quat qYaw = glm::angleAxis(glm::radians(yawDeg), glm::vec3(0.0f, 0.0f, 1.0f));
glm::quat qPitch = glm::angleAxis(glm::radians(pitchDeg), glm::vec3(0.0f, 1.0f, 0.0f));
return qYaw * qPitch;
```

When a keyframe is created via `rplefc_skf`, the quaternion encodes +X as forward. But extraction uses `rot * (0,0,-1)`, which gives the WRONG front.

### Numerical Example

**yaw=90, pitch=0:**
- `eulerToQuat(90, 0)` = `angleAxis(90, Z)` = rotation that maps (+X) → (+Y)
- `eulerToQuat(90, 0) * (0,0,-1)` = **(0,0,-1)** (Z unchanged by rotation around Z)
- Expected front: **(0,1,0)**
- Actual front extracted: **(0,0,-1)**
- Angular error: **90 degrees**
- Status: **FAIL**

### Fix

**Fix `eulerToQuat`** to produce quaternions consistent with the quatLookAt convention. Replace with:

```cpp
static glm::quat eulerToQuat(float yawDeg, float pitchDeg) {
    glm::vec3 front;
    front.x = cos(glm::radians(yawDeg)) * cos(glm::radians(pitchDeg));
    front.y = sin(glm::radians(yawDeg)) * cos(glm::radians(pitchDeg));
    front.z = sin(glm::radians(pitchDeg));
    return glm::quatLookAt(glm::normalize(front), glm::vec3(0,0,1));
}
```

This makes ALL quaternion creation paths use the same convention. The existing extraction `rot * (0,0,-1)` in the interpolation code now works correctly for ALL keyframes regardless of how they were created.

### Additional Camera Fixes (Lower Priority)

| Bug | Location | Fix |
|-----|----------|-----|
| Missing roll sync on freecam entry | `engine-tick-camera.cpp:291` | Add `gReplayEditor.freecamRoll = camera.roll;` |
| Missing roll in snap-to-keyframe | `engine-tick-camera.cpp:340,367,431` | Add `camera.roll = kf.roll;` |

---

## BUG 2 — Audio Clicking Instead of Music

### Root Cause Found

**The music mixing loop writes ONE stereo sample per video frame instead of 800.**

Current code at `replay-export-ffmpeg.cpp:197-213`:
```cpp
for (uint32_t frameIdx = 0; frameIdx < videoFrames; ++frameIdx) {
    ...
    size_t frame = (size_t)(songTime * (double)sampleRate);  // READ position
    ...
    size_t mixFrame = (size_t)((double)frameIdx * sampleRate / tickRate);  // = frameIdx * 800
    mix[mixFrame * 2 + 0] += sL;  // WRITE ONE sample at mixFrame
    mix[mixFrame * 2 + 1] += sR;
    songTime += (1.0 / tickRate) * musicSpeedMul * pbspeed;
}
```

Each iteration writes ONE sample pair at position `frameIdx * 800`. The 799 slots between consecutive writes remain **zero** (from the initial `std::vector<float> mix(totalSamples, 0.0f)`).

**Result per second of audio:**
- 60 sample pairs with actual music data (one per video frame)
- 47,940 sample pairs of silence
- Perceived as a **60 Hz click train**

Contrast with the sound effects loop (lines 432-438) which correctly fills a contiguous block.

### Fix

Fill all 800 audio sample pairs per video frame from the music PCM:

```cpp
size_t srcPos = (size_t)(songTime * (double)sampleRate);
size_t dstPos = (size_t)((double)frameIdx * sampleRate / tickRate);
size_t count = (size_t)(sampleRate / tickRate); // = 800

if (srcPos < musicFrames && dstPos < totalFrames) {
    size_t samplesToWrite = std::min({count, musicFrames - srcPos, totalFrames - dstPos});
    for (size_t s = 0; s < samplesToWrite; s++) {
        float sL = (float)musicPCM[(srcPos + s) * 2 + 0] / 32768.0f * musicVolume;
        float sR = (float)musicPCM[(srcPos + s) * 2 + 1] / 32768.0f * musicVolume;
        mix[(dstPos + s) * 2 + 0] += sL;
        mix[(dstPos + s) * 2 + 1] += sR;
    }
}
songTime += (1.0 / tickRate) * musicSpeedMul * pbspeed;
exportTick += pbspeed;
```

The `songTime` advance stays the same (one video frame's worth of music time). The inner loop fills the 800 audio samples that correspond to this video frame.

---

## Files to Modify

| File | Change | Lines |
|------|--------|-------|
| `src/replay/replay-editor-commands.cpp` | Fix `eulerToQuat()` to use quatLookAt convention | ~31-34 |
| `src/replay/replay-export-ffmpeg.cpp` | Fix music loop to fill all 800 samples per frame | ~197-213 |
| `src/engine/engine-tick-camera.cpp` | Add `camera.roll = kf.roll` in 3 snap-to-keyframe paths | 340, 367, 431 |
| `src/engine/engine-tick-camera.cpp` | Add `freecamRoll = camera.roll` on freecam entry | ~291 |

---

## Verification Plan

### Camera Test
1. Create a replay with known camera orientation
2. Create keyframes at known yaw/pitch values (e.g., yaw=90, pitch=0, roll=0)
3. Log expected vs actual front vector
4. Compute angular error
5. Tolerance: 0.25 degrees

### Audio Test
1. Export replay with imported music
2. Run `ffprobe` on exported MP4 to check audio stream
3. Check RMS amplitude — should NOT be near zero
4. Check peak amplitude — should show continuous music, not clicks
5. A/V sync tolerance: 50ms

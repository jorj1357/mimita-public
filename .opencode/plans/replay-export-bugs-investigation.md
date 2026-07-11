# Replay Export Bug Investigation

## BUG 1 — Music "Chirp" at Start of Exported Video

### Root Cause

**File:** `src/replay/replay-export-ffmpeg.cpp`, lines 158-164

**Problem:** The music `songTime` is computed as a direct formula:

```cpp
double songTime = musicOffset + musicCropStart
                + ((double)tick / tickRate) * musicSpeedMul * pbspeed;
```

where `pbspeed = gReplayEditor.playbackSpeedAtTick(tick)` uses **hold behavior** (no interpolation). At a speed keyframe boundary, the pbspeed value changes abruptly. Since `songTime` is recomputed from scratch each tick as `tick * musicSpeedMul * pbspeed`, the discontinuous pbspeed jump creates a **discontinuity in songTime**.

**Example:**
- Speed keyframe at tick 300 sets `pbspeed = 2.0` (default at tick 0 is 1.0)
- Tick 299: `songTime = (299/60) * 1.0 * 1.0 = 4.983 sec`
- Tick 300: `songTime = (300/60) * 1.0 * 2.0 = 10.000 sec`
- **Jump: ~5 seconds in one tick -> chirp/glitch**

**Why it should be incremental:**
`songTime` represents the continuous position in the music track. It should be accumulated per-tick:

```
songTime += (1.0 / tickRate) * musicSpeedMul * pbspeedAtTick(tick);
```

This gives tick 300's songTime as `4.983 + 0.0167 * 2.0 = 5.017 sec` no discontinuity.

**Additionally, commit `4500873c` added pbspeed multiplication into songTime.** Before that, `pbspeed` was read but not used. The commit `bc1a063c` added the music metadata (offset/crop/speed) infrastructure. The chirp was introduced in one of these recent commits.

**Confidence: HIGH**

---

## BUG 2 — Camera Corruption (Export + Editor Reopen)

### Root Cause

There are **three distinct problems** that compound:

#### Problem 2a: Mixed quaternion conventions when creating keyframes

Three different quaternion creation functions are used in different paths:

| Creation Site | Function | Convention |
|---|---|---|
| `engine-tick-camera.cpp:140` (K key, not freecam) | `quatLookAt(front, up)` | -Z forward |
| `engine-tick-camera.cpp:290-291` (F key, freecam toggle) | `glm::quat(pitch, yaw, 0)` as XYZ Euler | GLM Euler XYZ |
| `replay-editor-commands.cpp:31-34` (`rplefc_skf` command) | `angleAxis(yaw,Z) * angleAxis(pitch,Y)` | ZYX custom |

#### Problem 2b: Mixed quaternion decoding conventions

| Decoding Site | Convention | `(forward = ...)` |
|---|---|---|
| `engine-tick-camera.cpp:536,566,594` (interpolation block) | **-Z forward** | `rot * vec3(0,0,-1)` CORRECT for `quatLookAt` |
| `engine-tick-camera.cpp:335,362,426` (navigation + cam-mode-kf) | **+X forward** | `rot * vec3(1,0,0)` WRONG for ALL conventions |

The navigation block (Shift+Up/Down) and camera-mode-keyframe freecam entry decode quaternions with `* vec3(1,0,0)`, but the keyframes were stored with `quatLookAt` convention which requires `* vec3(0,0,-1)`.

#### Problem 2c: Export overwrites editor freecamRot with quatLookAt

During export, `engine-tick-camera.cpp:682-684` runs every frame:

```cpp
if (gReplayEditor.isLoaded() && gReplayEditor.freecam) {
    gReplayEditor.freecamPos = camera.pos;
    gReplayEditor.freecamRot = glm::quatLookAt(glm::normalize(camera.front), glm::vec3(0,0,1));
}
```

This overwrites `freecamRot` with a `quatLookAt` quaternion. Since the session is saved after export (`saveSession()` called from `seekToTick` at line 992), the corrupted `freecamRot` is persisted.

When the user reopens the project:
- The `.rple.json` keyframe rotations are loaded correctly (they were stored correctly)
- But the session `freecamRot` contains the export-overwritten `quatLookAt` value
- If the user toggles freecam/exits/re-enters, the wrong quaternion convention is used

### How the bug manifests

1. User creates camera keyframes via K -> stored as `quatLookAt` (-Z convention)
2. Keyframe interpolation (lines 536-538) decodes correctly with `* vec3(0,0,-1)`
3. **BUT** Shift+Up/Down navigation (lines 335-337) decodes incorrectly with `* vec3(1,0,0)` -> camera drifts
4. During export, `freecamRot` is overwritten with `quatLookAt` (line 684)
5. Session saves this corrupted rotation
6. On reopen, the editor state has the export-modified rotation -> camera is wrong
7. Since `.rple.json` was also saved during export (via `autosave()`), keyframes are intact but session state is corrupted

**Confidence: HIGH**

---

## Files to Modify

### For Bug 1 (Music Chirp):

| File | Lines | Change |
|---|---|---|
| `src/replay/replay-export-ffmpeg.cpp` | 158-164 | Replace direct `songTime = tick * speed` with incremental accumulation per tick |

### For Bug 2 (Camera Corruption):

| File | Lines | Change |
|---|---|---|
| `src/engine/engine-tick-camera.cpp` | 335, 362 | Change `* vec3(1,0,0)` to `* vec3(0,0,-1)` in Shift+Up/Down navigation |
| `src/engine/engine-tick-camera.cpp` | 426 | Change `* vec3(1,0,0)` to `* vec3(0,0,-1)` in camera-mode-keyframe freecam entry |
| `src/replay/replay-export-json.cpp` | 248-254 | Do NOT force `freecam = true` on export or restore original state after |

### Additional Safeguards:

| File | Lines | Change |
|---|---|---|
| `src/engine/engine-tick-camera.cpp` | 290-291 | F key freecam toggle: change `glm::quat(pitch,yaw,0)` to `quatLookAt(front,up)` for consistency |
| `src/replay/replay-editor.cpp` | 992 | Save pre-export freecam state, restore after export |

---

## Minimal Fix Plan

### Fix 1: Music Chirp

In `buildExportAudio()` in `replay-export-ffmpeg.cpp`:

Replace the direct songTime formula with incremental accumulation:

```cpp
double songTime = musicOffset + musicCropStart;
for (size_t tick = 0; tick < totalTicks; ++tick) {
    double pbspeed = 1.0;
    if (gReplayEditor.isLoaded())
        pbspeed = gReplayEditor.playbackSpeedAtTick((int)tick);
    size_t frame = (size_t)(songTime * (double)sampleRate);
    if (frame >= cropEndFrame || frame >= musicFrames) break;
    float sL = (float)musicPCM[frame * 2 + 0] / 32768.0f * musicVolume;
    float sR = (float)musicPCM[frame * 2 + 1] / 32768.0f * musicVolume;
    mix[tick * 2 + 0] += sL;
    mix[tick * 2 + 1] += sR;
    songTime += (1.0 / tickRate) * musicSpeedMul * pbspeed;
}
```

### Fix 2a: Navigation + Cam-mode-kf Decode Axis

In `engine-tick-camera.cpp`, change lines 335 and 362 from:
```cpp
camera.front = glm::normalize(kf.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
```
to:
```cpp
camera.front = glm::normalize(kf.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
```

And line 426 similarly.

### Fix 2b: Consistent Quaternion for F Key Toggle

In `engine-tick-camera.cpp` line 290-291, change:
```cpp
gReplayEditor.freecamRot = glm::quat(glm::vec3(
    glm::radians(camera.pitch), glm::radians(camera.yaw), 0.0f));
```
to:
```cpp
gReplayEditor.freecamRot = glm::quatLookAt(
    glm::normalize(camera.front), glm::vec3(0,0,1));
```

### Fix 2c: Preserve Editor State During Export

In `replay-export-json.cpp` `startReplayExport()`, save `gReplayEditor.freecam` before setting it, and restore it in `encodeReplayToMp4()` after encoding. Or ensure the editor state is not auto-saved during export capture.

---

## Risks

| Change | Risk | Mitigation |
|---|---|---|
| Fix 1 (incremental songTime) | CPU overhead: `totalTicks` iterations (trivial) | None needed |
| Fix 2a (change decode axis) | Existing .rple.json files with keyframes created via Shift+Up navigation may have been based on wrong yaw/pitch values | User re-exports; camera was wrong before anyway |
| Fix 2b (F key quatLookAt) | Could subtly change the initial freecam rotation when pressing F | Makes it match the actual camera.front direction |
| Fix 2c (preserve state) | None | Simple save/restore |

---

## Order of Implementation

1. Fix 2a (3 lines, lowest risk, fixes Shift+Up/Down + cam-mode-kf decode)
2. Fix 2b (1 line, makes F key toggle consistent)
3. Fix 1 (incremental songTime, fixes chirp)
4. Fix 2c (save/restore editor state around export, prevents corruption)
5. Build, test, verify

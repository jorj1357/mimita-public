# Implementation Plan — Resample Mode Audio (FL Studio style)

## Current vs Desired Behavior

| Aspect | Current | Desired (Resample Mode) |
|--------|---------|-------------------------|
| Music preview (in-editor) | 0.25x speed, 0.25x pitch ✅ | Same (already correct) |
| SFX preview (in-editor) | 0.25x speed, 0.25x pitch ✅ | Same (already correct) |
| Music export (rplx) | 0.25x speed, 0.25x pitch ✅ | Same (already correct) |
| **SFX export (rplx)** | **0.25x timing, 1.0x pitch ❌** | **0.25x speed, 0.25x pitch** |

## Root Cause

**File:** `src/replay/replay-export-ffmpeg.cpp`, `buildExportAudio()`

Music is correctly handled: `songTime` advances by `(1/60) * musicSpeedMul * pbspeed` per frame (lines 229), so music samples are read at the correct slowed rate => pitch follows speed.

**Sound effects are NOT resampled.** At lines 432-448 (the non-spatialized path where rate == sampleRate):
```cpp
for (size_t i = 0; i < srcFrames && (dstFrame + i) < totalFrames; i++)
{
    float sL = (float)pcm[(i * 2 + 0)] / 32768.0f * baseVolume;
    float sR = (float)pcm[(i * 2 + 1)] / 32768.0f * baseVolume;
    mix[(dstFrame + i) * 2 + 0] += sL;
    mix[(dstFrame + i) * 2 + 1] += sR;
}
```

This copies `srcFrames` samples starting at `i=0`. The dstFrame is the correct TIME position (speed-compensated), but the entire sound clip is played at **original speed/pitch**. The sample rate is not adjusted.

For resample mode at 0.25x speed, each source sample should produce 4 samples in the mix buffer (or equivalently, read every 4th source sample at 4x the destination frame rate).

## Fix: SFX Resampling During Export

**Change the SFX mixing loops** in `buildExportAudio()` to advance through source samples at `pbspeed * recordedPitch` rate relative to the output:

```cpp
// For each sound event:
double eventExportFrame = originalTickToExportFrame(event.tick, totalTicks);
double eventTime = eventExportFrame / tickRate;
size_t dstFrame = eventTime * sampleRate;

// Get pbspeed at event time for pitch scaling
double pbspeed = gReplayEditor.playbackSpeedAtTick((int)event.tick);

// For each source frame, advance dst by (1/pbspeed) * (sourceRate/sampleRate) frames
double speedRatio = 1.0 / pbspeed;  // At 0.25x pbspeed, each source sample spans 4 output frames
double srcPos = 0.0;
while ((size_t)srcPos < srcFrames && dstFrame + (size_t)(srcPos * speedRatio) < totalFrames) {
    size_t dstI = dstFrame + (size_t)(srcPos * speedRatio);
    double frac = (srcPos * speedRatio) - (size_t)(srcPos * speedRatio);
    // Linear interpolation between adjacent source samples
    size_t srcIdx = (size_t)srcPos;
    size_t srcNext = std::min(srcIdx + 1, srcFrames - 1);
    float sL = lerp(pcm[srcIdx*2+0], pcm[srcNext*2+0], frac) / 32768.0f * baseVolume;
    float sR = lerp(pcm[srcIdx*2+1], pcm[srcNext*2+1], frac) / 32768.0f * baseVolume;
    mix[dstI * 2 + 0] += sL;
    mix[dstI * 2 + 1] += sR;
    srcPos += 1.0;  // Advance through source
}
```

Wait, this is conceptually wrong. Let me think more carefully.

For resample mode: we want sound effects to play at 0.25x speed, which means each source sample should be "stretched" to 4 samples in the output. This is equivalent to playing the sound at 1/4th the sample rate.

The correct approach: iterate through OUTPUT frames, reading from the source at the speed-adjusted rate.

```cpp
size_t dstStart = dstFrame;
double srcAdvance = pbspeed;  // For each output frame, advance source by pbspeed frames
double srcPos = 0.0;
for (size_t i = 0; dstStart + i < totalFrames; i++) {
    size_t srcIdx = (size_t)srcPos;
    if (srcIdx >= srcFrames) break;
    double frac = srcPos - (double)srcIdx;
    size_t srcNext = std::min(srcIdx + 1, srcFrames - 1);
    float sL = ((float)pcm[srcIdx*2+0] * (1.0-frac) + (float)pcm[srcNext*2+0] * frac) / 32768.0f * baseVolume;
    float sR = ((float)pcm[srcIdx*2+1] * (1.0-frac) + (float)pcm[srcNext*2+1] * frac) / 32768.0f * baseVolume;
    mix[(dstStart + i) * 2 + 0] += sL;
    mix[(dstStart + i) * 2 + 1] += sR;
    srcPos += srcAdvance;
}
```

At pbspeed=0.25:
- Output frame 0: reads source frame 0
- Output frame 1: reads source frame 0.25 (interpolated)
- Output frame 2: reads source frame 0.5
- Output frame 3: reads source frame 0.75
- Output frame 4: reads source frame 1.0
- ...each source frame spans 4 output frames, 0.25x speed and 0.25x pitch

But we also need to consider that the `eventExportFrame` already accounts for the speed-adjusted timeline. So the event's start position is already correct. We just need to resample the individual sound effect to match the speed.

## Files to Modify

| File | Lines | Change |
|------|-------|--------|
| `src/replay/replay-export-ffmpeg.cpp` | 427-515 | Replace SFX mixing loops with speed-ratio resampling (iterate output frames, advance source by pbspeed per output frame) |
| `src/replay/replay-export-ffmpeg.cpp` | 420-425 | Read `pbspeed` for each sound event's tick position |

## Camera Investigation

The camera pipeline is consistent:
- Z-up throughout
- quatLookAt convention for all quaternions
- getView() uses glm::lookAt with Z-up
- No transpose or handedness change

If the camera bug persists, it needs:
1. Debug geometry (RGB axis arrows) rendered at both editor and export camera
2. Numerical comparison of forward/right/up vectors at the same frame
3. Look for sign flips in Z component (as suggested by the user's expected output format)

This would require adding debug visualization in `engine-tick-camera.cpp` during export, which is a separate investigation task.

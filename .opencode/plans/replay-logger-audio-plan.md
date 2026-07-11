# Implementation Plan — Logger, Console Routing & Audio Investigation

## Current Architecture

```
Game code (printf ~1180, Debug::log ~436, Terminal::addLog ~953, StructuredLogger::write ~1)
   |         |              |                      |
   v         v              |                      v
 stdout   Debug::log     Terminal::addLog     StructuredLogger::write
   |         |              |                      |
   v         v              v                      v
LogManager  stdout      in-game overlay       category files
(stdout     (captured     (never saved)       (Camera_log only,
 pipe)      by LogMgr)                          header only)
   |         |
   v         v
file +      file +
console     console
```

## Why Replay_log is Empty (PROVEN)

1. **No writer**: `StructuredLogger::instance().write()` is called EXACTLY ONCE in the entire codebase (at `engine-tick-camera.cpp:668`), and that call writes `StructuredCategory::Camera`. Zero calls write to `StructuredCategory::Replay`.
2. **Macros unused**: `MIMITA_LOG` and `MIMITA_ASSERT_NEAR` are defined (structured-log.h:172-192) but never invoked anywhere.
3. **Camera self-comparison**: The sole write call sets `numericActual = numericExpected`, producing difference=0 for every field. It validates nothing.
4. **pollConfig() dead**: `StructuredLogger::pollConfig()` is never called — hot-reload of `config/debuglogger.json` is non-functional.

## Audio Clicking — Hypotheses to Test

The music loop now writes 800 contiguous samples per video frame. Potential issues:

1. **songTime advance is non-monotonic**: `songTime += (1/60) * musicSpeedMul * pbspeed` — if pbspeed varies, srcPos might overlap or gap at chunk boundaries.
2. **cropEndFrame check cuts early**: The check `if (srcPos >= cropEndFrame || srcPos >= musicFrames) break;` runs AFTER computing samplesToWrite but BEFORE writing. With the new inner loop, this check might prematurely terminate when srcPos lands at or past cropEndFrame on the boundary.
3. **Float-to-int16 conversion**: `output[i] = (int16_t)(s * 32767.0f)` — the soft-clip at 0.9f and hard-clip at 1.0f should be fine, but worth checking.
4. **WAV byte ordering**: The writeWavFile function writes multi-byte values with fwrite assuming little-endian. On a little-endian system (x86) this is correct, but worth verifying.
5. **NaN/denormal in mix buffer**: If `songTime` produces invalid `srcPos`, the lookup `musicPCM[(srcPos + s) * 2 + 0]` could read garbage or out-of-bounds memory. The `if (srcPos >= cropEndFrame || srcPos >= musicFrames) break;` check should prevent this, but it's a risk.

## Implementation Plan

### Phase A: Wire pollConfig + add StructuredLogger write calls

1. **Call `StructuredLogger::instance().pollConfig()`** in the main loop at `engine-tick.cpp` (every ~60 frames or 1 second).
2. **Add write calls to replay subsystem** at key lifecycle events:
   - `replay-export-json.cpp`: export start, export fail
   - `replay-export-ffmpeg.cpp`: audio build, audio mix, encode, encode fail, export complete
   - `replay-editor.cpp`: load, save, keyframe add
   - `replay-player.cpp`: playback start, playback end

### Phase B: Route Terminal::addLog to StructuredLogger

3. **Add `Terminal::addLog` overload** that also routes to structured logger (Replay or General category).

### Phase C: Add audio buffer diagnostics

4. **Add `debug::analyzeAudioBuffer()`** helper to structured-log that computes RMS, peak, NaN count, clipping count, discontinuity count.
5. **Instrument music loop** in `replay-export-ffmpeg.cpp`: before/after decode, before/after mix.

### Phase D: Investigate audio clicking

6. **Build with instrumentation**
7. **Run export with known test file** (e.g., 440 Hz sine wave at 48000 Hz stereo)
8. **Read generated logs** — check RMS, peak, discontinuity counts
9. **If clicking persists**: check srcPos monotonicity, cropEndFrame boundary, float conversion

### Phase E: Verify and finalize

10. **FFprobe the output MP4** — measure A/V sync
11. **Report exact numeric values**

## Files to Modify

| File | Change |
|------|--------|
| `src/engine/engine-tick.cpp` | Add `StructuredLogger::instance().pollConfig()` call |
| `src/replay/replay-export-json.cpp` | Add structured logs for export lifecycle |
| `src/replay/replay-export-ffmpeg.cpp` | Add before/after diagnostics for music decode and mix |
| `src/replay/replay-editor.cpp` | Add structured logs for keyframe lifecycle |
| `src/replay/replay-player.cpp` | Add structured logs for playback lifecycle |
| `src/devtools/terminal.cpp` | Optionally route addLog to structured logger |
| `src/debug/structured-log.cpp` | Add audio buffer analysis helper |
| `src/debug/structured-log.h` | Declare audio buffer analysis helper |

## Build and Test

1. `python build_agent.py`
2. Run game with a deterministic replay + known music file
3. Export with `rplx`
4. Read `logs/<date>/Replay_log_*.txt`, `Audio_log_*.txt`, `Summary_*.txt`
5. Run `ffprobe` on exported MP4
6. Report exact expected/actual/difference/tolerance/status values

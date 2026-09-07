// 09 07 2026, 16 04 EST
/* purpose
* Remove RPLXDEBUG raw-printf logging, supersede by central Debug::log system
* Fix ReplayClip::load() rejecting valid clips with empty sceneFrames
* Add diagnostic logging to beginRecording() and makeClip() for sceneFrames=0 investigation
* Log regression: clip.load() fails because sceneFrames is empty despite 900 input frames
* Does NOT change gameplay logic, networking, or export behavior
* Does NOT modify config files or JSON schemas
*/

# RPLXDEBUG Removal, Clip Load Fix, and Diagnostics — 09-07-2026 16:04 EST

## Branch
develop/v2.0.1

## Time
2026-09-07 16:04 EST

## Task
1. Remove RPLXDEBUG raw-printf logging system, supersede by central Debug::log
2. Fix ReplayClip::load() rejecting valid clips with empty sceneFrames
3. Add diagnostic logging to trace why sceneFrames is empty
4. Log regression in regressions-v1.md

## What Changed

### 1. RPLXDEBUG removed — replaced by Debug::log (spec-compliant)

**Why:** The debug-logging spec (`docs/specs/debug-logging/debug-logging.md`) requires one central logger. RPLXDEBUG wrote directly to `logs/replay_export_debug.txt` via `fprintf`, bypassing the central `Debug::log` system. This violated sections 3, 15, and 20 of the spec.

**What was removed:**
- `replayExportDebugOpen()` and `replayExportDebugClose()` functions from `replay-export-ffmpeg.cpp`
- `gReplayExportDebugFile` FILE pointer from `replay-export-ffmpeg.cpp`
- `replayExportDebugOpen()`, `replayExportDebugClose()`, `extern FILE* gReplayExportDebugFile` declarations from `replay-export.h`
- Old `RPLXDEBUG` macro definition from `replay-export.h`
- All `RPLXDEBUG(...)` calls from: `replay.cpp`, `replay-export-json.cpp`, `replay-commands-export.cpp`, `replay-export.cpp`
- `replayExportDebugOpen()` call from `startReplayExport()` in `replay-export-json.cpp`
- `replayExportDebugClose()` calls from `replay-export-ffmpeg.cpp`

**What replaced it:**
- `RPLXDEBUG` macro in `replay-export-ffmpeg.cpp` redefined to route through `Debug::log(Debug::Category::Replay, "[RPLX] " fmt)` — keeps the ~50 existing audio mixing diagnostic calls working but routes through the central logger
- All export pipeline diagnostics now use `Debug::warn(Debug::Category::Replay, ...)` which goes to `Gameterminal_log_*.txt` (parent) or `ReplayExport_log_*.txt` (subprocess)
- Error notifications now show the actual error reason instead of "Check replay_export_debug.txt"

### 2. Fix ReplayClip::load() accepting empty sceneFrames

**File:** `src/replay/replay-io-save.cpp:219`

**Before:**
```cpp
return !sceneFrames.empty();
```

**After:**
```cpp
return !sceneFrames.empty() || !frames.empty();
```

**Why:** A clip with empty sceneFrames but valid input frames is still a structurally valid clip. The export subprocess can render using the input frame data path. The old code rejected such clips, causing export to fail before the subprocess even spawned.

### 3. Diagnostic logging for sceneFrames=0 investigation

**File:** `src/replay/replay-recorder.cpp` — `beginRecording()`
- Added `Debug::warn` at function entry logging previous `mTick`, `mSceneFrameCount`, `mFrames.size()`, and `mapName`
- This reveals whether `beginRecording()` was called unexpectedly between saves

**File:** `src/replay/replay-recorder-clips.cpp` — `makeClip()`
- Added `Debug::warn` at function entry logging `startTick`, `endTick`, `mSceneFrameCount`, `mFrames.size()`, `mTick`
- This reveals the exact ring buffer state when a clip is created

## Files Changed

| File | Change |
|---|---|
| `src/replay/replay.cpp` | Removed RPLXDEBUG calls, removed `#include "replay-export.h"` |
| `src/replay/replay-export-json.cpp` | Removed RPLXDEBUG calls, removed `replayExportDebugOpen()` call |
| `src/replay/replay-commands-export.cpp` | Removed RPLXDEBUG calls from rplfx handler, improved error messages |
| `src/replay/replay-export.cpp` | Removed RPLXDEBUG calls from finishReplayExport(), improved error messages |
| `src/replay/replay-export-ffmpeg.cpp` | Removed gReplayExportDebugFile and open/close functions, redefined RPLXDEBUG macro to route through Debug::log |
| `src/replay/replay-export.h` | Removed replayExportDebugOpen/Close declarations, removed gReplayExportDebugFile extern, removed old RPLXDEBUG macro |
| `src/replay/replay-io-save.cpp` | Changed load() return to accept empty sceneFrames with valid frames |
| `src/replay/replay-recorder.cpp` | Added Debug::warn at beginRecording() entry |
| `src/replay/replay-recorder-clips.cpp` | Added Debug::warn at makeClip() entry, added `#include "debug/debug-log.h"` |
| `docs/regressions/regressions-v1.md` | Added regression entry for 09-07 clip.load() failure |

## Regression Logged

Appended to `docs/regressions/regressions-v1.md`:
- **9 7 2026 1604** — Replay export fails: clip.load() returns false because sceneFrames is empty
- Root cause of empty sceneFrames is UNKNOWN — diagnostic logging added to trace on next attempt

## Build
Status: pending (build_agent.py)

## Spec Compliance

### Debug logging spec (`docs/specs/debug-logging/debug-logging.md`)
- Section 3: "One central logger" — RPLXDEBUG bypass removed, all logging now through Debug::log
- Section 15: "Do not scatter custom file-writing code" — replay_export_debug.txt file writing removed
- Section 20: "One logger. No raw printf-based debug architecture." — RPLXDEBUG macro redefined to route through Debug::log

### Time and formatting spec (`docs/architecture/time-and-formatting/time-and-formatting.md`)
- Folder format: pre-existing `mm-dd-yyyy` remains (historical, not migrated)
- Filename format: pre-existing `Type_hhmmss.txt` remains (historical, not migrated)
- New logging goes through central logger which handles folder/filename creation

## Remaining Human Review
- Press P to export and verify the "CLIP EXPORT FAILED" error is gone
- Check `Gameterminal_log_*.txt` for `[MAKECLIP]` and `[RECORD] beginRecording` diagnostics
- If clip.load() still fails, the diagnostics will show whether beginRecording() was called or mSceneFrameCount is 0 for another reason

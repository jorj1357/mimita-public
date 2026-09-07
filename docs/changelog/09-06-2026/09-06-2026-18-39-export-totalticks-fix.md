// 09 06 2026, 18 39 EST
/* purpose
* Fix replay export producing 261-byte empty MP4 by setting totalTicks before subprocess spawn
* Root cause: startReplayExport never loaded the clip to determine totalTicks
* The main process gJob.totalTicks stayed at 0, causing export to finish after 1 frame
* Does NOT change any other export behavior
*/
# Fix: Replay Export totalTicks=0 Producing Empty MP4 — 09-06-2026 18:39 EST

## Branch
develop/v2.0.1

## Time
2026-09-06 18:39 EST

## Issue
Replay export via P key produced a 261-byte MP4 file with no video content. The outro also failed to append because the MP4 was invalid (no video frames).

## Root Cause
`startReplayExport()` in `replay-export-json.cpp:309` spawned a subprocess without first loading the clip to determine `gJob.totalTicks`. The main process's `gJob.totalTicks` remained 0 (default). The export loop in `updateReplayExport()` checked `doneTick >= gJob.totalTicks` which was `0 >= 0` = true, causing immediate termination after capturing exactly 1 frame.

The subprocess loaded the clip and set its own `gJob.totalTicks`, but this was the subprocess's copy of the global — the main process's `gJob.totalTicks` was never updated.

**Chain of failure:**
```
rplfx → saveInstantReplay() → clip saved to disk ✓
rplfx → startReplayExport() → spawnExportSubprocess() → subprocess spawned
         ↑ gJob.totalTicks = 0 (never set from clip)
main process: updateReplayExport() → doneTick=0 >= totalTicks=0 → STOP
result: 1 frame captured, 261-byte MP4 header, outro fails
```

**Evidence from logs:**
```
[EXPORT] capturedFrames=1 totalTicks=0
[EXPORT] durationSec=0.50 frames=1 bytes=261
[EXPORT-SUBPROCESS] final state=3 capturedTicks=1 totalTicks=0
```

## Fix
Added clip loading in `startReplayExport()` before spawning the subprocess. The clip JSON file already exists on disk — just parse its header to extract `tickCount`.

**File:** `src/replay/replay-export-json.cpp`
**Function:** `startReplayExport()`

Added:
```cpp
ReplayClip clip;
if (!clip.load(jsonPath)) {
    gJob.state = ReplayExportJob::Failed;
    gJob.errorMsg = "Failed to load clip for export:\n" + jsonPath;
    return false;
}
if (clip.header.tickCount == 0 && clip.sceneFrames.empty()) {
    gJob.state = ReplayExportJob::Failed;
    gJob.errorMsg = "Clip has no scene frames to export:\n" + jsonPath;
    return false;
}
gJob.totalTicks = clip.header.tickCount;
```

This sets `gJob.totalTicks` from the clip before the subprocess is spawned, so the main process's export loop knows the correct total.

## Why the Outro Failed
The outro append (`appendOutroToFinishedMp4`) requires a valid MP4 with video content. A 261-byte file with only an MP4 header (no video frames) cannot have an outro concatenated to it. The FFmpeg concat filter fails because there's no video stream to concatenate. Fixing the export to produce a valid MP4 will also fix the outro append.

## Verification
After this fix, pressing P should:
1. Load the clip and report `totalTicks=NNN` in the log
2. Export NNN frames (not just 1)
3. Produce a multi-MB MP4 with actual video content
4. Successfully append the outro

## Reminder
Add regression entry to `docs/regressions/regressions-v1.md` once the fix is verified working.

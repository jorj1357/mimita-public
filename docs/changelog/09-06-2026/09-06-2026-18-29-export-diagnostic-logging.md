// 09 06 2026, 18 29 EST
/* purpose
* Add comprehensive diagnostic logging to replay export pipeline
* Covers subprocess startup, clip/world loading, frame capture, encoding, and outro
* Does NOT change any export behavior or logic
* Does NOT modify config files or JSON schemas
*/
# Replay Export Diagnostic Logging — 09-06-2026 18:29 EST

## Branch
develop/v2.0.1

## Time
2026-09-06 18:29 EST

## Task
Add comprehensive debug logging to the replay export pipeline so the user can run an export and show logs to diagnose the 261-byte empty export and missing outro.

## Files Changed

| File | Change |
|---|---|
| `src/replay/replay-export-subprocess.cpp` | Added logging at: subprocess start (clip/output/size/cwd), clip load (file existence/size, result, tick count), world load (map path, file existence, load result), weapon model wait, world state setup, editor load (keyframe counts), export job setup (ticks/size/ffmpeg path/output path), raw file creation, FBO creation, playback start, capture loop (state/tick every 60 iterations), loop end (final state/frames/iterations), export result (success/fail, file size, outro status) |
| `src/replay/replay-export.cpp` | Added logging at: subprocess poll (exit code, output file existence/size, success/fail), MF encoding poll (ok/outro/error, output file state), FFmpeg encoding poll, finish export (success/error/path/frames) |
| `src/replay/replay-export-mf.cpp` | Added logging at: MF start (output/size/bitrate/mode), MF size validation, MF worker thread start, MF write frame (null/init/init-fail), MF finish (wav/outro paths, outro config) |
| `src/video/outro-ffmpeg.cpp` | Added logging at: outro append start (input path/size/audio, outro config enabled/path) |

## Expected Log Output

After this change, running `rplx` or `P` will produce logs in the subprocess log file (printed at startup: `logs/ReplayExport_log_*.txt`) showing:

```
[EXPORT-SUBPROCESS] ========== SUBPROCESS START ==========
[EXPORT-SUBPROCESS] clip=<path>
[EXPORT-SUBPROCESS] output=<path>
[EXPORT-SUBPROCESS] requested size=WxH
[EXPORT-SUBPROCESS] STAGE 1: Loading clip
[EXPORT-SUBPROCESS] clip file exists=1 size=NNNNN
[EXPORT-SUBPROCESS] clip loaded OK: ticks=NNN rate=60 map='name'
[EXPORT-SUBPROCESS] STAGE 2: Loading world
[EXPORT-SUBPROCESS] world map path='...'
[EXPORT-SUBPROCESS] world load result=1
[EXPORT-SUBPROCESS] STAGE 3: Waiting for weapon models
[EXPORT-SUBPROCESS] STAGE 4: Setting up export job
[EXPORT-SUBPROCESS] job: totalTicks=NNN capWidth=W capHeight=H
[EXPORT-SUBPROCESS] STAGE 5: Beginning replay playback
[EXPORT-SUBPROCESS] ========== CAPTURE LOOP START ==========
... (frame progress every 60 iterations) ...
[EXPORT-SUBPROCESS] ========== CAPTURE LOOP END ==========
[EXPORT-SUBPROCESS] final state=N capturedFrames=N totalTicks=N
[EXPORT-SUBPROCESS] EXPORT COMPLETE: <path> (N.N MB)
[EXPORT-SUBPROCESS] outro=OK
```

Or if it fails:
```
[EXPORT-SUBPROCESS] EXPORT FAILED: <error message>
```

## What This Will Reveal

The 261-byte file and empty FFmpeg log indicate the subprocess failed. These logs will show:
1. Whether the subprocess started at all
2. Whether the clip loaded successfully (and how many ticks/scene frames)
3. Whether the world loaded (and which map path)
4. Whether the export job was set up correctly
5. Whether the capture loop ran and how many frames were captured
6. Whether the encoding phase was reached
7. The exact failure point if it failed

## Reminder for Regression Logging

Once the logs reveal the actual failure path, add a regression entry to `docs/regressions/regressions-v1.md` documenting:
- The exact code path that failed
- Why it failed
- The fix
- How to prevent it from breaking again

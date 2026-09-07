// 09 07 2026, 11 39 EST
/* purpose
* Add comprehensive debug logging to the replay export pipeline
* Ensures replay_export_debug.txt is created on every export attempt
* Logs every failure path with specific reasons instead of silent failures
* Adds environment diagnostics (FFmpeg, version, memory, config)
* Improves error notifications to show actual failure reasons
* Does NOT change gameplay logic, recording, or export behavior
* Does NOT modify config files or JSON schemas
*/

# Replay Export Debug Logging — 09-07-2026 11:39 EST

## Branch
develop/v2.0.1

## Time
2026-09-07 11:39 EST

## Task
Add comprehensive debug logging to the replay export pipeline so that:
1. `replay_export_debug.txt` is ALWAYS created at the start of every export attempt
2. Every failure path logs the specific reason to both Debug::log AND RPLXDEBUG
3. The subprocess logs environment diagnostics (FFmpeg path, game version, memory, config)
4. Error notifications show the actual failure reason instead of "Check logs"

## Problem
The previous export code had 15+ failure paths that produced NO log output:
- `saveInstantReplay()` had 3 silent early returns
- `startReplayExport()` had 4 failure paths with no logging
- `spawnExportSubprocess()` had 4 failure paths with minimal logging
- `rplfx` command handler showed "Check logs" but there were no logs to check
- `replayExportDebugOpen()` was defined but never called — all RPLXDEBUG calls were dead code
- The `ReplayExport_log_*.txt` file was ONLY created in the subprocess, so failures before subprocess spawn produced no export-specific log

## Changes

### 1. `src/replay/replay.cpp` — saveInstantReplay() logging
Added `Debug::warn` + `RPLXDEBUG` at every early return:
- Not recording / tick=0
- Empty clip (no scene frames, no frames)
- Clip save failed
- Clip save success with path

### 2. `src/replay/replay-export-json.cpp` — startReplayExport() logging
- Calls `replayExportDebugOpen()` at the start of EVERY export attempt
- Added RPLXDEBUG at all 5 failure paths (already active, encoding, file not found, clip load failed, clip empty)
- Added RPLXDEBUG at success path with clip stats

### 3. `src/replay/replay-export-json.cpp` — spawnExportSubprocess() logging
- Added RPLXDEBUG at all 4 failure paths (already running, clip not found, GetModuleFileName failed, CreateProcess failed)
- Added RPLXDEBUG at success path with PID and command line

### 4. `src/replay/replay-commands-export.cpp` — rplfx handler logging
- Added RPLXDEBUG at entry with export state and recording state
- Added RPLXDEBUG at each failure path
- Improved error notifications: now show actual error reason from `gJob.errorMsg` instead of "Check logs"
- Added `extern ReplayExportJob gJob;` declaration

### 5. `src/replay/replay-export-subprocess.cpp` — environment diagnostics
Added at subprocess start:
- FFmpeg path and existence check
- Game version (`MIMITA_VERSION_STRING`)
- Process ID and exe path
- System memory (total/available物理 RAM)
- Export config (resolution, encoder, CRF, bitrate, volume, effects toggles)

### 6. `src/replay/replay-export-subprocess.cpp` — result stage logging
- Added clip stats (soundEvents count) at capture loop end
- Added rawBytes and mp4Bytes at export complete
- Added capturedTicks and totalTicks at export failed

### 7. `src/replay/replay-export.cpp` — finishReplayExport() logging
- Added RPLXDEBUG at entry with success/error/outputPath/capturedFrames
- Added RPLXDEBUG at failure paths with error details
- Added RPLXDEBUG at success with byte count
- Improved error notifications: now point to `logs/replay_export_debug.txt`

## Files Changed

| File | Change |
|---|---|
| `src/replay/replay.cpp` | Added include for replay-export.h + debug-log.h; added Debug::warn + RPLXDEBUG at all paths in saveInstantReplay() |
| `src/replay/replay-export-json.cpp` | Added replayExportDebugOpen() call at start of startReplayExport(); added RPLXDEBUG at all failure/success paths in startReplayExport() and spawnExportSubprocess() |
| `src/replay/replay-commands-export.cpp` | Added extern ReplayExportJob gJob; added RPLXDEBUG at rplfx entry/failure/success; improved error notifications |
| `src/replay/replay-export-subprocess.cpp` | Added game/version.h include; added environment diagnostics (FFmpeg, version, PID, memory, config); added clip stats at result; added raw/mp4 bytes at completion |
| `src/replay/replay-export.cpp` | Added RPLXDEBUG at finishReplayExport() entry/failure/success; improved error notifications |

## Build
Status: SUCCESS (build_agent.py, 9.70s, 2 compiled, 468 skipped)

## What the logging now proves

After this change, every export attempt will produce:
1. `logs/replay_export_debug.txt` — created at export start, contains full pipeline trace
2. `Debug::log` output in `Gameterminal_log_*.txt` — all parent-process export steps
3. `ReplayExport_log_*.txt` — subprocess-side log (only if subprocess spawns)

The debug file answers:
- Is export active? Is recording active? What tick?
- Did saveInstantReplay succeed? How many scene frames/sound events?
- Did clip.load succeed? What's the tick count?
- Did CreateProcess succeed? What PID?
- What FFmpeg path? Does it exist?
- What game version? What config?
- What happened at each stage? Where did it fail?

## Remaining Human Review
- Press P to export and verify `logs/replay_export_debug.txt` is created
- If export fails, check `replay_export_debug.txt` for the exact failure reason
- If subprocess spawns, check `logs/*/ReplayExport_log_*.txt` for subprocess-side diagnostics

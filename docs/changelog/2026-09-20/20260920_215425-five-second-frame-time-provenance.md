# Five-second frame-time provenance summary

Date: 2026-09-20

## Changed

- Preserved profiler source file, function, and exact line through `PerfFrame`.
- Replaced the JSONL frame-window output with a monotonic five-second aggregate.
- Added sorted contributors with calls, frames seen, self milliseconds, inclusive
  milliseconds, averages, and maximums.
- Included the active `events.jsonl` path and explicit timing units in the event.

## Validation

- Build: `SUCCESS`
- Executable: `mimita-20260920T215425.exe`
- Existing running EXE was not closed or replaced.
- New-session evidence:
  `logs/2026-09-21/20260921_015512/events.jsonl`
- Observed event:
  `performance.frame_time_summary`
- Observed provenance fields: `file`, `function`, `line`, `self_ms`,
  `inclusive_ms`, `calls`, and `frames_seen`.

## Runtime note

The build launcher started the new timestamped executable in a separate process.
The previous process and its world/session remained untouched.

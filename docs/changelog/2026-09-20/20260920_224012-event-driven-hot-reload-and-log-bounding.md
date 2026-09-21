# Event-driven hot reload and bounded performance logging

Date: 2026-09-20

## Changed

- Moved watcher-triggered manifest reload, cold-boundary checks, source hashing,
  and build enqueueing off the frame thread onto the existing hot-reload worker.
- The frame path now consumes watcher state and completed candidates.
- Added the recurring-work regression record.
- Changed ordinary performance spikes to use bounded logger repeat aggregation;
  severe spikes remain immediate errors.
- Increased the repeat aggregation window to five seconds.

## Validation

- `git diff --check`: completed with no whitespace errors.
- Cold build: SUCCESS.
- Executable: `mimita-20260920T223833.exe`.
- Existing running processes were not killed or replaced.
- New process log opened at:
  `logs/2026-09-21/20260921_024013/events.jsonl`.

## Not yet human-proven

The five-minute idle comparison, one-edit debounce test, failed-build retry
test, and compressed-log readback still require runtime acceptance. Source and
build evidence do not prove the frame-time improvement by themselves.

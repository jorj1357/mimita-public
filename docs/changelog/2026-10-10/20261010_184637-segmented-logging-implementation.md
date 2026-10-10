# Segmented canonical logging implemented

- Time: 2026-10-10 18:46:37 EDT
- Branch: `2026-10-10-Z-Tower`.
- Scope: segmented canonical `StructuredLogger` implementation, segmented log reader, focused runtime self-test, and matching documentation.
- Pre-existing edits: preserved. No unrelated configuration, entity-editor, terminal, or documentation changes were reverted.

## Implementation

- `src/debug/structured-log.cpp:62-64` defines the hard decimal limits: 100,000,000 bytes per segment, a 99,000,000-byte rotation target, and 1,000,000,000 bytes for the `logs` tree.
- `src/debug/structured-log.cpp:455-499` discovers numbered segments, migrates a legacy `events.jsonl` name for a new run, and re-reads the active segment’s actual size.
- `src/debug/structured-log.cpp:502-545` accounts for the complete `logs` tree and deletes oldest regular files before a reservation would exceed the 1 GB ceiling.
- `src/debug/structured-log.cpp:788-806` initializes segment state and performs startup quota cleanup under the shared named mutex.
- `src/debug/structured-log.cpp:1067-1181` serializes each record, rotates before the target, emits bounded metadata/drop records, reserves folder space, writes/flushes/closes the selected segment, and verifies the final segment size.
- `src/debug/structured-log.h:245-260` replaces the single persistent event-file state with segment path/number/size and quota/drop state.
- `src/devtools/dev-log-commands.cpp:14-133` makes `log_open` read every numbered segment as one logical stream.
- `src/game/game-cli.cpp:1403-1470` adds the focused real-executable segmented logger self-test.
- `docs/specs/debug-logging/canonical-jsonl.md:3-50` documents the numbered-segment reader contract.
- `docs/specs/debug-logging/debug-logging.md:3-10` records that the segmented writer is implemented while legacy parallel writers remain separate work.

## Validation

- Build: PASS. `python build_agent.py` completed successfully at 2026-10-10 18:44:56 and linked the updated `mimita.exe` reported by this checkout’s build system.
- `mimita.exe --structured-log-segment-selftest`: PASS. It created two valid segments; largest segment was 98,034,026 bytes and the test run was 105,036,926 bytes.
- Quota pressure: PASS. Repeated real-executable runs kept the recursive `logs` total at 940,930,674 bytes; 14 new `events-*.jsonl` segments were present and none exceeded 100,000,000 bytes.
- Concurrent shared-path check: PASS. Two simultaneous new executable self-tests both exited 0; the shared segmented stream had 1 segment, 2,894 bytes, and 0 invalid JSON lines.
- `mimita.exe --versioninfo`: PASS. It reported `EVENTS_JSONL_PATH=logs/10-10-2026/20261010_184557\\events-000001.jsonl`.
- Diff check: PASS for changed implementation/spec files, with the existing Git line-ending warning for `src/devtools/dev-log-commands.cpp`.

## Known boundary

- Two pre-existing processes from `.dev/builds/1921/mimita.exe` were already running before this implementation. They continue writing the legacy `logs/2026-10-10/20261010_220809/events.jsonl`, which measured approximately 298,750,647 bytes during validation. They were not terminated or modified. The new executable enforces segmentation; full live acceptance requires restarting those old processes through the updated executable.
- The implementation intentionally does not yet delete every parallel category/summary/terminal writer; that remains the documented Phase 3/4 migration work.

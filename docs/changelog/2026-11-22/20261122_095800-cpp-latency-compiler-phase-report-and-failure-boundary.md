# C++ latency: compiler phase report and failure boundary

## Changes

- Added bounded GCC `-ftime-report` parsing to the game DLL worker. For explicitly changed `.cpp` files, the parsed phase rows are attached to the existing per-translation-unit completion record under `compiler_timing`.
- The report option is diagnostic-only and is not included in persistent object-cache identity. If the compiler explicitly rejects the option, that translation unit is retried without it; ordinary source/compiler failures remain required candidate failures.
- Added six focused parser tests, including old and wall-only GCC report layouts, missing output, row bounds, and exact-limit behavior.
- The latency issue discussion records the existing failure rule: optional tools/accelerators may fail open with a degraded result; required compile/link/coherence/cache correctness must fail closed and preserve the last published DLL. It also records the observed Win32 priority-call FFI issue as a separate, unproven cause of build latency.

## Validation

- `python -m py_compile devscripts/game_build_worker.py devscripts/quality/test_game_build_time_report.py` — passed.
- `python -m unittest devscripts.quality.test_game_build_time_report` — 6 tests passed.
- `git diff --check -- devscripts/game_build_worker.py devscripts/quality/test_game_build_time_report.py` — passed (Git reports the worker's existing checkout line-ending conversion warning).

## Limits and next step

- No compiler or game build was started in this activation. The root dev-loop was live on run `20261122_144609_01`, generation 500, and other tasks were using the shared runtime. Therefore this instrumentation has not yet been proven in a real build-timing JSONL record, and its measurement overhead is unknown.
- The next safe verification is a root-dev-loop changed-source DLL request after shared runtime ownership clears, then inspect the `compiler_timing` record and compare its reported phases with the translation-unit wall duration. Preserve the existing last-good DLL when testing a deliberately broken required candidate.
- Keep the long-running edit-to-live goal active. No runtime behavior or sub-second target is claimed from this instrumentation-only change.

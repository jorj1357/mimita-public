# C++ latency worker priority and failure-containment follow-up

## Scope

Continued the active C++ edit-to-live investigation through the existing root dev-loop. No compiler, worker, alternate coordinator, or additional game instance was launched directly. No C++ gameplay implementation was changed; the temporary comment canary in `src/effects/zombie-tower-effects.cpp` was removed after measurement.

## Changes

- Fixed `devscripts/game_build_worker.py` priority telemetry ordering. It now constructs `Progress` before writing priority metadata; two attempted retries exposed the original `UnboundLocalError` at worker startup.
- Added typed Win32 priority-class setup with effective-class reporting and passed requested creation flags to compiler/linker process launches.
- Added a Windows focused test for the child creation flags in `devscripts/quality/test_game_build_time_report.py`.
- Extended [the build-latency issue](../issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md) with timing records, failure evidence, cache invalidation reasons, resource samples, and remaining measurement gaps.

## Validation and live build evidence

- `python -m py_compile devscripts/game_build_worker.py` passed.
- `python -m unittest devscripts.quality.test_game_build_time_report` passed: 8 tests.
- Focused `git diff --check` passed (only line-ending conversion warnings on Python files).
- Candidate attempts 501 and 502 published successfully through the root dev-loop. Attempt 501 took 944.347s (3 compiled, 640 skipped; 472.077s aggregate compile, 166.079s link); attempt 502 took 197.180s (2 compiled, 641 skipped; 97.084s compile, 58.841s link). These workloads differ and do not form a matched optimization A/B.
- Attempts `game-dll-20261122T161831011383Z-738c5a` and `game-dll-20261122T162454119696Z-a296b3` failed before compilation because the newly added telemetry referenced `progress` before construction. The ordering was corrected and focused tests passed.
- Corrected worker attempt `game-dll-20261122T163256885716Z-84a726` reached compilation. Its source set includes two changed `.cpp` files and `src/npc/npc.h`; the object cache explicitly invalidated 134 objects because that header was newer. The snapshot took 82.708s; the changed TUs took 589.518s and 621.760s. At the latest check, 92 translation units had started and 28 completed; no terminal result was present.
- A later candidate, `game-dll-20261122T171723932165Z-86a612`, failed in changed-source preflight because `src/npc/npc.cpp:3836-3837` references missing member `Npc::hasLoggedDash`. It was rejected before link/publish. `build/game/current.json` remained on generation 502, while the root server stayed alive. This demonstrates candidate failure isolation; it does not demonstrate successful recovery/publish of the newer source.

## Resource and priority observations

- One five-second sample during a live candidate recorded 95.4% host busy over 12 logical CPUs; server PID 4540 used 4.64% and client PID 30076 used 36.61% of total machine capacity. This sample does not assign all remaining CPU to MiMITA.
- During overlapping candidate activity, a process snapshot saw 11 `cc1plus.exe` and 11 `g++.exe` processes. Two candidate attempts were concurrently present in the event logs. Parent-PID attribution is incomplete, but one worker's `jobs=10` is not a global concurrency bound.
- The worker process was Windows High priority while compiler children were Normal. The new child flags did not prove priority reached the actual compiler processes; do not claim a build-speed improvement from this change.
- Ten recent client frame-budget samples averaged about 75.2ms; eight exceeded 50ms and two exceeded 100ms. The top logged scope varied. This shows user-visible delay during the observed high-load period, but does not isolate the build's causal share.
- A dev-loop status snapshot briefly listed no client PIDs even while PID 30076 was alive and still writing its journal. Process inventory should be treated as a separate evidence channel.

## Current status and next evidence

The active long goal remains open. The existing gold handoff already directs agents to preserve long-running goals across “continue” turns and treat optional-tool failures with safe fallbacks; that practice was followed here. The <=10s target is not met. Next work should capture worker ancestry and disjoint wall-time spans, establish a source-only `.cpp` test with no header change or concurrent edits, enforce a shared compiler-process budget across candidate workers, and get a matched CPU/frame/tick comparison before changing priority or build concurrency policy. Keep required compile/link/coherence failures fail-closed: reject only the candidate and retain the last-good generation; optional diagnostics may fail open with an explicit recorded fallback.

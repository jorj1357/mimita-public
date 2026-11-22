# CPU affinity and build worker history audit

## Question investigated

The user suspected a recent change had MiMITA claiming CPU cores or changing CPU use. The current source and recent history show no persistent CPU affinity assignment. The likely memory is build-worker concurrency and priority, which can create high temporary CPU demand during builds.

## Evidence

- Repository search across current `src`, `devscripts`, `build`, and `config` found no `SetProcessAffinityMask`, `SetThreadAffinityMask`, or `sched_setaffinity` implementation.
- A live read-only process snapshot showed MiMITA processes at Windows `Normal` priority with affinity mask `0xFFF` on a 12-logical-CPU machine. `0xFFF` means all 12 CPUs are allowed; it does not reserve them. The build worker was not running during this snapshot.
- Commit `d4898e0b4` (2026-11-19) added a worker fallback where `--jobs 0` means `os.cpu_count()` (12 here). The root build launcher passed `--jobs 0` until commit `d89884f64` (2026-11-21), which changed it to `--jobs 10`. Thus the earlier setting could compile on all 12 reported logical CPUs; the current explicit setting reduces that to 10. This is the strongest recent “claim cores” candidate, and it only applies while compiling.
- Dependency-stat prewarm uses up to 16 Python threads (`min(16, max(2, cpu_count * 2))`). The latest timing file, `.dev/build-timings/game-dll-20261122T022936769355Z-e0de53.json`, reports only 173.5 ms for stat prewarm, so that setting is not currently supported as a sustained CPU cause.
- The build launcher passes `--priority high`; the worker maps this to Windows `HIGH_PRIORITY_CLASS` (`0x80`). The priority class and `--priority high` predate the suspected recent period (worker implementation dates to Nov 12; caller to Nov 14), so this is not a new persistent MiMITA affinity change. It can favor active compile work while a build runs.
- That same DLL build attempt took 131.9 seconds total, compiled 12 of 642 sources, skipped 630, and spent 38.5 seconds linking, 14.5 seconds publishing, 27.7 seconds in changed-source preflight, and 17.6 seconds in the compile phase. Per-TU compile durations sum to 133.5 seconds because they run in parallel; do not treat that sum as wall time.
- Previous runtime sampling showed MiMITA gameplay processes using about 7.4 core-equivalents while multiple game runs were concurrently active. No build worker was running during that sample, so it does not establish build contention or explain the idle playerless-server CPU cost.

## Conclusion and next discriminating measurement

There is no evidence that MiMITA permanently pins or reserves CPU cores. There is evidence that the build path used all 12 logical CPUs for compiler concurrency from Nov 19 until Nov 21, when the launcher changed to 10; builds still request high process priority. This can temporarily compete with gameplay and desktop work but is not the cause of MiMITA's CPU use between builds.

To determine whether build-worker settings materially explain slow builds, compare the same incremental build under the same cache/source state with a low-background baseline and the normal concurrent game load. Record CPU use by compiler worker, game processes, and host; compare build request queue time separately from compile/link wall time. Do not change affinity or priority until that comparison shows a benefit and the interactive CPU tradeoff is accepted.

No source or process settings were changed in this audit. The CPU issue remains open.

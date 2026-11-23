# Current live resource sample and profiler attribution constraints

## Session-specific evidence

This activation advanced the active resource-efficiency objective. I sampled
the existing client/server processes without starting a build, WPR capture, or
game process. The measured process CPU and memory values were appended to
`issue-ed3ac775283d`. The runtime journal path reported by dev-loop is absent,
so I cannot match the sample to the actual in-game mode or gameplay workload.

## Exact source changes

- No C++ or configuration source changed.
- Created this changelog; there was no prior text.
- Appended the current live resource sample and profiler threading caveat to
  `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md`.

## Current process and host measurements

- Dev-loop identified server PID 8804 and client PID 10148 on a 12-logical-CPU
  machine. The active processes were alive in every row of their captures.
- Process capture windows: server
  `.dev/perf-captures/live-resource-20261123T0959Z-server.csv`,
  09:58:49.725–09:59:20.735 UTC (32 rows); client
  `.dev/perf-captures/live-resource-20261123T0959Z-client.csv`,
  09:58:49.601–09:59:20.605 UTC (32 rows).
- Host capture:
  `.dev/perf-captures/live-resource-20261123T0959Z-host.csv`,
  09:58:52.879–09:59:24.657 UTC (32 rows). During the shared 27.726-second
  overlap, host CPU averaged 83.25%; server averaged 329.16% of one logical
  CPU (27.43% of whole host); client averaged 376.80% of one logical CPU
  (31.40% of whole host); combined pair averaged 58.83% of whole-host
  capacity.
- During the paired process windows, server mean working set was 4,941.3 MiB
  and private bytes 8,184.5 MiB; client mean working set was 437.8 MiB and
  private bytes 1,817.9 MiB. Combined working set was 5,379.1 MiB. Combined
  private bytes were 10,002.4 MiB; this is a separate committed-memory metric,
  not an additional working-set/RAM total.
- The CPU readings are process CPU-time deltas divided by elapsed wall time;
  whole-host normalization divides the one-core percentage by 12. Host total
  is the Windows `Processor(_Total)\% Processor Time` counter. A first host
  export incorrectly read the wrapper object's `CookedValue` and contained
  zeros; it is invalid and excluded. A second corrected host capture was
  timestamp-misaligned and is also excluded. The `0959Z` triplet is the
  matched artifact set.

## Workload identity and limits

- The expected `logs/2026-11-23/20261123_055730_01/dev-loop/events.jsonl`
  path did not exist at inspection. Dev-loop launch identity reported
  `jugger_horde`, map `dust2cyberiav4`, NPC count 1, but this does not prove
  actual in-game state after mode switching.
- Actual in-game mode, player/NPC counts, loaded generation, frame cap/FPS
  distribution, 60 Hz tick timing, and workload activity are unknown.
- GPU use, per-process disk/network I/O, thread count/CPU distribution, and
  competing process identities were not measured. Dev-loop status shortly
  afterward showed the run alive and waiting for an explicit batch build; this
  does not prove its phase throughout every sample row.
- Therefore this is direct live pair resource evidence, not a matched idle
  baseline, build-pressure measurement, or function attribution. The pair was
  high-CPU and high-memory during this interval; the goal's idle thresholds
  cannot be scored without verified workload identity.

## Profiler implementation review

- `src/perf/perf-spike.cpp:172-175` implements `PerfScopeGuard::readCycles()`
  with `std::chrono::steady_clock`; it measures elapsed wall time, not thread
  CPU time.
- Scope parent lookup and stack push/pop use process-global
  `gPerfScopeStackDepth` and `gPerfScopeStack` at
  `src/perf/perf-spike.cpp:183-235,289-291`, declared in
  `src/perf/perf-spike.h:86-90`. `PerfScopeCapture` has no thread ID. Directly
  adding `GetThreadTimes` to each scope risks cross-thread parent
  misattribution/races, and would add measurement overhead to every scope.
- A safer CPU-time profiler needs per-thread scope/parent state plus bounded,
  synchronized aggregation/snapshot semantics (or a deliberately restricted
  thread allowlist). Keep it opt-in; disclose sample coverage, lost scopes,
  and overhead; compare matched profiler-on/off runs before trusting results.

## Validation and completion state

- Measurement and aggregation scripts completed successfully for the
  `0959Z` artifacts. No code build, runtime behavior check, GPU capture, or
  human gameplay review occurred.
- The resource-efficiency objective remains active and unproven; no optimization
  was made in this session.

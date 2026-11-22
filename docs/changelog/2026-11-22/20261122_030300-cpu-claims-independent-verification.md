# CPU "claiming cores" claim: independent history verification

Agent: cpp-latency-pressure-profile (wake-b775aa7362b5)
Context: ctx-20f2a628316f, "i think it was somejing else like some days ago changing cpu usage and claiming cores can u investgate"

## Finding

Read-only investigation. No source, config, process, or build settings were
changed. The user's "claiming cores" memory matches build-worker concurrency,
not a persistent MiMITA CPU-affinity reservation.

- No `SetProcessAffinityMask`, `SetThreadAffinityMask`, or `sched_setaffinity`
  exists in current `src`, `devscripts`, `build`, or `config`.
- Live `mimita.exe` PIDs 18664/24860/26748 are Windows `Normal` priority with
  `ProcessorAffinity = 0xFFF` on a 12-logical-CPU host: all 12 CPUs are allowed,
  none are reserved.
- The `FramePacer` spin-wait was removed in commit `85f49500` (2026-08-23); only
  a bounded `sleep_for` remains (`src/video/frame-pacer.cpp`).
- Server loops sleep when no fixed-step debt remains
  (`src/network/server.cpp:1828-1835`, resident loop `:2535`); there is no
  busy-spin CPU owner.
- Recent CPU-relevant change: `d4898e0b` (2026-11-19) made
  `game_build_worker.py --jobs 0` resolve to `os.cpu_count()` and the dev-loop
  passed `--jobs 0`, so builds could compile on all 12 logical CPUs.
  `d89884f` (2026-11-21) set `devscripts/dev_loop/game_build.py` to
  `--jobs 10` and still passes `--priority high`
  (`HIGH_PRIORITY_CLASS`, `devscripts/dev_loop/build.py:95`).
- Current idle-game CPU is NPC/world simulation, not spin or affinity: the
  playerless FCS server calls `serverSimulateWorld` every tick
  (`src/network/server.cpp:1537-1543`), consistent with the evidence already
  recorded on issue-ed3ac775283d.

## Conclusion

Builds, not idle MiMITA, can temporarily claim many cores and request high
priority. This matches the prior audit in
`docs/changelog/2026-11-22/20261122_023300-cpu-affinity-and-build-worker-history-audit.md`.
The discriminating test remains a matched same-source build under low vs normal
game load. No fix or completion is claimed.

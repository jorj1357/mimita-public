# Current six-process MiMITA resource sample

## Goal and experiment rule

The active resource-reduction goal remains in progress. The user's latest
instruction was recorded once in the coordination brain and linked to
`issue-ed3ac775283d`. A task-scoped directive now requires each game-DLL build
requested for this CPU goal to contain exactly one source CPU treatment chosen
from function-level attribution, with a matched before/after run. The result
must decide keep, revise, or revert and preserve negative outcomes. Build
worker latency experiments stay separate. If a live owned workload or usable
CPU attribution is missing, do not make a speculative gameplay change.

## Bounded host and process sample

- Artifact: `.dev/perf-captures/cpu-orphan-six-win32-2026-11-25T0333Z.csv`
- UTC: 2026-11-25 03:36:23.089–03:36:38.178 (15.09 seconds)
- Method: Python standard-library `ctypes` calls to Win32 `GetSystemTimes`,
  `GetProcessTimes`, `GetProcessMemoryInfo`, and `GlobalMemoryStatusEx`; fixed
  list of six PIDs, approximately one-second intervals, no process discovery
  per interval. Per-process rates were recalculated using measured UTC sample
  intervals (mean 1.0047 seconds). Sampler CPU time was not separately
  instrumented.
- Host: 12 logical CPUs; CPU mean 97.32% (range 91.90–100%); physical memory
  load 94.2%; available RAM mean 812.8 MB, minimum 735.2 MB; pagefile use
  averaged 76.82%.
- All six PIDs used the same EXE path:
  `.dev/builds/2026-11-25T01-21-28.987918Z/mimita.exe`.

| PID | Role evidence | CPU of one logical CPU | Whole-host CPU | Working set | Private bytes |
|---:|---|---:|---:|---:|---:|
| 36452 | Previously identified server | 202.80% | 16.90% | 1655.0 MB | 5374.8 MB |
| 25552 | Previously identified client | 180.73% | 15.06% | 282.9 MB | 1624.2 MB |
| 13336 | Previously identified server | 91.26% | 7.60% | 38.9 MB | 139.2 MB |
| 36632 | Previously identified client | 163.82% | 13.65% | 124.4 MB | 1242.5 MB |
| 6244 | Server candidate from launch arguments | 69.47% | 5.79% | 37.5 MB | 124.0 MB |
| 38720 | Client candidate from launch arguments | 124.52% | 10.38% | 110.6 MB | 858.4 MB |

The six processes together averaged 69.38% whole-host CPU, about 8.33 logical
cores. Their summed mean working set was about 2.25 GB; summed private bytes
were about 9.36 GB. These are process resource measurements, not function
attribution.

## Evidence limits and decision

Root dev-loop reported no registered live run or active build. WPR was not
recording. The three parent launcher PIDs had exited. Launch arguments showed
`ssbm` and one NPC for the server candidates, with maps
`dust2cyberiav4` or `slaughterhouse1`; actual in-game modes, players, NPCs,
frame data, tick data, and current journal alignment remain unknown because
these processes are outside root runtime ownership. Top competing processes,
GPU/disk/network use, and sampler self-CPU were not measured. The sample does
not prove memory paging caused the slowdown or identify a source function.

No runtime optimization or CPU-focused build was performed. Do not send
commands to or restart these orphaned processes. The next discriminating step
is to resolve ownership or provision a fresh root-dev-loop run after memory
headroom recovers, verify the exact workload and generations, then run the
existing bounded callsite CPU probe before selecting one treatment. Evidence
and handoff are recorded in `issue-ed3ac775283d` (`msg-b30bfa99dccd`).

## Correction after journal reconciliation

The local `dev-loop --status` view was stale at the sample time. The canonical
run journals were actively advancing, so the first two PIDs were a verified
pair in run `20261125_014732`, not orphan processes. During the exact
03:36:23.089–03:36:38.178 UTC sample, server PID36452 and client PID25552 ran
Juggernaut Horde on `dust2cyberiav4` with 67 NPCs and one player. The other
four PIDs remain outside that verified pair.

- Server events recorded 11 tick windows / 110 ticks. The mean of the 11
  `total_avg_ms` window values was 129.88 ms; 59.09% of reported ticks
  exceeded 100 ms. There
  were 65 `performance.server-tick-stall` rows with mean 180.05 ms and maximum
  423.094 ms.
- Client events recorded 61 frame-slice rows covering 202 frames; all 202
  exceeded 4.167 ms. Mean of per-slice average frame times was 89.96 ms and
  mean per-slice p95 was 124.60 ms.
- Server host DLL generation was 635, while every in-window NPC phase row
  reported module generation 632. This is not a generation-aligned baseline
  for an NPC-module treatment. No `performance.cpu-callsite-window` rows were
  present, so the sample still does not identify CPU cycles by function.
- The target pair used 31.96% whole-host CPU (server 16.90%, client 15.06%);
  all six MiMITA processes used 69.38%. Host CPU averaged 97.32%, available
  RAM averaged 812.8 MB (minimum 735.2 MB), and pagefile use averaged 76.82%.

This is strong evidence of a high-load active-play failure to meet frame and
tick budgets, but not function-level CPU attribution or proof that memory
pressure caused the slowdown. Root command routing and the host/NPC generation
mismatch must be reconciled before the bounded callsite probe or a source
treatment. The correction and coordination handoff are in
`issue-ed3ac775283d` (`msg-157358e75534`) and the related build issue
`issue-136d4553ff92` (`msg-8c55f8a5e74f`).

Do not use `.dev/perf-captures/cpu-orphan-six-current-2026-11-25T0333Z.csv`:
its preliminary PowerShell post-processing failed, and its process rates used
the host-counter interval rather than measured process-snapshot intervals.
The valid corrected artifact is the Win32 fixed-PID CSV listed above.

## Current registered run identities (03:53 UTC)

`dev-loop.py --list-test-runs` shows three online runs, correcting the stale
single-run view from `--status`. The latest canonical journals confirm:

- `20261125_014732`: server PID36452 and client PID25552, Juggernaut Horde,
  `dust2cyberiav4`, 67 NPCs / 1 player. Latest rows at 03:53Z show host DLL
  generation 635; a fresh NPC phase row at 03:54Z still reports module
  generation 632.
- `20261125_020100`: server PID13336 and client PID36632, Bloodtest on
  `slaughterhouse1`, 1 NPC / 1 player, host DLL generation 635. A 03:53 client
  frame slice records 6/6 frames over 4.167 ms, p95 68.10 ms.
- `20261125_022449`: server PID6244 and client PID38720, Bloodtest on
  `slaughterhouse1`, 1 NPC / 1 player, host DLL generation 635. A 03:53 client
  frame slice records 14/14 frames over 4.167 ms, p95 50.97 ms.

These are registered active test runs, not unowned orphan processes. Their
current journals prove the three workloads but no new CPU rates were sampled
at 03:53Z; do not extrapolate the 03:36 CPU percentages forward. The two
Bloodtest runs are material confounders for a one-pair optimization A/B and
must be released by their owners before a clean single-pair treatment test.

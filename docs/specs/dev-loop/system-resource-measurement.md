// 2026-11-26
/* purpose
* define repeatable, low-overhead measurement of host and MiMITA CPU/memory use
* attribute every process to a verified runtime/build or label the identity unknown
* separate dashboard cost from whole-host pressure and concurrent workloads
* this file DOES NOT authorize stopping or adopting processes
* this file DOES NOT replace runtime journals, build authority, or process ownership
*/

# System resource measurement

**Status:** Desired behavior contract and measurement standard.

## Purpose and audience

This specification defines how MiMITA contributors measure system resource
pressure, identify the processes consuming it, and compare dashboard/build/run
changes. It applies to the Windows development machine, the dev-loop daemon and
dashboard, MiMITA servers and clients, compiler workers, and other material
competitors on the host.

The goal is to make claims falsifiable and to explain the host as a whole while
keeping each process's contribution separate. A high host CPU or low available
RAM value is not, by itself, proof that the dashboard or a MiMITA process
caused it.

## Authority and boundaries

- The root dev-loop and runtime registry remain the process/run authorities.
- Reuse `devscripts/dev_loop/build_resource_sampler.py` for build-time resource
  samples. Do not introduce a competing build sampler.
- The dashboard remains a view. It may display a completed or explicitly
  requested resource snapshot, but it must not add heavyweight per-refresh
  polling merely to collect measurements.
- Measurement is read-only. Never stop, restart, adopt, or relabel a process
  based only on its resource use or a similar executable name.
- Existing runtime journals remain the authority for the live mode and workload
  when they disagree with launch-time arguments or the dashboard's launch mode.
- A sample is evidence, not a process-control permit.

## Required sample identity

Every report records:

- unique sample ID, UTC start/end timestamps, monotonic duration, sample
  interval, collector name/version, and collector PID;
- host OS, processor model when available, logical CPU count, installed
  physical memory, and pagefile/commit limit;
- target process PID **and creation time** (to detect PID reuse), executable
  path, parent PID, command line, role, and whether identity was verified;
- run ID, runtime/instance ID, agent, issue, EXE build identity, DLL generation
  and SHA-256, and exact journal paths when available;
- exact live mode/map and players/NPCs from the active runtime journal when
  present; otherwise record the source and mark the value unknown;
- workload state: dashboard visible/minimized, open sections, game visible or
  headless, player/NPC counts, frame cap/FPS or server tick data when
  available, and whether a build or linker was active;
- top competing process IDs/roles and their CPU and memory values over the
  same time window.

Missing values are `unknown` or `unavailable`, never silently treated as zero.
Separate direct observations, inferences, and unresolved identity gaps.

## CPU metrics and units

For each process, capture cumulative user+kernel CPU time at sample start and
end. Normalize the delta against the measured monotonic duration:

```text
one_logical_cpu_percent = 100 * process_cpu_seconds / duration_seconds
whole_host_percent = one_logical_cpu_percent / logical_cpu_count
```

The first value may exceed 100% for a multithreaded process. The second is the
process's share of total logical-CPU capacity. Always label which scale is
reported. Never call a per-logical-CPU percentage a whole-host percentage.

Also capture total host CPU percentage over the same interval. Report process
CPU separately from host total, and list top competing processes by their
whole-host CPU share. Rank both total contributors and the MiMITA/build
processes so an important MiMITA process cannot disappear below unrelated
host processes.

For a series of one-second (or explicitly stated interval) samples, report
mean, p50, p95, p99, maximum, and the sample count. Also report cumulative CPU
seconds over the full interval. A single Task Manager reading or one average
cannot establish a stable dashboard cost.

## Memory and paging metrics

Capture these per process:

- resident working set/RSS and peak working set;
- private bytes/USS (the process's private committed memory) and peak private
  bytes when available;
- process thread count and process I/O read/write byte deltas when available.

Capture these for the host:

- total physical RAM and available physical RAM (start, minimum, end);
- total committed bytes and commit limit (start, maximum, end);
- pagefile/swap used and total (start, end), plus page-in/page-out or hard
  fault rates when the operating system exposes them;
- total host CPU over the same interval.

Working set is resident memory, not total memory charged to a process. Private
bytes are not resident RAM. Host commit is not the sum of process working sets.
Do not sum process working sets and present the result as unique host RAM;
shared pages can be counted more than once. State each metric and unit
explicitly.

Flag host pressure when available RAM falls below 10% of installed RAM or 1 GB,
or when committed bytes exceed 90% of the commit limit. These are diagnostic
flags, not automatic stop thresholds. Report pagefile growth and hard-fault
activity as additional evidence; do not infer paging solely from low available
RAM.

## Identity and ownership

Reconcile each MiMITA PID with the current run manifest/runtime registry using
PID, process creation time, executable, run ID, and journal. Use the live
journal to confirm the effective mode/map. For external or unregistered
processes, show the exact command-line purpose clues that are available, label
run/agent/issue ownership as unknown, and keep the process unverified until
matching evidence is found.

Do not attribute a child process to a run solely because its parent PID belongs
to that run. Record both parent/child identities and the missing run/journal
links. Do not mark a process unused merely because no matching local client or
visible window was found.

## Measurement protocol

For an idle-dashboard comparison:

1. Pin the current dashboard, daemon, runtime, and build identities. Record the
   active game processes and workload before changing the dashboard.
2. Collect a dashboard-closed host baseline when it can be done without
   stopping or disturbing a human- or agent-owned game/runtime process.
3. Start one dashboard through the existing checkout dev-loop path. Confirm its
   PID and window identity, and verify that the existing daemon, builds, and
   game pairs were preserved.
4. Allow 60 seconds for startup and data caches to settle. Sample for at least
   5 minutes at one-second resolution so periodic refresh and coordination
   work is included. Record whether the dashboard is visible or minimized and
   which sections are expanded.
5. Repeat at least three times for each before/after condition. Keep the same
   runtime workload and build state; report any mismatch as a confounder.
6. For build pressure, capture a matched host baseline immediately before the
   build and continue through the build. Keep build-stage identity and top
   compiler processes aligned with each sample. A build sample does not replace
   the idle-dashboard sample.

Change one variable at a time. Record inconclusive or worse results. Do not
manufacture a failing build or kill a process to create a measurement state.

## Collector overhead and sample validity

The collector must use an in-process OS API or an already available low-cost
library. It must not spawn PowerShell/WMI or enumerate every process every
second when a cached PID set and native process counters suffice. Include the
collector PID in samples and measure its CPU and memory too.

The collector should consume at most 0.25% of whole-host CPU on average over a
60-second sample. If it exceeds that budget, improve or lower its sampling
frequency and mark affected samples as instrumented/overhead-confounded. Do
not let telemetry delay daemon controls, builds, or live gameplay.

Sample identity must remain stable across the interval. If a target exits or
its PID is reused, split the sample and report both identities. Failed reads,
permissions, and unsupported counters must be explicit; a partial sample must
not be presented as complete.

## Initial dashboard budgets

These are initial engineering targets for an idle dashboard on the reference
Windows development machine; report the hardware and workload beside them:

- mean dashboard CPU below 1% of whole-host capacity over a five-minute idle
  sample, with the one-logical-CPU percentage also reported;
- after a 60-second warm-up, p95 private bytes at or below 100 MB and p95
  working set at or below 125 MB;
- during a 15-minute idle observation, private bytes grow by no more than
  10 MB after warm-up, with no persistent upward trend;
- process/status controls remain responsive, and the dashboard continues to
  meet its one-second freshness contract in
  [the dashboard specification](dashboard.md).

These are measured targets, not reasons to hide status or process visibility.
If a target is missed, report the exact process stage and measured workload,
then reduce unnecessary work without silently weakening freshness or ownership
requirements.

## Reporting format

For every comparison, record:

```text
Hypothesis:
Single change:
Target metric and threshold:
Host and logical CPU count:
Runtime/build identity and workload:
Sample ID, start/end UTC, duration, interval, sample count:
Dashboard CPU: one-logical-core % / whole-host %; mean/p50/p95/p99/max:
Dashboard memory: working set and private bytes; mean/p95/max and growth:
Host CPU, available RAM, commit/limit, pagefile and pressure flags:
Top competing processes by CPU and memory:
Collector CPU/memory overhead:
Journal, runtime manifest, and sample artifact paths:
Direct observations:
Inferences:
Unknowns/confounders:
Decision and next measurement:
```

Attach resource evidence to the owning CPU/resource issue and cross-link any
separate runtime, build, or process-identity issue. Preserve raw samples and
report aggregates; never report only the metric that improved.

## Current measurement example (2026-11-26)

After restarting the checkout-owned dashboard, PID 35600 used 16.08 CPU
seconds over 68.05 seconds: 23.63% of one logical CPU and 1.97% of whole-host
capacity on 12 logical CPUs. Its working set averaged 87.1 MB (91.5 MB maximum)
and private bytes averaged 71.0 MB (75.2 MB maximum). The earlier long-lived
dashboard PID 35740 had 1,468.8 MB private bytes and about 86 MB working set;
the fresh process began near 57 MB private bytes and remained near 57–75 MB in
the measured windows. This is evidence of substantial long-uptime private
memory retention, but the cause is not yet identified. Restarting is a
mitigation, not a fix.

The same 68.05-second host window had 70.09% mean host CPU, 96.7% p95, and
100% maximum. Available physical RAM ranged from 1,238 MB to 3,463 MB. The
largest non-dashboard CPU contributors included Defender, System, Desktop
Window Manager, ChatGPT, Task Manager, and VS Code. There was no active build.
This host-wide load is a confounder and cannot be attributed to the dashboard.

During a separate DLL build sample, available physical RAM fell to 0.7 MB and
system commit approached its limit while the MiMITA client/server and compiler
workers used resources. Treat that as correlated build pressure; do not infer
that the dashboard caused it. See [the host-resource issue](../../issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md)
and [the dashboard CPU issue](../../issues/performance/20261125-dev-loop-dashboard-should-cost-under-1-percent-c-issue-acf28dc22c85.md)
for the raw issue discussion and follow-up.

## Post-refresh five-minute measurement (2026-11-26)

After the dashboard refresh path was changed to reuse its already-read daemon
snapshot for checkout state, dashboard PID 1328 was sampled for 300 one-second
intervals after a 60-second warm-up. From 23:06:48.800Z to 23:12:11.104Z it
used 109.36 CPU seconds over 322.3 seconds: 33.93% of one logical CPU and
2.83% of 12-logical-CPU whole-host capacity. Per-second CPU p95 was 95.3% of
one logical CPU (maximum 109.4%). Working set mean/p95/maximum was
92.4/122.3/123.1 MB; private bytes mean/p95/maximum was 95.6/125.6/126.3 MB.
Collector CPU was 0.22 seconds (0.006% whole-host capacity).

The sample misses the mean CPU target (<1% whole host) and private-byte p95
target (<=100 MB); working-set p95 remains within its 125 MB target. This is
not evidence that the refresh-path change improved CPU: the earlier 68.05-
second sample measured 23.63% of one logical CPU / 1.97% whole-host, so the
observed mean went up. These samples are not a matched A/B: run
20261126_230039 changed during the later sample and run 20261126_231000 began;
the client was offline by the later sample's warm-up end. Host CPU averaged
84.68% (p95 and max 100%), available physical RAM fell as low as 455.6 MB,
and commit reached 31,418.8 MB of 36,233 MB. The dashboard was the largest
identified application process contributor after Defender, System, and DWM,
but no matching live MiMITA workload was present in the final sample summary.
These load conditions and the run transition prevent causal attribution.

Raw one-second samples and summary:
`.dev/resource-measurements/20261126T230539Z-dashboard-post-refresh.jsonl`.
The sample's launch/runtime alignment and transition must be read with
`logs/2026-11-26/20261126_230039/{server-01,client-01}/events.jsonl` and run
`20261126_231000`; the process target was PID 1328. The implementation change
removes one redundant daemon status request per GUI refresh, but current
measurements do not prove a dashboard CPU reduction. Profile the refresh stages
and obtain matched visible/hidden idle samples before selecting another change.

## Runtime-row caching iteration (2026-11-26)

The GUI previously destroyed and recreated every independent-runtime row on
each one-second dashboard refresh, even when all displayed runtime values were
unchanged. `devscripts/dev_loop/gui.py` now caches the displayed row signature
(runtime ID, mode, map, server/client liveness, and port) and rebuilds the panel
only when one of those visible values changes. A fresh dashboard process (PID
32132) loaded the change.

A five-minute sample after a 60-second warm-up, sample ID
`20261126T231900Z`, ran 2026-11-26T23:21:06.125Z–23:26:06.124Z for 300
one-second observations. Dashboard PID 32132 used 60.73 CPU seconds: 20.24%
of one logical CPU and 1.687% whole-host capacity (12 logical CPUs). Working
set mean/p95/max was 79.82/81.93/89.29 MB; private memory was
63.87/65.90/72.39 MB. Host CPU mean/p95/max was 75.11/96.3/100%; available
physical RAM minimum was 2,692.6 MB.

The new sample meets the initial RAM p95 budgets (working set <=125 MB and
private bytes <=100 MB), but the CPU target (<1% whole-host mean) remains
missed. Against the preceding unmatched sample (33.93% one core / 2.83% whole
host; working-set p95 122.3 MB; private p95 125.6 MB), the measured values are
lower, but this is not a controlled A/B: the earlier run had transitioned and
its client was offline, while this sample used run `20261126_231947`, mode
Vehicle Arena - NPC flip-reset freestyle probe, map arena2, one player and zero
NPCs. A game DLL build was reported active at sample start and generation 742
was published by the post-sample status check; exact build overlap is unknown.
Host load, game workload, build overlap, and runtime-row changes can all affect
the result. Do not claim the code change alone caused the reduction.

The sample collector did not capture same-window top-process CPU, same-window
commit/pagefile values, or its own CPU overhead; these are unknown for this
sample. A later host snapshot showed commit 28,128 MB / 37,993 MB, but it is
not a substitute for in-window data. Raw samples are at
`.dev/resource-measurements/20261126T231900Z-runtime-rows-cache.jsonl`.
The journal `logs/2026-11-26/20261126_231947/server-01/events.jsonl` confirms
vehicle_arena / arena2 with one player and zero NPCs. Client journal:
`logs/2026-11-26/20261126_231947/client-01/events.jsonl`. Seven seconds of
post-sample client FPS entries (23:27:06Z–23:27:13Z) ranged 336–447 FPS, with
per-second p95 frame times 2.95–3.57 ms; this is separate from the measured
window and does not validate the user's reported 60-FPS scenario.

## Lazy coordination iteration and stressed-host sample (2026-11-27 UTC)

The COORDINATION section now starts collapsed when there is no saved user
choice. While collapsed, the dashboard does not request a coordination snapshot
or render agent/issue/message rows; opening it resets the refresh timer so the
next status refresh requests current data. The process and main status panels
remain live. After restarting dashboard PID 1828, I observed no
`dashboard.coordination.*` events while the section stayed collapsed through
sample `20261127T000100Z`. This verifies the idle panel did not run its periodic
read during that window; it does not measure CPU saved by this one change in
isolation.

That 300-sample, one-second measurement ran 2026-11-27T00:02:42.616Z–
00:07:42.608Z after a 60-second warm-up. Dashboard PID 1828 used 81.58 CPU
seconds: 27.19% of one logical CPU and 2.266% of 12-CPU whole-host capacity.
Working set mean/p95/max was 93.96/131.64/151.57 MB; private memory was
84.31/115.58/146.17 MB. Host CPU mean/p95/max was 90.83/99.8/100%; physical
RAM available fell to 0.4 MB. Commit usage peaked at 38,698.1 MB with a
contemporaneous limit of 39,697.2 MB; pagefile use peaked at 6,968.5 MB.
Collector use was 0.844 CPU seconds (0.0234% whole-host capacity).

CPU and private/working-set p95 targets remain missed. The host was under
severe pressure, and the root dev-loop changed from a server-only FCS run to a
TDM server/client run during this sample; generation/build status was idle.
This is not a matched A/B and does not establish whether the lower dashboard
CPU than the previous sample came from the lazy coordination change. The top
whole-host CPU contributors were FL64 PID 32776 (10.69%), Desktop Window
Manager PID 1984 (5.297%), unverified MiMITA server PID 5440 (5.193%), Defender
PID 7060 (4.774%), Windows System PID 4 (3.673%), OpenCode PID 6988 (2.619%),
and dashboard PID 1828 (2.29%). End-of-window private-memory leaders included
FL64 (4,429 MB), managed MiMITA server PID 11572 (1,368 MB), ChatGPT PID 20868
(942 MB), Desktop Window Manager (899 MB), and OpenCode PID 6988 (850 MB).
These process measurements identify contributors; they do not establish why
the total host pressure occurred.

Process discovery also found unverified MiMITA server PID 5440, started
2026-11-26T23:47:06.969Z as a child of client PID 27864. Safe launch arguments
indicate `vehicle_freeplay`, map `arena2`, and name `MiMITA Server`; it has no
run ID or journal. It used 5.193% whole-host CPU in the five-minute sample and
8.09% during a separate 60-second sample with the dashboard closed. A second
unverified server PID 20596 appeared during the window as a child of managed
client PID 24528. Its safe arguments indicate `team_deathmatch`, map
`dust2cyberiav4`, and one NPC, which differ from the managed TDM server's
`atdm` map and `tdm npc elite` mode. It was created after the per-process CPU
baseline, so its sample-window CPU delta is unknown. Cross-agent ownership
signals received no owner answer. Neither process was controlled or stopped;
its launch owner and purpose remain open.

Raw samples: `.dev/resource-measurements/20261127T000100Z-coordination-lazy.jsonl`,
`.dev/resource-measurements/20261126T234300Z-daemon-inventory-cache.jsonl`, and
`.dev/resource-measurements/20261126T235300Z-dashboard-closed-fps-ab.jsonl`.
The 60-second closed-dashboard window had no FPS events because the client
journal's last event was 2026-11-26T23:50:43.453Z. The earlier five-minute
visible sample recorded 265 FPS events averaging 423.7 FPS (min 201.7, max
700.1); this does not reproduce the reported 60-FPS scenario. No valid
open/closed FPS conclusion is available.

## Dashboard-visible live sample — 2026-11-27 UTC

A 300.06-second read-only sample covered 2026-11-27T00:21:44.841Z–00:26:44.906Z while dashboard PID 1828 was visible. It produced 284 nominal one-second rows on a 12-logical-CPU host. The client/server stayed in dev-loop run 20261127_001718, mode team_deathmatch / gamemode tdm, map atdm; server PID 11572 and client PID 29544. Sample artifact: .dev/resource-measurements/20261127T002100Z-dashboard-open-current-run.jsonl.

Dashboard CPU mean/p95/max was 2.335/4.875/7.031% of whole-host capacity (28.019/58.498/84.375% of one logical CPU). Working set mean/p95/max was 94.93/112.28/134.08 MB; private bytes mean/p95/max was 114.30/130.39/159.92 MB. The whole-host CPU and private-memory targets remain missed; working-set p95 target passed. The collector's own mean overhead was 0.260% of host capacity (p95 0.521%, peak 5.472%).

Host CPU was 99.711% mean, 100% p95 and maximum. Physical RAM available averaged 2294.56 MB and reached 55.31 MB minimum; the artifact now records physical_ram_used_estimate_mb (total minus available). System commit charge and limit were not captured by this collector and remain unknown. Pagefile use reached 7363.88 MB maximum. Do not treat physical RAM totals as commit charge.

The game journal recorded 175 FPS windows only through 00:24:39.519Z, not the full resource window. Mean/min/max FPS was 150.998/59.988/203.335. Mean per-window average frame time was 6.75 ms; mean per-window p95 and p99 were 8.999 and 13.165 ms; the largest reported frame was 875.987 ms. These windows include brief dips around 60 FPS and severe spikes, but do not establish that the visible dashboard caused them. The server journal reported one player, a mean of nine NPCs across 168 tick windows, and 0.754% mean / 10% maximum ticks over 100 ms.

DLL generation 744 (game-dll-20261127T002302708617Z-988794) began at 00:23:02.708Z, overlapping about 3m42s of this measurement; no matched pre-build baseline or system-wide top-process sample exists. Known same-window contributors were client PID 29544 (20.027% whole-host CPU mean; 4599.92 MB private at end), server PID 11572 (6.682%; 1396.25 MB private), unverified server PID 20596 (2.686%; 517.83 MB private), and unverified server PID 5440 (1.646%; 303.02 MB private). Build and other host-process attribution is incomplete; do not claim that the build or dashboard caused the host pressure. The two unverified servers have no run ID or journal; see issue issue-b9edd402e658.

Separate dashboard stdout timing observations from 00:29:18–00:33:35Z show 20 dashboard.refresh.slow records (maximum 2061.9 ms), 43 dashboard.stage.slow records (maximum 1146.6 ms in process_rows), and three dashboard.status.slow records (mean 4552 ms, maximum 6031 ms). Other stage maxima: game_queue_panel 329.1 ms, configured_modes 255.2 ms, and process_scan_request 228.4 ms. These are wall-time spans after the CPU/RAM sample, not CPU attribution, and dashboard stdout was not persisted as its own journal. The expensive sub-operation inside the status provider remains unknown. Next, add bounded timings around status-provider, game-DLL progress/queue reads, process scanning and Tk row rendering, then run a matched workload sample after GUI ownership is clear.

## Dashboard CPU attribution events

For each slow dashboard operation, collect both wall duration and the executing Python thread's CPU duration. These are different measurements: wall time can grow while the thread waits or is descheduled, while thread CPU shows work actually executed by the dashboard. Never present thread CPU as whole-host CPU; the separate process sampler supplies normalized host-capacity figures.

The refresh path must expose these bounded stages: status_provider (including time waiting for its control lock), game_dll_progress, game_dll_queue_view, each named Tk refresh stage, and total refresh. Emit dashboard.status.stage when wall duration is at least 50 ms or thread CPU is at least 10 ms; add thread_cpu_ms to dashboard.stage.slow and dashboard.refresh.slow. Keep per-stage log cooldown at two seconds or longer. Every record must retain dashboard PID, UTC time, stage and refresh number. This iteration implements these counters using Python thread_time; it leaves status and redraw cadence unchanged. The running dashboard must be refreshed before these source changes can produce runtime evidence.

To close the measurement loop, persist the dashboard records with the sample ID, process start time, exact dev-loop run/build identity, and the canonical journal or raw artifact path. The current GUI sends dashboard records to stdout, and the live PID 1828 has not loaded the new instrumentation. No CPU attribution or runtime improvement is claimed until a refreshed dashboard emits these fields and a matched workload sample is collected.

## Two-second status-poll sample (2026-11-27 UTC)

After a source edit, dashboard PID 7820 was stopped and an explicit dashboard launch started PID 6996 with the updated module. The code skips status-provider starts until two seconds have passed since the last start; Tk repaint remains at one second. A 60-second warm-up followed by 300 samples ran from 2026-11-27T01:29:35.583Z to 01:34:49.577Z. Host had 12 logical CPUs. The raw artifact is `.dev/resource-measurements/20261127T012806Z-dashboard-two-second-poll-idle.jsonl`; records came from direct Windows process/system APIs, with mean interval 1.051 s, p95 1.176 s and maximum 1.339 s.

Observed dashboard PID 6996 CPU was 0.491% of whole-host capacity on average (5.890% of one logical CPU), p95 1.032%, maximum 1.514%. Private bytes p95/maximum were 49.62/51.43 MB; working set p95/maximum were 32.99/33.85 MB. The earlier visible PID 7820 run averaged 0.817% host CPU with private p95 53.29 MB and working-set p95 75.48 MB. These values are directional only: the runs were not matched, the process IDs and collector implementations differ, and concurrent host workload differs. They do not prove this cadence change caused the drop. The less-than-1% mean target was met in this sample; the p95 is reported without a target.

This was not a game-idle workload. The dev-loop reported no managed run and no game journals, so live mode, map, players/NPCs, FPS and server ticks are unavailable. A DLL build, `game-dll-20261127T013303208483Z-a405dd` for generation 749, started at 01:33:03.208Z and overlapped the last 1m46s; it remained active at the 01:36:22 status read. Host CPU averaged 99.971% (p95 100%). Available physical RAM averaged 2064.48 MB and fell to 653.02 MB; commit charge peaked at 41094.70 MB of a 43064.11 MB limit. Two `mimita.exe` processes, PIDs 5900 and 34092, averaged 6.986% and 14.461% whole-host CPU respectively; their role, launch reason, runtime mode and journal are unknown. The sample cannot attribute overall pressure to the dashboard or build.

The sample did not retain dashboard stdout stage records or a status-age event. Status freshness <=2.5 seconds is therefore unverified; leave this iteration provisional and capture status timestamps or observable GUI state in the next run. If age exceeds 2.5 seconds or queued/active build state is missed, revert the two-second throttle. Do not claim FPS improvement without a matched live game run, mode/workload, journal and dashboard-open/closed measurement.

## Five-second completed-history cache and stressed-host sample (2026-11-27 UTC)

The history-only cache keeps the most recent completed game-DLL summaries for five seconds. It does not cache live `progress.json`, `current.json`, root status, queue state, or dashboard redraws. Direct same-process microprofile with 298 summary files present: the cold `_recent_game_generations()` call took 37.262 ms wall / 31.250 ms thread CPU; seven cache hits had 0.002–0.011 ms wall (median 0.003 ms; thread CPU rounded to 0.000 ms). It returned three usable completed generation summaries. This proves the scan is avoided on a cache hit, but not a whole-dashboard CPU reduction.

A five-minute process sample after this edit ran 2026-11-27T01:55:22.131Z–02:00:37.961Z (300 rows, 12 logical CPUs) and is stored at `.dev/resource-measurements/20261127T015409Z-dashboard-history-cache-idle.jsonl`. Dashboard PID 12196 averaged 2.197% whole-host CPU (p95 4.868%), with private p95 117.83 MB and working-set p95 130.14 MB. Host CPU averaged 99.924%; available physical RAM averaged 809.69 MB and fell to 6.47 MB; commit peaked at 45254.95 MB / 46033.16 MB. This misses the dashboard sample targets, but cannot be compared with the earlier 0.491% run: host pressure was much higher and two unverified `mimita.exe` processes, PIDs 14572 and 25844, averaged 11.361% and 8.251% whole-host CPU. PID14572 private-memory p95 reached 11277.19 MB. Their process roles, command lines, purpose and journals were not captured. The managed run `20261127_020014` began at 02:00:14Z and therefore overlapped only the final 23 seconds; the generation-751 DLL attempt began at 02:00:57Z, after the sample ended. Do not attribute this host or dashboard result to the cache or compiler.

Thresholded dashboard stdout for this sample (`.dev/tmp/dashboard-history-cache.stdout.log`) recorded 26 slow status reads: mean 3175 ms, p95 4282 ms, maximum 11672 ms. Status-stage events had mean duration 1285 ms, p95 3158 ms, max 10027 ms. These events show status work can be very late during severe host pressure; they do not directly measure how old the visible dashboard values were. The 2.5-second freshness target remains unverified and the dashboard must not be described as meeting it under saturation. The next design review should let base run/build status update independently from slow DLL-history and progress detail work.

## Live frame-scope evidence while the dashboard was open

Read-only inspection of run `20261127_014331` found dashboard PID 29964 visible while a one-player `Vehicle Arena - settling fuzz probe` / `vehicle_freeplay` session on `arena2` was running. The client journal `logs/2026-11-27/20261127_014331/client-01/events.jsonl` contains 1272 frame-slice windows from 01:43:44.580Z to 01:48:21.944Z: 33686 frames over 270558.3 measured ms (124.51 FPS when weighted by window duration). Window-average frame time median/p95/max was 7.13/16.22/3031.01 ms; average per-window p95 frame time was 31.88 ms; the worst frame was 4819.60 ms. Mean scope time per recorded call was EngineTick::Render 5.47 ms, Rendering 2.47 ms, Rendering::Shadows 0.636 ms, Rendering::World 0.669 ms, and Setup::ConfigPolling 1.21 ms. Scope totals overlap by design and are not additive. No scope name containing `swap` appeared among the 72 recorded scope names. The journal reports one player and zero NPCs; this is an automated probe, not a representative human gameplay run. No dashboard-closed comparison exists, so neither the long frame spikes nor the user's 60-FPS report can be attributed to the dashboard.

### Commit-limit correction

For sample `20261127T015409Z`, the maximum charge and its same-row limit were 45254.95/46033.16 MB at 01:56:50.778Z (98.31%). The highest charge-to-limit ratio was a different row: 44949.43/44989.67 MB (99.91%) at 01:56:37.307Z. Keep peak charge and peak utilization as separate metrics.

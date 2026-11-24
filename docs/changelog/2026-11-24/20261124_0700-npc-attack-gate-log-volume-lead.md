# NPC attack-gate diagnostic volume lead

## Observation

Read only from the root server journal for run `20261124_053435`, server PID
32184. I parsed the newest 1 MiB of
`logs/2026-11-24/20261124_053435/server-01/events.jsonl`, spanning
2026-11-24 07:00:16.312–07:00:22.870 UTC (6.558 seconds). The slice contained
1,321 structured records, including 95 `npc.attack-gate` events (14.49/s) and
about 201 total events/s, or approximately 160 KB/s. The active journal segment
was 41,325,706 bytes; configured rotation is 100,000,000 bytes per segment.

Events in the same slice show Juggernaut Horde on dust2cyberiav4 with 67 NPCs
and one player. Server tick windows around the slice report 82.26 ms and
43.62 ms average total tick time, above the 16.667 ms fixed-tick budget. This
was not a healthy-tick baseline. The event slice overlaps active build
`game-dll-20261124T065924976860Z-3ed108` (generation584); no CPU sample or
callsite probe was taken, so there is no measured CPU share for logging.

## Source trace and test candidate

`emitAttackGate` in `src/npc/npc.cpp:1446` emits `npc.attack-gate` as Important
at lines1463-1464 and is called at line4278. The category is Important in
`config/debuglogger.json:38`. `StructuredLogger::pollConfig` hot-polls at most
twice per second (`src/debug/structured-log.cpp:1353`); `shouldLog` checks the
category and severity at line1436. Because the structured JSON payload is
constructed at the callsite before `writeEvent` applies that check, turning
the category off would suppress the logger write but still build the payload.
`file_output:false` does not suppress the shared `events.jsonl` stream.

After the current build and runtime owners release the pair, test the existing
`npc_combat` category on/off on the same server/client PIDs, while retaining
performance, tick, and generation evidence. Record per-process CPU and tick
windows, verify attack-gate event output changes, then restore the original
config byte-for-byte. Check other live processes and build-watcher effects
before touching the shared config. If logging has material cost, a source
candidate is to retain one actor in eight at Important and allow the complete
stream when set to Verbose, with JSON construction avoided before the logger
call when disabled. That candidate needs an isolated measured build/runtime
A/B; it is not a proven fix.

## Evidence location

Full event-rate evidence, generation and workload limits, and the CPU/build
issue cross-reference are recorded in
`docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md`.

## Same-slice NPC and tick costs

Parsing the same server journal through 07:00:24.172 UTC showed
`npc.server-phase-window` work counts of 5, 20, 25, 19, and 25 calls in
consecutive emitted windows. Adjacent 67-NPC windows reported 25 and 19 calls
in roughly one second. At 07:00:24.082, `npc.phase-window` reported
`total_ms=342.195`, `decision_ms=52.199`, `physics_ms=185.329`,
`collision_ms=161.021`, `sense_ms=28.244`, `navigator_ms=9.298`, and
`path_ms=12.283`. Collision is included within physics and must not be summed
twice. Nearby `performance.server-tick-window` totals were 114.457, 34.633,
82.260, and 43.620 ms.

These timings overlap the active generation584 build. They are wall-time
diagnostics, not CPU milliseconds, and do not prove clean 60 Hz behavior or
build causality. No callsite probe or process CPU sample was taken. The large
decision/physics scopes are profiling targets for the next matched CPU sample.

NPC sensing is already staggered. In that same 07:00:24.082 window the journal
reports 1,598 updates, 400 sense runs and 1,198 skipped senses (about 25% sense
work). Source sets `staggerNpcThinking=true` at
`src/network/server-npcs.cpp:2344`; `thinkingDue` in line2362 schedules every
fourth tick and forces refresh for missing or invalid target snapshots.
`src/npc/npc.cpp:1747` runs sensing only when due. Further reducing sensing
cadence is not the first optimization candidate without callsite evidence.

## Gen584 build-pressure cross-check

The automatic resource sidecar for build attempt
`game-dll-20261124T065924976860Z-3ed108` sampled 2026-11-24 06:59:40.288–
07:10:45.447 UTC: 212 samples, 12 logical CPUs. Host CPU averaged 99.437%
(peak 100%); memory use averaged 89.56%, with 1,672.94 MiB available on
average and a 27.92 MiB minimum. The 680.469-second attempt compiled 258 of
644 units; phase wall times were 43.156s changed-source preflight, 571.890s
compile, 25.766s link, 3.860s cache scan, and 0.625s publish.

The sidecar's runtime alignment is unavailable, so it does not prove that
these process rows belong to the same live run or workload. PID34760, known
from the concurrent run as a client, averaged 8.13% of whole-host CPU (97.6%
of one logical CPU) across 199 rows. PID32184, known as a server, averaged
4.17% of whole-host CPU (50.1% of one logical CPU) across 67 rows. These
different-coverage rows are not a matched-pair total. Other `mimita.exe`
processes were present with unverified run identities. Compiler/linker-class
processes averaged 14.29% host CPU across samples; other applications
averaged 35.19%, including FL64 at 10.68% and SNAPOS64 at 8.15%.

The sampler itself reports 143.063 seconds of thread CPU over the attempt,
plus 355.822 seconds cumulative process-discovery wall time. This is a
measurement cost to account for in build tests; discovery wall time may
overlap asynchronously and must not be added to the build critical path.
This is evidence of a saturated build window, not proof that MiMITA caused
the build delay. Exact function-level CPU attribution remains unavailable.
The resource and timing artifacts are in `.dev/build-timings/` under the
attempt ID above; the full evidence and caveats are also in the linked CPU
issue.

## Gen585 build-pressure sample on Bloodtest run

For build attempt `game-dll-20261124T072533876573Z-7080a4`, the resource
sidecar sampled 2026-11-24 07:25:38.604–07:30:27.112 UTC (85 samples, 12
logical CPUs). At the final sampled row, the build was compiling with 559 of
645 units completed (65 compiled, 494 skipped). Host CPU averaged 99.93%
(99–100%); RAM use averaged 92.62%, with 1,239,583,033 bytes available on
average and a minimum of 12,816,384 bytes.

The same sidecar tracked the current root Bloodtest run
`20261124_072002`: server PID29932 averaged 6.01% whole-host CPU (72.1% of
one logical CPU), client PID32296 9.29% (111.5% of one logical CPU), and
client PID34840 9.21% (110.5% of one logical CPU). Together they averaged
24.51% whole-host CPU over these 85 rows. The server journal identifies
Bloodtest on mimita-duels-map-v3 with 3 NPCs and 2 players; its nearby
07:30:34.666 phase window reports module generation584. That journal row is
about six seconds after the resource sample ended, so exact in-window frame
compliance is unknown and this is not a matched no-build baseline.

The nearby phase window records 177 NPC updates, collision_ms64.530 within
physics_ms69.714, decision_ms31.127, sense_ms28.278, and navigator_ms25.899.
Do not add collision to physics because it is nested. A nearby tick window
reports 58 ticks and one over-100ms tick; its slowest total tick was145.012ms.
That is a single build-stressed window and does not establish steady 60Hz.
The client journals contain frame-slice events, but every-frame compliance
against 4.167ms was not verified. The sidecar says runtime alignment is
unavailable; process and run context were manually cross-checked, but a
clean matched active-game A/B remains outstanding.

Across the sidecar's rows, MiMITA processes averaged26.35% host CPU,
compiler/linker-classified processes13.62%, and other processes34.28%.
SNAPOS64 averaged8.11% and FL64 averaged7.70%. This is a high-load
concurrent-build observation for Bloodtest, not evidence that game CPU caused
the build duration or a general Juggernaut result. Artifacts:
`.dev/build-timings/game-dll-20261124T072533876573Z-7080a4.resource.jsonl`,
the attempt summary with the same ID, and
`logs/2026-11-24/20261124_072002/{server-01,client-01,client-02}/events.jsonl`.

## Gen585 final sidecar correction

The completed gen585 sidecar has 147 samples through 07:34:11.462 UTC and
reports `runtime_alignment.status=observed` for run `20261124_072002`, server
PID29932 and client PID34840. The selected server/client actual modes differ:
server `bloodtest`, client `repeat`. Treat this as same-run process alignment,
not a same-effective-mode gameplay pair. Full-window host CPU averaged
99.943%; RAM use averaged 91.016%, with 1,509,328,798 bytes available on
average and a 12,816,384-byte minimum.

Across the full sidecar window, server PID29932 averaged 5.963% whole-host
CPU, selected client PID34840 9.478%, and the second run client PID32296
9.54%. Their three-process total is about 24.98% host. MiMITA processes
averaged 26.061% host, compiler/linker 12.357%, and other processes 33.485%.
The previous section's 85-row partial values and statement that automatic
alignment was unavailable are superseded by this completed summary.

No matched pre-build baseline, verified every-frame 4.167ms client result,
no-build comparison, or function-level CPU breakdown is present. The sample
does not establish that MiMITA caused the build duration. See the CPU issue's
final gen585 correction for full process and sampling details.

## Gen585 sampler overhead

The same 147-row sidecar reports 283,534.547ms cumulative process-discovery
wall time (mean 1,907.158ms, median 0, p95 10,916.711ms, max 13,170.22ms).
Runtime-context discovery averaged 326.631ms per row (p95 1,065.976ms,
max 1,561.971ms). Actual sample spacing averaged 3.516s, with p95 13.031s
and max 14.843s despite a configured 1s interval. The sampler thread used
106,968.75ms CPU across the 517.594s capture: 20.67% of one logical CPU or
1.72% of the 12-logical-CPU host on average.

This is measurable profiler load and cadence loss under host pressure; it
may perturb build observations. Cumulative discovery wall time can overlap
asynchronously and is not itself build critical-path duration. A sampler
optimization should consider sampling known MiMITA/build PIDs more cheaply
and refreshing broad process inventory less often, while retaining explicit
compiler/linker and competing-process attribution. The proposed telemetry
change is owned by the build-latency agent; no sampler source was edited here.

## Gen585 mode and code-generation correction

Journal review corrected the sidecar's client `actual_mode=repeat` value. In
run `20261124_072002`, server tick events report Bloodtest, and client PID34840
has `network.mode.start` and `network.mode.active` events for Bloodtest. The
client's unrelated `dev_timer_tips.config_loaded` event also has a generic
`fields.mode=repeat`; the sampler treated that as game-mode evidence. The
server and client were both in Bloodtest, so the earlier mode-mismatch claim
was a false positive. This remains a Bloodtest build-overlap sample, not the
target Juggernaut run.

There is a separate activation mismatch: the server journal reported host
DLL generation585 at 07:34:58 UTC, while the adjacent NPC phase row at
07:34:59 reported `module_generation=584`. This pair cannot prove the
generation585 NPC probe path. No profiler command was sent. The latency agent
is taking the narrow parser/test correction in
`devscripts/dev_loop/build_resource_sampler.py` so only authoritative
game-mode events can populate `actual_mode`.

# Server-only CPU sample and build-adjacent NPC tick evidence

## Purpose

Add one bounded CPU/RAM observation to the existing MiMITA resource issue and
record the limits that prevent treating it as a matched client/server result.
This session did not change gameplay source, start a runtime, or request a
build.

## Evidence captured

- Artifact: `.dev/perf-captures/cpu-server-only-twoserver-20261124T0621Z.csv`
- Sample interval: 2026-11-24T06:21:56.482Z–06:22:27.857Z UTC, 31.375 seconds.
- Host: 12 logical CPUs; CPU mean 97.82%, min 93.89%, max 100%; available RAM
  mean 2,000 MiB, minimum 603 MiB.
- PID 32184: `mimita.exe --server`, root run `20261124_053435`, launch arguments
  Juggernaut Horde / `dust2cyberiav4` / `--npcs 1`. Mean 129.87% of one logical
  CPU = 10.82% whole-host capacity; mean working set 45.4 MiB and private bytes
  326.2 MiB.
- PID 30424: a separate `mimita.exe --server`, launch arguments Bloodtest /
  `slaughterhouse1`. Its exact run and journal were not identified. Mean 42.08%
  of one logical CPU = 3.51% host; mean working set 29.8 MiB and private bytes
  107.2 MiB.
- These two server PIDs together averaged 14.33% of whole-host CPU. No top
  competing-process inventory was captured, so the remaining host CPU is
  unattributed.

## Alignment and limits

The PID 32184 journal had no rows during the exact sample interval. Its nearby
06:14:45Z hotness row reported 67 NPCs, 0 players, and NPC module generation
581; this is adjacent context, not in-window workload proof. No client process
was connected. PID 30424 has no verified run, journal, NPC count, or player
count. Therefore this is process CPU/RAM evidence, not a matched workload
baseline.

The sampler called `Get-Counter` once per nominal one-second loop, but the
actual interval was 31.375 seconds for 16 host queries. Treat cadence as
approximately two seconds, not one second. The dev-loop had a build request
pending, but `game_build.started_utc` was 06:22:29.727Z, about 1.9 seconds after
the sample ended. This interval precedes the build worker; it is not a clean
client/server build comparison.

After the worker started, the same server journal emitted
`npc.server-phase-window` rows at 06:22:57.643Z and 06:22:58.684Z with
`npc_count=67` and `ticks=31`, then `ticks=33`. In
`src/network/server-npcs.cpp:3334`, this counter increments once per
`simulateSharedNpcs` call; lines 3351–3358 emit the count after at least one
second. These rows indicate roughly 30 NPC simulation calls per second during
those in-build windows. They do not directly measure the complete authoritative
server tick loop and do not prove a connected-client workload or cause.

The in-window `npc.phase-window` record at 06:22:57.718Z separately reports
2,013 NPC updates, 269.87 ms in physics stages, 214.41 ms in collision, and
638.08 ms total accumulated phase time; the next window reports 1,995 updates,
293.23 ms physics, 217.07 ms collision, and 596.13 ms total. These are
instrumented wall-time aggregates, not CPU milliseconds. Collision time is
inside physics time and must not be added to it. This is useful to select
follow-up profiling targets, but does not establish that collision is the
largest CPU consumer.

## Interpretation and next action

Host CPU was already near saturation before the game build worker began. The
two observed MiMITA servers account for about 14.33% host capacity; the top
remaining processes and the cause of the host load are unknown. The current
server-only run does not satisfy the requested matched client/server proof.
Keep the Nov 17 physics-timer A/B separate from the unrelated pending batch;
run it only after a clean connected pair and exact workload/generation are
verified. Preserve animation-budget timing, and compare CPU, server tick
telemetry, client frame-time distribution, and gameplay behavior before and
after.

## Changed files

### New file: `docs/changelog/2026-11-24/20261124_0621-server-only-cpu-sample.md`

- Old content: no prior file.
- New content: this evidence record.
- Reason: make the sample, its timestamp/cadence limits, PID identities, and
  follow-up test available for later agents.

## Gen582 build pressure evidence

On 2026-11-24 06:22:29.815–06:29:20.969 UTC, game-DLL attempt
`game-dll-20261124T062229815697Z-8af3a7` published generation582 after
411.154 seconds. Its resource sidecar recorded 300 samples over that span
(about 1.37 seconds per sample despite a nominal 1-second interval), 12 logical
CPUs, mean host CPU 99.779%, peak 100%, mean memory use 87.07%, and mean
pagefile use 34.38 GiB. One sample reports zero available RAM; treat that as a
single-row outlier until raw-row validation. Pagefile use does not prove active
paging I/O.

Tracked process roles averaged 22.76% host CPU for compiler/linker processes,
8.95% for MiMITA, 0.88% for the build orchestrator, 0.64% for the build worker,
and 24.62% for other tracked processes; the remainder was not attributed.
Sampler thread CPU was 6.4375 seconds over the 411.154-second build, about
1.57% of one core or 0.13% of whole-host capacity. Process discovery cost
10.568 seconds and runtime-context discovery 14.733 seconds of wall time.

The server journal showed actual Juggernaut Horde on dust2cyberiav4 with 67
NPCs during the build, though the launcher snapshot reported one NPC. No client
was connected, so this does not provide paired client/server or 240-FPS proof.
NPC phase timings are wall-time scopes, not CPU attribution; the sample shows
host saturation and memory pressure during a build, but does not prove MiMITA
caused all host load or caused the build duration. Full evidence and the
fanout/build-stage breakdown are in CPU issue `issue-ed3ac775283d` and linked
DLL latency issue `issue-136d4553ff92`.

### Addendum to changed-file record

- Updated this changelog with the gen582 automatic build resource sample and
  its limitations. The original server-only sample record remains unchanged.


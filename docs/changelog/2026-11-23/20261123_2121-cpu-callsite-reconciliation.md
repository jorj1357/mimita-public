# CPU callsite evidence reconciliation

- Session: `work-af9e18941e29`
- Date: 2026-11-23
- Scope: read-only reconciliation of the existing matched Juggernaut process sample, in-game callsite probe, and frame/tick journals; no source, runtime, or build changes.

## Evidence

Run `20261123_195307`, server PID `22184`, client PID `32832`, Juggernaut Horde on `dust2cyberiav4`, 67 NPCs / 1 player, no compiler/build. The matched host/process sample is `.dev/perf-captures/jugger-horde-pair-20261123T1957Z-cpu.csv` (2026-11-23 19:58:30.201–19:58:59.856 UTC, 12 logical CPUs). Host CPU averaged 99.36%; server/client averaged 9.55% / 17.76% of whole-host capacity. The bounded probe was active 19:58:46.756–19:59:01.946 UTC, overlapping that process sample for about 12 seconds.

Parsed 15 server `performance.cpu-callsite-window` rows from `logs/2026-11-23/20261123_195307/server-01/events-0001.jsonl`:

- Server process CPU averaged 7.57% of host capacity during those windows.
- `updateOneNpc`, marker at `src/npc/npc.cpp:1579`, estimated mean 36.16% (median 33.04%) of server process sampled cycles; mean estimate about 330 process CPU-ms per one-second window, or 2.76% whole-host capacity.
- `doCollisions`, marker at `src/physics/movement/physics-collision-dispatch.cpp:181`, estimated mean 8.61% of process cycles; about 77 process CPU-ms per window, or 0.65% whole-host capacity.

Parsed 14 client windows from `logs/2026-11-23/20261123_195307/client-01/events-0002.jsonl`. Client process CPU averaged 14.26% whole-host capacity. `engineTickRenderFrame` at `src/engine/engine-tick.cpp:242` estimated 15.63% of client process cycles on average (about 2.24% whole-host). Simulation markers were sparse and are not ranked.

The 29.655-second client frame sample had all 1,862/1,862 frames above the 4.167 ms budget. The server journal had 23 tick windows over the matching 25-second analysis interval: mean total tick time 47.18 ms, mean p95 97.2 ms, mean NPC stage 44.04 ms. This misses both performance targets.

## Limits and next step

Probe values are 1/64 sampled thread-cycle estimates, not exact function CPU measurements. The probe scopes may overlap; marker lines identify where instrumentation begins, not the internal instruction hotspot. Do not add percentages across scopes or call this a before/after optimization result. The later native sample at 21:03 is separate and must not be combined with this interval.

Generation proof remains inconsistent: server hotness reports game DLL generation 571 while NPC phase events report module generation 570. Before interpreting another NPC profile or claiming a source change is live, require game DLL and NPC module generation equality in the same run. Next useful profile, after source-owner readiness, is bounded non-overlapping thread-cycle attribution inside NPC physics/collision and decision/path phases. Then choose one measured owner and repeat the same workload for before/after, including per-frame 240 FPS and 60 Hz tick acceptance.

The full evidence handoff was appended to CPU issue `issue-ed3ac775283d` and sent to the DLL-latency owner. No build or runtime capture was started by this session.

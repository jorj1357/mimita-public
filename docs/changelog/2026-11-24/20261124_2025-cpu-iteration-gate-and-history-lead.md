# CPU iteration gate and historical lead

**Date:** 2026-11-24 UTC  
**Issue:** `issue-ed3ac775283d`  
**Change type:** Investigation and experiment planning; no game source changed.

## User direction

The active goal is to reduce MiMITA CPU/resource use through repeated code
changes and live, evidence-backed tests. For each game-DLL build requested by
this CPU effort, use one named CPU-focused code change and compare matched
before/after resource and runtime evidence. Do not count unrelated builds as
optimization experiments.

## Current gate

At 2026-11-24 20:23:53Z, read-only process inventory found MiMITA server PIDs
4372, 13680, and 16440, and client PIDs 35812 and 28996. Their command lines
associate them with run folders `20261124_193611`, `20261124_195553`, and
`20261124_202224`. Root `dev-loop --status` reported `run=(none)` and a stale
published state, so it cannot establish which pair is currently owned or
whether all journals describe the same live workload. Recent journal files for
the latter two runs were still being written around 20:23Z. No process was
sampled, commanded, stopped, or restarted in this investigation.

The latest root build attempt observed during this investigation,
`game-dll-20261124T202420131204Z-8cc10c`, changed
`src/entities/player-render.cpp`; it was not a CPU-focused change requested by
this effort. Its resource alignment was `mode_unverified`, and the brain still
flags missing matched pre-build baseline, host commit-charge/limit, and
independent client-mode confirmation. Treat it as unrelated build pressure,
not a CPU optimization A/B.

## Historical hypothesis

Commit `45580410a52f31bb02f702f35f7dcc597b60bb4b` on Nov 22 explicitly says
the game was showing low FPS and 30–50% CPU. It also changed NPC tuning:

- `config/npc-difficulty.json`: `perceptionSightRange` 99 to 1600,
  `turnSpeed` 270 to 1080, and `aimAtTargetMax` 10 to 0.
- `config/behavior-profiles.json`: `max_attackers_per_target` 8 to 32.
- `config/gamemodes/jugger_horde.json`: targeting changed from
  `opposite_team` to `player`.

Commit `186eebd23ce5f8f3f10377c47277ed58bd3b4d83` on Nov 23 is described as
making Juggernaut Horde NPCs much more overwhelming; it raised
`downDashChance` from 0.25 to 1.0. These changes are plausible workload
amplifiers, but commit timing and configuration values do not prove they caused
the current CPU usage. NPC count, target count, navigation, rendering, and
other host processes must be held constant to test them.

## Next experiment contract

Before a source edit or build, obtain an owner-approved, registered pair with
fresh server and client journals, verified actual mode, actor counts, PIDs,
and host/NPC module generation. Capture a matched baseline. Then select one
measured code owner and change only that owner; record the hypothesis and the
predicted metric change before building through root dev-loop. After exact
generation activation, repeat the same workload and compare whole-host and
per-process CPU, memory, client frame-time distribution, server tick health,
and behavior evidence. Keep profiler/callsite estimates separate from OS CPU
measurements and preserve uncertainty where symbol or workload alignment is
missing. Revert a change if the measured owner does not improve or behavior
regresses.

No matched runtime pair was established in this investigation, so no source
change or build was requested and no CPU improvement is claimed.

## Concurrent-build sample discovered during the investigation

The automatic sidecar for attempt
`game-dll-20261124T202420131204Z-8cc10c` provides a useful process-level
pressure snapshot, but not a code A/B. The 128.632-second build window
(20:24:20.135–20:26:28.767Z) sampled 12 logical CPUs 81 times: host CPU
averaged 99.99%, peaked at 100%, and available RAM fell to 16,961,536 bytes.
The declared changed path was `src/entities/player-render.cpp`, not a CPU
optimization. Sidecar and journals aligned the server PID 16440 to Bloodtest,
1 NPC/1 player, but the client mode was stale/unreported. Its near-end client
frame window showed 50.54 FPS, all 51 frames over 4.167 ms, p95 61.49 ms and
max 82.23 ms.

Across the sidecar's available process snapshots, seven `mimita.exe` processes
averaged about 45.57% of whole-host CPU combined: client 35812 10.93%, server
4372 9.94%, client 28996 6.71%, server 13680 5.77%, server 16440 3.63%, and
two unclassified MiMITA processes 17660 4.81% and 25848 3.78%. The known
Bloodtest pair's measured contribution was about 10.34% host, with client
mode unverified. Compiler `cc1plus` averaged 1.72% host in its listed samples;
other notable listed processes included ChatGPT (4.92% and 4.65%), cpptools
(4.65%), and pythonw (3.10%). The seven-process sum is evidence that multiple
MiMITA processes contributed materially during this saturated interval; it
does not identify a hot function or prove why the host was saturated. There is
no matched no-build baseline, so this remains pressure evidence only.

Artifacts: `.dev/build-timings/game-dll-20261124T202420131204Z-8cc10c.resource.jsonl`
and `.dev/build-timings/game-dll-20261124T202420131204Z-8cc10c.resource.json`.
Exact process and workload caveats were appended to CPU issue
`issue-ed3ac775283d`.

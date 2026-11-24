# Gen570 CPU profile and header fanout plan

- Session ID: work-9b45b8857814
- Owner: resource-efficiency-goal
- Date: 2026-11-23
- Status: partial; runtime performance regressed and no optimization has yet been applied

## Scope and coordination

Reviewed the matched Juggernaut Horde CPU/callsite sample, aligned authoritative server tick windows, corrected the shared CPU issue, notified the Mode 3 latency owner, and appended a staged header fanout plan to `issue-196c3045b4cb`. No game source, build-driver, or profiler source was edited. The bounded probe ended and the latency owner was told its build window was released. Existing run processes remain under their owner.

## Runtime evidence

Run `20261123_195307`, game DLL generation 570, server PID 22184 and client PID 32832. Server journal verified Juggernaut Horde, `dust2cyberiav4`, 67 NPCs and 1 player; both processes use the pinned generation 570 artifact. Paired resource CSV `.dev/perf-captures/jugger-horde-pair-20261123T1957Z-cpu.csv` covers 2026-11-23 19:58:30.201–19:58:59.856 UTC (29.655s, 52 rows, 12 logical CPUs): host CPU mean 99.36%; available RAM mean 629 MiB/minimum 502 MiB; server 114.6% of one core = 9.55% host; client 213.14% = 17.76% host; pair 27.31% host. Process snapshot overhead averaged 4.50% of one core (~0.375% host). No compiler/build process was observed. Major other host CPU consumers included RobloxPlayerBeta PID 33908 at 24.27%, SnapOS64 PID 7384 at 9.38%, Xeno PID 31428 at 5.96%, pythonw PID 16864 at 5.08%, and OpenCode PID 26288 at 3.39% of host.

Aligned `performance.server-tick-window` records in `logs/2026-11-23/20261123_195307/server-01/events-0001.jsonl` span 19:58:30.110–19:59:02.530 UTC: 30 windows and 800 tick-index increments over 32.420s = about 24.68 Hz, below the required 60 Hz. Mean reported total tick average was 43.09ms, mean NPC stage average 40.19ms, and the maximum reported tail value was 175.53ms.

The opt-in 1/64 callsite probe estimated `updateOneNpc` at 22.9–42.5% of server-thread CPU in observed windows (final window 41.51%, about 298.39 CPU-ms) and `doCollisions` at 6.5–12.3% (final 12.30%, about 88.40 CPU-ms). These broad scope estimates may overlap and do not identify the exact instruction-level hotspot inside either function. Probe source lines are instrumentation markers. The earlier elevated xperf capture had no sampled profile events, so it supplied no symbol-level attribution. Client probe sample counts were too low for a ranking; client frame telemetry reported every sampled frame above 4.167ms, so 240 FPS was not met in this workload.

## Header fanout plan recorded

The new user report is linked and acknowledged as `ctx-81d51e316de5`, primary issue `issue-196c3045b4cb`. Existing issue evidence reports a historical dependency fan-in ranking including `pch.h` 456; `structured-log.h` 355; `physics-types.h` 313; `movement-types.h` 293; `weapon-types.h` 287; `config.h` 284; `avatar-rig.h` 281; `debug-log.h` 276; and `player.h` 243. Build-derived invalidation samples include generation 569: `camera.h` 143, `gamemode.h` 32, and `post-fx.h` 14 invalidations; generation 564: `npc.h` 133 affected TUs, with 68 transitive-only. These are historical sample counts, not a current full rank.

The issue plan is: (1) rank exact root compiler dependency graph fanout separately as direct/transitive, then attach invalidation, per-TU time and critical-path impact; (2) prioritize behavior-neutral dependency-edge reductions using forward declarations, `.cpp`-local includes, cohesive owner/detail headers, and stable narrow interfaces; (3) pilot one measured seam, retaining umbrella headers only where compatibility requires them and preserving live DLL ABI/layout; (4) require agents to explain broad-header ownership, avoid convenience/transitive includes, report fanout before editing, queue build notes and use the root dev-loop; (5) compare affected-TU counts, build critical-path and runtime correctness before and after. Do not split by line count alone or assume every header should be split.

## Follow-up header inventory

Read-only `dependency_graph.py --top 400` on the current `.d` set (699 sources,
612 headers, 31 stale) found 73 project headers with >=100 direct dependents and
31 more at 50-99. The >=200 tier is 19 headers led by `pch.h` 513,
`structured-log.h` 355, `physics-types.h` 313, `movement-types.h` 293, and
`weapon-types.h`/`vip-appearance.h` 287. Counts move while concurrent builds
rewrite `.d` files (`pch.h` measured 504 and 513 minutes apart), so item 1 must be
a generated revision-stamped artifact, not a hand-copied table.

Enforcement gap: `dependency_graph.py --gate` currently exits 1 with 70 `growth`
violations above `config/header-fanout-allowlist.json`, but it runs only in docs
and `test_dependency_graph_gate.py` - not in a build, quality, or agent commit
path. `header-hygiene.md` still calls the checker "planned", and two overlapping
owner docs remain (`headers/header-hygiene.md` and
`live-development/incremental-build-and-header-fanout.md`). Appended this
evidence to `issue-196c3045b4cb`; no source or shared doc edited.

## Limits and next evidence

The CPU sample occurred with no build, so it does not establish CPU-pressure causality for DLL build durations. The server missed 60 Hz and client missed 240 FPS in this sampled workload; no optimization has yet been tested. Next CPU-side step is a child-scope breakdown under a matched workload, then one isolated optimization and a same-PID before/after test with tick/FPS gates. The latency owner should produce the exact full dependency-graph inventory and select an owner-approved low-risk header pilot. Keep source, build, runtime and human acceptance claims separate.

# Completed continuous resource sample comparison

## Goal and outcome
Continued the long-running MiMITA resource measurement/optimization goal without changing game or profiler code. Analyzed the stopped 594-row PID/host sampler across a matched pre-build, build, and post-build window; appended the detailed evidence and caveats to CPU issue `issue-ed3ac775283d`.

## Evidence
- Run `20261123_101135`: Juggernaut Horde on `dust2cyberiav4`; actual runtime journal confirms 67 NPCs and one player. PIDs: server11256, client22492; host has 12 logical CPUs.
- Completed sampler: `.dev/perf-captures/mode3-live-15file-20261123-03.csv`, 10:18:57.242–10:28:52.334Z.
- Whole-host CPU / pair CPU: pre-build 54.55% / 29.68%; 17-sample build window 79.60% / 29.64%; 393-sample post-build window 81.81% / 57.51%. The later pair increase is real in this sample, but workload equivalence and its cause are not established.
- Pair CPU is normalized to total 12-core host capacity; issue record also includes each process as percent of one logical core and whole-host percent. RAM available averaged 6.15 GiB before build, 5.77 GiB during build, and 4.96 GiB after.
- Build JSON `.dev/build-timings/game-dll-20261123T102200810724Z-dcef23.json` reports generation555 and 17.612s request-to-completion for 15 changed marker sources, 16 compiled units, 627 skipped. Build process IDs and competing process attribution are missing from the sampler.
- No source edits, build request, WPR capture, or runtime restart performed by this resource-analysis session. Existing live pair was preserved.

## Interpretation and next step
Build window pair CPU remained ~29.6% host, while the subsequent runtime window averaged 57.5%; this does not implicate compilation as the source of that later rise. It requires a repeat with per-window NPC/actor activity, FPS/frame time, server tick percentiles, player activity, and compiler/linker PID plus top-process attribution. Exact function CPU attribution remains unresolved; use symbolized ETL or bounded per-thread CPU-time profiling before optimizing a specific function. See CPU issue for detail and evidence limits.

## Files and records changed
- Issue `issue-ed3ac775283d`: appended a full-window comparison, build timing correlation, normalization, limits, and next-capture requirements. No source code changed.
- This changelog: new file; no prior text existed.


## Later live sample
A separate bounded PID-only sample of the same still-running pair ran 10:36:49.827–10:37:09.828Z (21 samples/PID). Server averaged 317.8% of one logical CPU = 26.48% whole-host; client 385.8% = 32.15% whole-host; pair 58.63%. Host CPU averaged 90.4%, max95.7%. System memory load averaged 96.6%, available physical RAM0.47GiB; server/client working sets averaged5.54/0.65GiB. Nearby journal evidence ~30s later reports 67 NPCs, one player, average89.89FPS over five windows with 453/453 frames above4.167ms, and a nearby 70-sample server tick window NPC average13.32ms/max38.17ms, worst total tick40.983ms. These gameplay metrics are adjacent, not exactly aligned to the PID capture. The result and caveats were appended to CPU issue `issue-ed3ac775283d` and cross-threaded to the Mode3 build collaborator in issue `issue-136d4553ff92`.


## Source path found for the largest measured server tick stage
A runtime `performance.server-tick-window` at10:37:41.434Z recorded 67 NPCs, one player, 70 ticks, NPC stage mean13.3215ms/tick and max38.169ms; adjacent `npc.server-phase-window` at10:37:41.636Z recorded 68 ticks and 654.14ms of aggregate `ai_ms` out of701.17ms total, while target selection consumed4.89ms. The source maps to `simulateSharedNpcs` in `src/network/server-npcs.cpp:2048`; the measured AI interval is lines2270–2927 and per-NPC `npcSystem.updateOneWithTarget` is lines2723–2730. Its per-call elapsed total is not currently exported in the one-second phase event, and neither event reports thread CPU time. This narrows the next profiling owner but does not identify a specific expensive nested function or its percent of process CPU. Detailed mapping and instrumentation caveats were appended to CPU issue `issue-ed3ac775283d`.

# CPU profile history and next A/B

Session: `cpu-resource-followup`  
Issue: `issue-ed3ac775283d`  
Run: `20261125_061355`, pinned game DLL generation 640, server PID 17988, client PID 13872.

## Evidence reviewed

The owner-confirmed matched profile spans 2026-11-25 06:33:57–06:34:13 UTC. The workload was Juggernaut Horde on `dust2cyberiav4`, 67 NPCs and one player, with host and NPC module generation 640 aligned. The 12-logical-CPU host averaged 69.13% CPU with 4.64 GiB minimum free RAM. The pair averaged 31.53% whole-host CPU: server 12.00%, client 19.53%.

The bounded 1/64 callsite estimates attributed 26.60% of server process CPU to `NpcSystem::updateOneNpc` (`src/npc/npc.cpp:1580`) and 11.28% to `doCollisions` (`src/physics/movement/physics-collision-dispatch.cpp:181`). The client estimates attributed 20.93% to `engineTickRenderFrame` (`src/engine/engine-tick-render.cpp:243`), 17.96% and 10.20% to two `engineTickSimulate` sites (`src/engine/engine-tick.cpp:113,223`). These are sampled cycle-share estimates, not exact CPU milliseconds; probe overhead and competing-process shares were not measured.

Client performance was 59.51 FPS; all 1,082 sampled frames exceeded the 4.167 ms target. The server produced 17 tick windows covering 1,042 ticks; mean reported per-window average tick time was 14.57 ms. This does not prove every tick stayed within 16.667 ms. Full evidence and raw paths are in the CPU issue and `.dev/perf-captures/jh-gen640-profile-matched-20261125T0632Z.csv`.

WPR `CPU.light` could not start because Windows denied the profiling privilege (`0xd0000061`, policy `0xc5585011`); no ETL or kernel-stack attribution was produced. The existing bounded in-game callsite probe did produce the profile above.

## Git history check

- Commit `45580410` (2026-11-22 19:25:49 -05:00) added 118 lines of FCS movement-metric collection and reporting inside `updateOneNpc`. The active code is gated to `fun_cs_rush` and `fun_cs_rush_easy`, so it was not active in this Juggernaut sample. The commit message already reported a 30–50% CPU concern; that timing association does not show the commit caused it.
- Commit `5e08e105` (2026-11-23 14:15:45 -05:00) added the 1/64 NPC CPU probe and elite objective-travel bhop logic in `updateOneNpc`. The matched profile includes the probe, so its overhead is unknown. No matched pre/post profile isolates either addition.
- A prior debug-category A/B reduced emitted log bytes by about 91% without a CPU improvement. Generic logging-volume reduction is therefore not the next treatment.

These are investigation boundaries, not proven regressions. The historical baseline needed to identify when CPU rose is absent.

## Next experiment candidates

1. First candidate: reduce measured work in the NPC collision path. It accounts for 11.28% of server process CPU and the same run's NPC phase telemetry reported roughly 170 ms/s in collision scopes. Before editing, identify the dominant inner collision stage; preserve the cached broadphase, zero-allocation hot-loop rule, collision results, and fixed 60 Hz simulation. Falsify the optimization if matched collision CPU share or server tick tails do not improve, or gameplay collision behavior regresses.
2. Second candidate: isolate the non-collision work inside `updateOneNpc`, which accounts for 26.60% of server process CPU. Use existing decision/physics/navigation scopes to select one sub-owner; do not make a broad rewrite based on the function marker alone.
3. Separate client candidate: the render/simulation markers are substantial while 240 FPS is missed, but GPU time and per-subphase detail are absent. First distinguish CPU-bound from GPU-bound frame time, then select one client treatment.

Each optimization build must contain one named, measured CPU treatment and use matched pre/post runtime evidence with the exact executable/DLL, workload, host/process counters, callsite profile, client frame distribution, server tick distribution, and gameplay checks. The existing coordination/self-improvement specifications already require this; no duplicate rule was added.

## Outcome and blockers

No source or build input was changed in this session. The root dev-loop reports `lifecycle=starting` and `published_stale`, and its shared status does not register this isolated run. The run owner `root-mode3-latency` confirmed the pair and profile but is inactive; the NPC/collision path owner handoff has not arrived. Bounded ownership questions were sent through agent coordination and no answer was available at session close. I did not send in-game commands, restart or stop the pair, or request a build.

The measured baseline is ready. The next action is to obtain an explicit file/build handoff, restore a trustworthy root build-control state without disturbing the pinned run, then seal one CPU-focused change and compare the same workload. The previous logging A/B and this profile should not be counted as a successful CPU optimization.

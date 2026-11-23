# MiMITA CPU phase evidence and source mapping

- Agent: `resource-efficiency-goal`
- Issue: `issue-ed3ac775283d`
- Run: `20261123_055730_01`
- Live processes: server PID 8804, client PID 10148; generation 546; mode `jugger_horde`; 67 server NPCs, one player; 12 logical CPUs.

## Findings

The generation-matched server journal reports sustained tick overruns with 67 NPCs. Around 08:00:55–08:00:57Z, 25–30 tick windows had NPC-stage averages of 38.04–43.67 ms and maxima of 88.19–127.93 ms. Tick 417411 took 130.038 ms total, with 127.925 ms attributed to NPC updates. One `npc.phase-window` reported 1340 updates and elapsed totals of 661.61 ms overall, 135.96 ms collision, 174.68 ms physics, and 103.28 ms decision. The physics total includes the `physicsMainUpdate` span and overlaps the collision substage; those values must not be added.

The paired 30-second process sample at 07:41:36.125–07:42:06.165Z measured 57.23% whole-host CPU for the client/server pair (client 34.32%, server 22.91%) on 12 logical CPUs. Memory averages were 757.1 MB client private bytes and 626.6 MB server private bytes; server working set peaked at 3000.5 MB. The build agent corrected the timeline: generation 548 completed at 07:41:04.320Z and became ready at 07:41:04.793Z. This resource sample is **post-build runtime load**, not concurrent compilation. A separate 10-minute sample showed high CPU before the build as well.

Rate-limited collision substage events identify concrete long stages: sweep/slide reached 20.266 ms, depenetration solve 14.457 ms, and floor recovery 10.482 ms in individual NPC samples. One floor-recovery outlier had only two candidates and zero recovery contacts, which merits investigation. These timers are elapsed durations, not per-function CPU percentages.

The CPU.light ETL still does not provide usable call stacks or symbolized function attribution. Do not claim exact function CPU shares from the current scopes. Generation-matched source owners are `NpcSystem::updateOneNpc` at `src/npc/npc.cpp:1545`, the physics/collision timing around `src/physics/physics-mini.cpp:397–404`, and the rate-limited collision substage logger in `src/physics/movement/physics-collision-glb-main.cpp:128–176`; the event journal records runtime lines 4203, 524/1650, and 177 respectively. Immutable generation-546 sources are under `.dev/build-snapshots/game-dll-20261122T235222465818Z-9bfbf7/source`.

Git history check: runtime generation 546 predates commit `45580410` (2026-11-23 00:25:49Z); that commit cannot explain these measurements. Its added movement metrics are gated to `fun_cs_rush` / `fun_cs_rush_easy`, while this journal records `juggernaut_swarm`. Earlier commit `70f3c051` changed objective-navigation fallback/recovery, but whether those branches run for this workload remains unverified. No regression is attributed to either commit without a matched before/after capture.

## Next steps

1. Export CPU Usage (Sampled) instruction pointers from WPA or collect a bounded stack-enabled trace with a verified symbolization path for the exact generation.
2. Inspect floor-recovery and sweep/slide outliers in the immutable runtime source; preserve the existing collision owner and avoid source changes before function-level CPU evidence.
3. Capture matched build-free idle and 67-NPC active windows. Report true idle CPU targets separately from active-play performance; the current active workload does not meet the 240 FPS client or 60 Hz server targets.
4. Build-latency work is owned by `mode3-live-dll-test`; the CPU sample window is clear for its next controlled build.

No source files were changed, no build was requested, and the live game processes were left running.

## Later live memory point

At 2026-11-23T08:08:44Z, while the same run and 67-NPC mode were alive and the dev-loop reported no active build, Get-Process showed server PID 8804 with 7,670,841,344 bytes private, 5,385,564,160 bytes working set, and 5,956,001,792 bytes peak working set. Client PID 10148 had 1,110,451,176 bytes private and 973,967,360 bytes working set. The host had 1,105,636 KB available out of 16,409,100 KB visible. These are point measurements, not an instantaneous CPU reading or a confirmed memory leak. They differ substantially from the earlier 07:41 sample; capture a time series with method, workload, and build phase aligned before concluding that memory grew. The CPU issue now contains the exact values and next measurement request.

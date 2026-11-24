# MiMITA CPU loop — generation 604 pressure only

## User direction

> like the whole pc immediately gets faster as soon as u close all the mmita processes like idk what happend or changed but can u make that ur goal to change code and test new changes each time to see like how to get lowest cpu and resource usage possible, continue measuring and stuff but prove it thru code changes and stuff with events.josnl and cpu tracking and stuff

Continue the active resource goal as a sequence of controlled CPU-focused code experiments. Do not count unrelated builds as optimization passes. Compare the same workload and runtime identity before/after each change; preserve the 240 FPS per-frame budget, 60 Hz server behavior, and gameplay correctness.

## Evidence this session

Generation 604 attempt `game-dll-20261124T191758400832Z-72e4b6` completed successfully at 2026-11-24 19:20:15.291Z (136.849s, exit 0). It belonged to unrelated feature work; no MiMITA processes or matched live runtime were present, and no CPU-focused runtime change was tested.

Resource artifact: `.dev/build-timings/game-dll-20261124T191758400832Z-72e4b6.resource.json` and its JSONL sidecar. It sampled 118 times across 12 logical CPUs. Host CPU averaged 89.724%, peaked at 100%, and available RAM fell to 30,789,632 bytes (29.4 MiB). During 90 compile-phase rows, host CPU averaged 97.78%, with the same minimum available memory. Sampled compiler `cc1plus.exe` averaged 5.11% of whole-host capacity per process observation (537 observations, max 7.88%). Other observed work included `pythonw.exe` role=other at 6.46% mean, `cpptools.exe` at 4.91%, build orchestrator `pythonw.exe` at 0.91%, and build worker `pythonw.exe` at 0.67%. The resource sampler used 1.281s thread CPU during 136.9s and spent about 2.010s total in process/runtime-context discovery.

The sidecar has no matched runtime alignment, no paired journal, and no frame/tick evidence. This establishes severe machine/build pressure while MiMITA was absent; it does not establish how much closing MiMITA contributed to the user's perceived improvement. A prior no-MiMITA control also found host CPU still averaging 43.76%, but it was not paired in time with a MiMITA-on sample and cannot measure the difference.

At final inspection the dev-loop build was terminal, no MiMITA processes were alive, and free physical memory was 1.86 GiB. Do not launch another pair at this headroom.

## Candidate to measure, not yet proven

The NPC server calls `NpcDifficultyConfig::pollReload()` every simulation tick at `src/network/server-npcs.cpp:2103`. The function at `src/npc/npc-difficulty-config.cpp:312-328` may call `getLastWrite()` for the movement preset and the main NPC difficulty file. At 60 ticks/second this is up to 120 file timestamp checks per server per second. The existing `npcDifficultyConfigPollMs` measure is elapsed wall time; it is not CPU-time attribution, and no evidence yet shows this is a meaningful CPU cost.

## Next pass contract

1. Wait for enough machine headroom and a source-owner/build window; establish a fresh connected pair with journal-proven mode, map, NPC/player count, same host DLL and NPC module generation, process IDs, and build identity.
2. Capture a bounded pre-change host and known-PID sample plus existing bounded callsite/per-scope evidence. Record client frame-time distribution and server tick-rate/tick-time in the same interval.
3. Select exactly one owner-level CPU hypothesis from that baseline. For the config poll candidate, include CPU-time/call-count evidence; do not infer cost from wall time alone.
4. Make one isolated code change with a falsifiable prediction, queue the required build note before saving, build only through root dev-loop, then capture the same workload and process roles after exact generation activation.
5. Keep the change only if normalized CPU use improves beyond measurement noise and frame/tick/gameplay requirements remain satisfied. If it fails or is neutral, record that result and revert or select the next measured owner.
6. Record exact build/run/journal/artifact paths, UTC windows, whole-host and per-process CPU, memory, sampler overhead, FPS/frame-time, ticks, workload, and uncertainty in the CPU issue and build issue as relevant.

No runtime optimization was changed or tested in this session.

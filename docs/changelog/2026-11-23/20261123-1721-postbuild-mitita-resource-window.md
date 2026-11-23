# Post-build MiMITA resource window

**Date:** 2026-11-23 UTC
**Agent:** `resource-efficiency-goal`
**Issue:** `issue-ed3ac775283d`
**Related build issue:** `issue-136d4553ff92`

## Measurement

Captured `.dev/perf-captures/cpu-live-postbuild-20261123T171950Z.csv` after generation566 completed. The 15 one-second rows span 17:19:51.498Z–17:20:05.515Z (14.017 seconds from first to last row); recorder wall time was15.047 seconds. No compiler/build process overlapped the sample. The PC reported12 logical CPUs, host CPU at100% in every row, mean physical-memory load95.87%, mean available RAM591.9 MiB, minimum410.8 MiB.

Process CPU was computed from `GetProcessTimes` deltas. The first percentage is per logical CPU; the second divides by12 for whole-host capacity.

| Process | PID | CPU, one core | CPU, whole host | Mean working set | Mean private bytes |
|---|---:|---:|---:|---:|---:|
| Server | 28600 | 121.81% | 10.15% | 177.2 MiB | 1417.8 MiB |
| Client 01 | 13404 | 194.31% | 16.19% | 304.8 MiB | 1079.4 MiB |
| Client 02 | 30924 | 196.02% | 16.34% | 282.4 MiB | 1205.2 MiB |

Server plus client01 used26.34% of host capacity. The server and both clients used42.68% (about5.12 logical-core equivalents). Their combined working set was764.5 MiB; private bytes totaled3702.4 MiB. A server plus one client working-set pair was about482 MiB, while that pair's private bytes totaled2497 MiB. This is not a verified <=500 MiB success: the memory metric and one-client workload must be defined, and the full live run had a second client.

## Runtime alignment

The same run's server journal reports host DLL generation566, 8 NPCs, and2 players at17:20:05.271Z. The live server was PID28600, clients13404 (`NPC Dev`) and30924 (`LateJoin`). Launch arguments specify team_deathmatch on `atdm`; an in-window mode-change event was not found, so active in-game mode is not independently confirmed. The NPC phase event at17:20:04.751Z reports `module_generation=565`, one behind host generation566.

The sample overlaps a server stall event at17:20:05.423Z:100.065 ms total, of which89.152 ms was NPC work. An adjacent one-second phase-budget record reports max NPC work136.494 ms against a10 ms NPC budget, with16 over-budget ticks. The NPC phase window reports359 updates and323.877 ms total; collision215.647 ms and physics227.757 ms are overlapping scopes, not additive CPU time. Decision took58.838 ms, navigation31.970 ms, and the worst actor window was26.589 ms. These are elapsed phase timings, not CPU milliseconds by function.

Client frame records inside the sample report30.25 FPS (32/32 frames over4.167 ms) and22.38 FPS (23/23 over budget). Both records also report `rendering_ms=0` and `max_npcs=0`, so this is not a visual acceptance result. No CPU callsite-probe event was present.

## Interpretation and gaps

Direct observation: three MiMITA processes used42.68% of host CPU while the host was saturated and memory load averaged95.87%. They were a large contributor in this active 8-NPC, 2-player run. The remaining57.32% host CPU was not attributed. This was a post-build sample and does not show that MiMITA caused generation566's 243-second build. The build owner reported no resource sample during that build.

Unknown in this window: top competing process CPU, GPU utilization/memory, disk and network I/O, thread-level CPU, and CPU milliseconds by source function. The exact active in-game mode is not confirmed by an event. The process sample does not test the <=1% idle/background target. The high resource load and frame/tick overruns justify profiling the actual loaded NPC module and splitting its collision, physics, and decision paths into bounded CPU callsite samples, after resolving the generation565/566 module mismatch and coordinating with the gameplay owner.

## Capture correction

The first one-off sampler attempt aborted before collecting rows because the script converted an already-converted `GetSystemTimes` value twice. No measurement was taken in that attempt. The corrected sampler produced the CSV above; no game process or command was changed.

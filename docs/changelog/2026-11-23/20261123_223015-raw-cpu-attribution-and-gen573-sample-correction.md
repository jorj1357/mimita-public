# Raw CPU attribution audit and generation 573 sample correction

- Session ID: work-482ad842f0aa
- Owner: `resource-profiler`
- Date: 2026-11-23
- Scope: Re-parse the bounded runtime callsite journal; reconcile generation 573 resource-sample classification; coordinate next CPU probe with the DLL-latency and NPC owners.
- Source changes: none.
- Builds, runtime restarts, and profiling captures: none in this session.

## Verified raw callsite windows

The `performance.cpu-callsite-window` rows were parsed as raw JSON from the server and client journals, avoiding filename/line prefixes. Each process emitted 15 windows during the 21:37:51–21:38:06 UTC bounded probe in run `20261123_195307`.

| Process/site | File:line | Mean share of process CPU | Estimated whole-host CPU | Estimated CPU time per wall second |
|---|---|---:|---:|---:|
| Server `updateOneNpc` | `src/npc/npc.cpp:1580` | 12.40% | 3.043% | 365.3 ms |
| Server `doCollisions` | `src/physics/movement/physics-collision-dispatch.cpp:181` | 5.33% | 1.308% | 156.5 ms |
| Client `engineTickSimulate` | `src/engine/engine-tick.cpp:112` | 9.85% | 2.811% | 362.7 ms |
| Client `engineTickSimulate` | `src/engine/engine-tick.cpp:222` | 6.79% | 1.943% | 249.2 ms |
| Client `engineTickRenderFrame` | `src/engine/engine-tick.cpp:242` | 7.75% | 2.205% | 283.3 ms |

These are 1/64 sampled thread-cycle estimates normalized against same-window process CPU time. Probe metadata says scopes may overlap and callsites may run on multiple threads. They are partial scopes, not an additive partition; no measured function accounts for 97% of process or host CPU. The exact run was 67 NPC/1 player on the server; client reported one player but its NPC field was zero. No 240-FPS client or 60-Hz server acceptance was demonstrated.

## Generation 573 build sample correction

The file `.dev/perf-captures/postgen572-mode3-20261123T2211Z.csv`, captured 22:11:54.175–22:12:08.305 UTC, overlaps the root game DLL attempt `game-dll-20261123T220945368970Z-09cabb` (22:09:45.369–22:23:53.871 UTC). It must be classified as a during-build sample, not a post-build/no-build sample. The attempt sidecar reports 534 samples, 12 logical CPUs, mean host CPU 99.996%, peak 100%, and minimum available physical RAM 19,185,664 bytes. These show severe concurrent system pressure; they do not alone prove why the 848.502-second build took that long.

The coordination brain reports gap `build-resource-sampling:game-dll-20261123T220945368970Z-09cabb`: no matched pre-build baseline, journal-aligned live mode/workload/frame/tick data, or host commit charge/limit. The sample classification correction and gap were appended to CPU issue `issue-ed3ac775283d` and sent to the DLL-latency owner.

## Next profiling target

A separate slow-tick journal lead records `NpcDifficultyConfig::pollReload()` called from `src/network/server-npcs.cpp:2102-2103`; implementation is `src/npc/npc-difficulty-config.cpp:312`, with `std::filesystem::last_write_time` checks in `getLastWrite` at line 25 and polling sites at lines 316 and 326. On 255 rate-limited ticks already selected for `total_ms > 50`, its wall-time mean was 2.098 ms, median 0.721 ms, p95 7.596 ms, max 41.789 ms. This is selection-biased elapsed time, not CPU time; it does not establish that file polling is a significant CPU consumer.

Requested a bounded CPU bucket around the poll from the NPC source owner, followed by a same-run A/B only after source ownership and the pending Mode 3 batch are clear. The DLL-latency owner has been asked to confirm ownership of both paths. No source edit or build is authorized by these observations alone.


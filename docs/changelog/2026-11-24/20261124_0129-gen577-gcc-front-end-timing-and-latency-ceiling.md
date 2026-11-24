# Gen577 frontend timings and the current edit-to-live ceiling

## Scope

Continue the Mode 3 edit-to-live latency goal. This was a read-only investigation of the latest warm DLL attempt, compiler timing records, current dev-loop ownership, and coordination with the resource profiler. No C++ or build-system implementation changed, no build was requested, and no MiMITA process was stopped or started by this work.

## Current build state

At the time of inspection, the root dev-loop was idle with source generation 48, game DLL generation 577 built from source generation 48, and no pending batch. The dashboard's global `published_stale` label is due to the resident EXE remaining at source generation 4; it does not mean game DLL generation 577 is stale for current game sources.

Warm attempt `game-dll-20261124T010546039497Z-2ea83a` took 47.154 seconds. It compiled five changed source translation units plus the generated identity provider and skipped 637 of 643 units. Changed-source preflight took 33.494 seconds; the five direct TUs each took 24.902–33.279 seconds. LLD took 5.542 seconds to link 642 object inputs totaling 486,582,242 bytes. Object-cache scan took 0.052 seconds and publication 0.427 seconds. The cache and dependency checks were not the dominant costs in this warm attempt.

The automatic sidecar recorded 44 samples, mean host CPU 99.715%, peak 100%, and minimum available memory 228,880,384 bytes. Its runtime alignment is explicitly unavailable. A manual same-window process join from the resource investigator attributed about 20.979% whole-host CPU to the client/server pair, but the run manifest's server PID was stale and the current run was Payload/funworld3 with one NPC, not the goal's Juggernaut workload. Do not infer that the live pair caused the build time.

## Compiler phase evidence

All five changed sources used the shared PCH. GCC `-ftime-report` rows in the attempt JSONL show, per TU:

- parsing: 15.16–16.82 seconds;
- deferred language processing: 3.41–6.88 seconds;
- optimization/code generation: 4.94–8.94 seconds;
- overload resolution: about 2.89–4.13 seconds.

These are GCC-reported phase timings, not a separately sampled whole-host wall-time decomposition. Each compiler was running during 99.7% host CPU and severe memory pressure, so intrinsic compiler work and host contention remain mixed. The rows make frontend parsing and optimization the next compiler leads; no single header or template has been proven responsible.

An earlier PCH-on/off comparison on a different set of 15 sources favored PCH-off for every TU and reduced the observed batch compile wall from 38.786 seconds to 20.516 seconds. The PCH-off changed sources still required 20.5–23.4 seconds per TU. This supports a controlled PCH-off A/B for representative files, but does not by itself approach the 10-second target.

## What remains between this path and the target

The warm five-file build already exceeds 10 seconds before runtime activation. Its slowest changed TU took 33.279 seconds, and the current DLL path still links the full 642-object game DLL. A 15–20-file change cannot reach the target through cache-scan tuning or header fanout cleanup alone while these compile/link costs remain. Header decoupling remains useful for batches that edit shared headers; this warm attempt changed only `.cpp` files and had no cache invalidations.

The next technical options are a matched PCH-off experiment for representative changed sources and a root-dev-loop-owned prototype that compiles/links only a coarse behavior candidate or patches supported functions, retaining the current MinGW whole-game DLL as fallback. Any function-patching path still needs explicit ABI, state-layout, and activation/rollback limits. No prototype was started in this session.

## Runtime and resource collaboration

Pinned run `20261124_011628` loaded game DLL generation 577 in a Juggernaut Horde server/client pair and reported 67 NPCs/1 player. It then failed active-mode startup after a 5.36-second maximum tick stall; 60% of sampled ticks exceeded 100 ms. The processes terminated and no matched host/process sample covered that run. This proves DLL activation, not healthy gameplay or the latency objective.

The only currently verified live pair is the agent-owned Payload run `20261124_005533`; its journal/manifest PID mapping is stale. The resource profiler confirmed no active capture and asked to wait until the Payload owner releases the run before creating an isolated pinned Horde pair and collecting a fresh 30–60-second baseline. I will preserve that run and avoid competing capture/build work until coordinated.

## Evidence and follow-up

- Added the measured warm-build ceiling and PCH-off comparison to issue `issue-136d4553ff92`.
- Added the GCC per-TU phase report, with contention caveats, to the same issue.
- Build artifacts: `.dev/build-timings/game-dll-20261124T010546039497Z-2ea83a.json`, `.jsonl`, and `.resource.jsonl`.
- Runtime journal: `logs/2026-11-24/20261124_011628/`.
- No compilation, tests, dev-loop restart, executable change, or source architecture change was performed.
- Follow-up EXE build sample, attempt `exe-build-20261123T204157-f923e86c`: successful in 166.920 seconds (build script 152.578s; post-build processing 13.123s; artifact validation 0.338s; publish 1.409s). Automatic resource capture ran 2026-11-24 01:41:59.564–01:44:44.686 UTC, 117 rows at about 1-second intervals on 12 logical CPUs. Host CPU averaged 99.995%; memory used averaged 86.57%; available RAM averaged 2.178 GB and fell to 228 MB; pagefile use averaged 35.61 GB. During compile-phase rows, top-process snapshots showed MiMITA PID27052 averaging 15.79% whole-host across the phase (peak 24.39%). Across all top-process snapshots, RobloxPlayerBeta PID30340 averaged 15.53%, FL64 PID27936 10.52%, and SNAPOS64 PID7384 8.01% whole-host. Compiler/linker rows were sparse in the top-process snapshots, so their apparent share is incomplete and is not an estimate of total compiler CPU. The sampler's runtime context named run `20261124_005533`, sandbox/funworld3/0 NPC, but had no journal paths; workload and client/server alignment are unverified. This is evidence of saturation during a cold EXE build, not proof that MiMITA caused the build delay. Artifacts: `.dev/build-timings/exe-build-20261123T204157-f923e86c.resource.json(l)` and root `.dev/state.json`.
- Process normalization correction for the same sample: on 12 logical CPUs, MiMITA PID27052 averaged 201.6% of one logical CPU (16.80% whole-host) across 113 top-process snapshots; RobloxPlayerBeta PID30340 averaged 186.3%/15.53%, FL64 PID27936 126.2%/10.52%, and SNAPOS64 PID7384 96.2%/8.01%. MiMITA server PID19056 averaged 34.1%/2.84% across 55 detected snapshots. Mode/workload remains unverified because journal paths were null.
- Current checkpoint after that attempt: root dev-loop reports no build running, game DLL generation577, source generation49, and a pending cold-only batch for `devscripts/dev-launch-modes.json`. The shared run is Bloodtest on `mimita-duels-map-v3` with zero NPCs and multiple clients, so it does not satisfy the Juggernaut latency workload. The current run is owned by another agent and was preserved; no build, capture, command, or restart was started by this session.
- Session ID: work-f081dabd4e36

## Continued run identity and pending-batch reconciliation

At 2026-11-24 03:11 UTC, the root dev-loop still had no active build. The pending Mode 3 batch had changed from cold-only to a mixed cold/game batch containing `devscripts/dev-launch-modes.json`, `src/render/post-fx.h/.cpp`, `src/gui/ui-system.h/.cpp`, `src/engine/engine-tick-ui.h/.cpp`, and `src/engine/engine-tick.cpp`. The current source-generation counter was 9 while latest successful source generation remained 49. A game-DLL request would therefore include a cold EXE target and unrelated renderer/UI edits. No files in this batch were edited or built by this latency task; ownership/readiness questions were sent to the Bloodtest and bodycam agents.

The exact cold-path diff is a new launch profile, mode 61, named “Bloodtest validation (NPC fill),” targeting `mimita-duels-map-v3`. The render/UI edits pass a framebuffer-use flag through `engineTickUI`/`uiBeginFrame` and add a post-FX setting. There was no active coordination lease for these paths at inspection time, so I preserved all changes and did not seal the batch.

## Fresh runtime evidence and journal identity caveat

The root run `20261124_022440_01` remained on Juggernaut Horde / `dust2cyberiav4`, server PID33892, game DLL generation577 and NPC module generation577. At 03:06:54.912 UTC, `npc.phase-window` from `src/npc/npc.cpp:4437` reported 1,197 updates and 574.794ms total elapsed time, including physics194.593ms, decision198.278ms, and collision158.466ms. At 03:06:55.178 UTC, `npc.server-phase-window` from `src/network/server-npcs.cpp:3373` reported 27 ticks, 683.58ms total and 621.30ms AI. These are overlapping wall-time scopes, not process CPU attribution. The server reported 67 NPCs and zero players; no build overlapped.

The client evidence for this run is not trustworthy. Root status at 03:11:25 showed client-01 PID3680 offline and client-02 PID2388 stale, with client-02's journal last updated at 02:39:55.993. The file labelled `client-01/events.jsonl` was receiving server-process events for PID6064, whose parent PID3680 was absent. PID2388 was alive with child server PID29656, but its client journal was capped at 99,999,714 bytes and stale. The process inventory therefore contained PIDs33892, 6064, 2388, and 29656, but did not prove a connected client to server33892. No client frame/FPS result or healthy matched-pair claim is made.

This correction was appended to the latency issue `issue-136d4553ff92` and sent to the resource-profiler for the CPU issue `issue-ed3ac775283d`. The resource profiler independently reported the same server-only condition and asked that its NPC timer A/B wait for a connected client and disposition of the shared batch. No build, runtime command, process restart, or CPU/RAM capture was started in this continuation; previously active processes were preserved.

## Handoff

- Await ownership/readiness replies for the pending Bloodtest launch profile and render/UI/engine files before any root build request.
- Do not add `src/npc/npc.cpp` to the mixed pending batch while the resource profiler owns that path and waits for a pinned connected client.
- After the shared batch is resolved, establish a fresh client journal and verify connection, workload roster, and generation equality before collecting build-overlap CPU/RAM evidence.
- The one-second arbitrary-C++ objective remains unproven; current MinGW per-TU compile and monolithic-link costs still require a separately measured module/function-patching prototype.

## Generation 579 cumulative build and live activation

Root Mode 3 attempt `game-dll-20261124T033440193460Z-3a5393` started at 2026-11-24 03:34:40.193 UTC and published generation 579 at 03:43:34.360 UTC, taking 534.174 seconds total (517.814 worker seconds). It carried only `src/ragdoll/ragdoll-mode.cpp` as the changed path relative to source generation 20, but this was not a clean one-file baseline: the earlier failed 13-path batch had left five changed headers newer than the cached objects. The object scan recorded 156 distinct dependent-TU invalidations: `ui-system.h` 100, `gamemode.h` 38, `post-fx.h` 8, `aimbody-config.h` 6, and `engine-tick-ui.h` 4. The result compiled 158 units including the generated identity provider and skipped 485. The changed source was compiled once during preflight; its object was reused. Stage timings were changed-source preflight 44.420 s, object-cache scan 3.857 s, dependent compile 461.703 s, LLD link 4.893 s, coherence 1.592 s, and publish 0.580 s. The cache scan itself comprised a 2.495 s dependency-index prepass, 0.481 s unique-dependency stat prewarm, and 0.822 s object freshness checks. The link consumed 642 normal object inputs (~486.7 MB) plus generated/extra objects.

The automatic resource sidecar captured 340 samples from 03:34:40.193–03:43:34.360 UTC on 12 logical CPUs. Host CPU averaged 99.977% and peaked at 100%; available RAM fell to 135,168 bytes (0.129 MiB), mean available RAM was 1.52 GB, mean memory use was 90.42%, and mean pagefile use was 38.72 GB. In the 281 compile-phase samples, tracked compiler/linker processes averaged 16.81% whole-host CPU and tracked MiMITA processes 9.55%. This establishes concurrent resource pressure, not a causal split; the sidecar had no matched prebuild baseline or fresh paired client/server journal alignment.

Runtime journal evidence: server PID 33892 in run `20261124_022440_01` activated `game-g579.dll` at 03:43:34.649 UTC, about 0.289 seconds after publication. Subsequent server hotness rows report generation 579, Juggernaut Horde on `dust2cyberiav4`, 67 NPCs, and zero players. Subsequent `npc.phase-window` events still report `module_generation=577`; source inspection confirms `ModuleHost::loadLocalModules()` caches the API generation from the active game module (`src/hot-reload/module-host.cpp:785-874`) and `poll()` returns early when `mLocalModules` is set (`:1155-1158`). This matches a separate NPC module refresh blocker. The registered client PID 2388 exited and no connected-client proof exists. The host DLL activation is verified on the server; NPC behavior activation and the <=10-second connected-pair goal remain unproven.

Artifacts: `.dev/build-timings/game-dll-20261124T033440193460Z-3a5393.json`, `.jsonl`, and `.resource.jsonl`; server journal `logs/2026-11-24/20261124_022440_01/server-01/events.jsonl`. Next discriminating trial is a clean behavior-only edit from the generation-579 object baseline on a fresh, verified connected Horde pair, measuring changed/dependent TUs through activation and separately verifying the target NPC/vehicle module generation.

## Selected collision outlier caveat and expanded batch

The resource profiler reported a follow-up parse of 264 `npc.collision.substage-sample` records from server PID33892 during 2026-11-24 03:13:05–03:18:30 UTC. These records are selected slow events (`diag.totalMs >= 10ms`) and throttled to at most one per 250ms, so they are not a representative timing distribution. Median total elapsed time was 23.67ms, median `sweep_slide_ms` 0.06ms, sweep p95 94.27ms and maximum 2557.68ms. In 123/264 rows, more than 90% of wall time was not accounted for by the logged phase-clock sum. Some tail rows were sweep-dominated, but the data does not establish sweep-slide CPU cost; preemption, waiting, or uninstrumented work may explain the discrepancy. The next diagnostic needs to reconcile total-vs-phase elapsed time and measure per-thread CPU/cycles before any optimization decision. There was no aligned host CPU sample, connected client, or build overlap in that interval. This correction is recorded in CPU issue `issue-ed3ac775283d` and cross-linked to `issue-136d4553ff92`.

At the latest root status refresh around 03:20 UTC, the pending batch had expanded to 14 paths: cold `devscripts/dev-launch-modes.json` plus 13 game C++/header paths: `src/render/post-fx.h/.cpp`, `src/gui/ui-system.h/.cpp`, `src/engine/engine-tick-ui.h/.cpp`, `src/engine/engine-tick.cpp`, `src/entities/aimbody-config.h`, `src/gamemode/gamemode.h/.cpp`, `src/gamemode/gamemode-runtime-resolver.cpp`, `src/ragdoll/ragdoll-mode.cpp`, and `src/network/community-match-client.cpp`. No build was active. The root Juggernaut server still had no verified connected client, and the capped/stale client journal remained unusable. Ownership/readiness questions were sent to the Bloodtest and bodycam agents; no reply was present at the last inbox check. The latency task did not edit those paths or request a build.

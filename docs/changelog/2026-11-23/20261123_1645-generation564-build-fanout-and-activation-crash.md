# Generation 564 build fanout and activation crash

## Build evidence

Root Mode 3 attempt `game-dll-20261123T162701884743Z-d98804` started at 2026-11-23 16:27:01.355 UTC and completed successfully at 16:39:06.246 UTC: 724.396 seconds (12 minutes 4 seconds). It published generation 564. The live server journal recorded generation 563 to 564 activation at 16:39:08.334 UTC.

The worker recorded 642 object/dependency units: 137 compiled and 506 skipped. All 133 cache invalidations were caused by `src/npc/npc.h` being newer than the corresponding object. The four weapon translation units visible as queued in the dashboard were transitive header dependents, not edited files. The cache skipped the other 506 units.

Measured phase durations: changed-source preflight 201.832s; object-cache scan 1.656s; compile 472.025s; LLD link 5.645s; coherence validation 0.203s; publish 0.432s. Manifest snapshot took 4.374s, source snapshot 8.516s, and packet-registry codegen 8.187s. The direct `server-packet-chat.cpp` compiler invocation took 194.028s within the preflight phase. Sum of per-translation-unit compile durations is 5,151.4 seconds across the parallel workers; it is cumulative worker time, not elapsed build time.

The 15-second sample immediately before this attempt (16:26:03.223–16:26:18.312 UTC) saw host CPU at 100%, RAM 96.17% used, and 585 MB minimum available. It ended about 43 seconds before the build started and did not overlap it. Therefore it establishes high pre-build pressure but does not prove that resource pressure caused the build's 12-minute duration. No concurrent root build was active; a single coordinated batch still took 12 minutes.

## Runtime evidence and blocker

The server journal confirmed generation 564 activation. A collaborating resource investigator then reported two Windows access violations (`0xc0000005`) in `game-g564.dll` at 16:39:09 UTC, including server PID 27384, less than one second after activation. The processes were left stopped for crash inspection. This is a temporal correlation, not a proven cause; runtime behavior and the `adminadmin` positive-password/probe path remain unverified.

Read-only comparison of `src/npc/npc.h` shows 13 movement-metric members added directly to `Npc` before `ActorFinalPoseState finalPose`. Existing NPC objects were alive across the generation change. This is a concrete object-layout compatibility risk that may explain both the 133-TU header fanout and the activation crash, but dump offsets have not yet confirmed it. Do not relaunch or hot-patch generation 564 until crash attribution and a safe layout strategy are established.

## Next measurements

1. Resolve the crash dump offsets against generation 564 symbols/source before relaunch.
2. Keep one build request and sample host/process CPU, available RAM, disk I/O, and compiler process utilization throughout the next controlled build.
3. Inspect why `server-packet-chat.cpp` preflight took 194s and which `npc.h` include edges account for the 133 invalidations.
4. Preserve existing dependency correctness; reduce header fanout by narrowing interfaces or keeping runtime-mutable measurement state out of `Npc` layout.
5. Retry `adminadmin` and the bounded CPU probe only after a safe candidate activates and a client reconnects; record build and runtime proof separately.

## Queue and compiler detail

All 137 compiled translation units report a nonzero `queue_wait_ms`: mean 233.00s, median 243.07s, p95 434.71s, maximum 449.16s. This wait is already included in the 472.025s compile phase; do not add it again to total build time. These are per-unit wait durations and overlap across jobs, not a 233-second serial addition.

The generation metadata records `jobs: 10`. Event-derived translation-unit start/end intervals briefly overlap 12 records for about 6 ms, despite the worker's `ThreadPoolExecutor(max_workers=min(jobs, ...))` limit. This is a timing-boundary discrepancy until reconciled with process-level sampling; it does not prove 12 compiler processes were active. The pre-build sample cannot prove which compiler processes ran concurrently.

For the three direct changed translation units, outer elapsed times were `server-packet-chat.cpp` 194.028s, `npc.cpp` 198.381s, and `server-npcs.cpp` 201.698s. GCC `-ftime-report` parsed 152.08s, 164.63s, and 171.86s respectively. Parsing took about 49–50s and preprocessing 31–33s in each report. The outer elapsed minus GCC-reported phase totals is 30–42s per unit and remains unexplained; process sampling during a future build is needed to separate scheduling, I/O, memory pressure, and unreported compiler work.

## NPC header dependency graph

The 133 `npc.h` invalidations were compared with current C++ source include directives. A broad source search found 72 `.cpp` files directly including a path named `npc.h`; 65 of those paths appear in this attempt's invalidation set, and 68 invalidated translation units do not directly include `npc.h` in the current source tree, indicating transitive header dependencies. Seven direct includers were not counted as stale-object invalidations because they were changed or otherwise handled through the preflight path. The count is an include-graph lead, not proof that each edge can be removed without changing behavior. Splitting a header helps only when includes/dependencies are narrowed; merely moving declarations among headers while preserving the same include chain will not reduce rebuild fanout.

## Crash offset symbolization

The generation564 DLL PE ImageBase is `0x180000000`. Symbolizing preferred virtual addresses (ImageBase plus the dump's RVA) maps PID27384's `game-g564.dll+0x83a446` read access violation to `NpcSystem::refreshDifficultyTuning()` at the build snapshot's `src/npc/npc-spawn.cpp:439`, inside a range-for over `npcs` that begins at line 438. PID3640's `game-g564.dll+0x3df54f` maps to `mimita::game::serverRuntimeSimulateWorld(void*)` at snapshot `src/game/server-runtime-module.cpp:311`, inside a range-for over `npcSystem.all()` beginning at line 310.

Both source locations iterate live `Npc` vectors. The same generation564 snapshot inserted 13 fields into `Npc` before `ActorFinalPoseState`, so new code may use the new `sizeof(Npc)`/element stride to traverse elements allocated with the generation563 layout. This is a strong source-and-offset explanation for the invalid reads, but register-level dump validation is still needed to confirm the precise bad pointer arithmetic. Do not hot-reload this layout-changing candidate into a process with old-generation NPC objects; keep the diagnostic state outside `Npc` layout or implement explicit migration before repeating live activation.

# Generation 642 edit-to-live timing and sampler critical-path evidence

## Goal status

The active goal remains unmet: reach at most 10 seconds edit-to-live for a coordinated Mode 3 batch changing 15–20 C++ files, under a measured live Juggernaut Horde server/client workload. This pass reviewed one source-only attempt and identified a measured pre-build delay. It did not run a new build or alter the active runtime.

## Attempt and activation evidence

Attempt `game-dll-20261125T084339926983Z-be8067` changed only `src/npc/npc-combat.cpp` and published generation 642 successfully. It compiled 2 translation units and skipped 643. Total attempt time was 19.781 seconds. The changed source compile took 8.724 seconds; LLD took 2.313 seconds over 644 object inputs (about 495 MB); object-cache scan took 0.016 seconds; publish took 0.312 seconds.

The pinned live run was `20261125_082735`: server PID 10644 and client PID 7452, Juggernaut Horde on `dust2cyberiav4`, with the server reporting 67 NPCs and 1 player. The server journal proves it activated `game-g642.dll` at 2026-11-25 08:43:59.098 UTC; the same client PID activated it at 08:43:59.361 UTC. From attempt start at 08:43:39.926 UTC, this is 19.172 seconds to server activation and 19.435 seconds to client activation. The source-save timestamp was not recorded, so these are attempt-to-activation values, not full edit-to-live values.

The host DLL generation changed, but the same server's later `npc.phase-window` rows still reported `module_generation=641` while `game.hotness.summary` reported generation 642. Therefore the DLL swap is proven, but execution of the changed NPC-local code is not. The client journal continued emitting frame-slice rows, but this pass did not aggregate frame acceptance; its NPC count also disagreed with the server roster. The CPU A/B owner was still collecting its gen642 post-change sample at the end of this pass. Preserve that run and its journals.

## Measured pre-build delay

The attempt sidecar reported 13 resource samples, mean host CPU 60.169%, peak 75.2%, and minimum available RAM 6,219,526,144 bytes. It did not provide a matched pre-build baseline, host commit-charge limit, or same-run journal/runtime alignment.

The attempt started at 08:43:39.926 UTC; its first `build.started` event was at 08:43:43.873 UTC, a 3.947-second interval. `BuildResourceSampler.start()` runs synchronously before `_run_game_build()` and performs process discovery before starting its sampling thread. The sidecar measured 3.953 seconds of sampler setup wall time and 3.813 seconds of sampler-thread setup CPU time. These timestamps identify sampler setup as work on the attempt critical path, separate from compilation. The sidecar also reports 6.692 seconds cumulative process-discovery wall time, which is not all serial build delay.

## Decision and next action

This one-file result misses the 10-second target by at least 9.172 seconds to server DLL activation; it is not evidence that a 15–20-file batch can meet the target. Header fan-out reduction alone cannot remove the measured compile, link, and setup time. A hypothesis to move the initial broad process scan off the synchronous build-start path has been appended to the build-timing issue. Any prototype must preserve approximately one-second host CPU/RAM samples, report process-attribution warm-up honestly, and prove valid process CPU deltas on short builds before adoption.

The current live pair belongs to the CPU-reduction agent. No daemon reload, runtime command, build request, or competing sample was issued. Next, wait for that agent's explicit release, verify the NPC module activation path, and then coordinate one measured 15–20-file Mode 3 attempt with continuous host/process data and exact same-PID activation evidence.

## CPU optimization pass status

The generation642 change is one falsifiable runtime treatment: an early `shouldLog(NpcCombat, Important)` gate in `src/npc/npc-combat.cpp:124` should skip angle math, vector normalization, and JSON construction when the category is filtered. It was built and the host DLL activated on the existing pair, but the NPC module remained generation641. That does not prove the changed function ran and provides no CPU before/after result.

The existing pinned run remained active through 09:04 UTC with journals advancing; I preserved it. `mode3-latency-continuation` confirmed it will not interact with the pair and said it belongs to this CPU A/B work. The original coordination owner `cpu-a-b-verification` is still active and has not replied to the release signal. I did not launch an overlapping pair, issue commands, or build again. The CPU issue now records the exact activation mismatch and next experiment. Continue with a fresh generation642 process pair after explicit run-owner release, verify host and NPC module generation match, then collect short matched host/process CPU+RAM, callsite, client frame-threshold, server tick, and gameplay evidence. Until then, CPU effect and behavior acceptance are unknown.

Historical source review also confirmed commit `45580410` (Nov 22) added 118 lines of once-per-second FCS movement telemetry to `NpcSystem::updateOneNpc`; it is gated to `fun_cs_rush` profiles and was inactive for this Juggernaut workload. The commit's timing near reports of high CPU is not causal evidence. This remains a separate candidate for an FCS-specific pass, after the current one is proven.
## Evidence correction: generation 642 and CPU A/B chronology

The 08:45:40–08:45:55 UTC resource window was after host DLL642 activation (server10644 at08:43:59.098Z; client7452 at08:43:59.361Z), so it is a post-change sample, not a gen641 pre-code sample. It measured pair47.83% whole-host CPU (server16.47%, client31.36%), host67.22%, and no client frame met the4.167ms threshold (1016/1016 over; mean67.16 FPS). Server covered919 ticks/15 windows, mean10.31ms, with max tick63.01ms. The pre-activation sample at08:32:51–08:33:06 after setting `npc_combat=errors` measured pair30.06% host, but different activity and a 13-minute gap prevent attributing the increase or claiming a reduction.

`npc.phase-window.module_generation` reports `ModuleHost::activeGeneration("npc-behavior")`, not whole game DLL generation. The 641 value describes the NPC behavior API module and does not show that `npc-combat.cpp` in activated game DLL642 was inactive. No specific invocation counter for `logAimDebug` was collected, so the code-path frequency and CPU delta remain to be measured. The active logger-level A/B owner is preparing a same-process test on this pair; preserve its runtime/config until release, then take the fixed-config generation642 sample and decide whether this candidate helped.
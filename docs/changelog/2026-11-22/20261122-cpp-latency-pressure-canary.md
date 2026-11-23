# C++ latency canary under host pressure

## Goal

Continue reducing MiMITA edit-to-live C++ latency toward 10 seconds while preserving the shared root dev-loop build authority, MinGW DLL fallback, object-cache correctness, and stable module wrapper identity. This record distinguishes build evidence from runtime behavior proof.

## Changes in this work

- Reduced dependencies in `src/npc/npc-world-query.cpp` by replacing the broad collision include with `src/physics/movement/physics-raycast.h`; the file now needs only the ray traversal declaration. The earlier Gen527 canary measured this TU at 6.634 seconds outside the later high-pressure period (not a matched comparison).
- Made a behavior-preserving local rename in the same TU for a second automatic build canary.
- No build scripts, compiler choice, DLL architecture, or dev-loop authority were changed.

## Build result

Attempt `game-dll-20261122T221400682391Z-2b5e5e`, generation 536, source revision `be433688...`, completed successfully from 2026-11-22T22:14:00.682Z to 22:15:35.659Z.

- Total attempt: 94.965 s.
- Changed paths: `src/npc/npc-world-query.cpp` only.
- Cache: 641 skipped, 2 compiled (the changed TU and generated module identity provider).
- Changed-source preflight: 56.220 s; changed TU compile: 56.170 s.
- Object-cache scan: 1.652 s: dependency-index prepass 0.520 s, freshness checks 0.723 s, unique dependency stat prewarm 0.369 s.
- Link: 25.690 s, 642 object inputs totaling 482,226,844 bytes.
- Publish: 0.447 s.
- The generated identity-provider TU took 3.640 s to compile. Wrapper-facing identity declarations remain stable; per-generation values live in the one provider TU. Keep those identity semantics intact while investigating a runtime identity source that does not need recompilation per generation.

## Resource pressure evidence

A 12-second host sample overlapping this attempt measured 98.6% mean CPU across 12 logical CPUs and 1.01 GiB minimum available RAM. Process attribution returned an empty list, so high system pressure is confirmed but its owner is unknown. This does not prove MiMITA caused the pressure. The sampled test run was pinned to generation 533 and no longer reported live by dev-loop status after the canary; this attempt has no same-process activation or NPC behavior proof.

## Conclusion and next work

The ≤10-second end-to-end target is not met. The changed TU's 56.170-second compile under system saturation and the 25.690-second monolithic link each exceed the whole budget. Cache scanning and publish are already below two seconds combined in this sample. Focus next on attributed process-level pressure and repeated matched edit-to-first-behavior samples. Then evaluate a safe partitioned/incremental link or genuinely hot behavior layer; the current 642-object MinGW DLL remains the fallback until an alternative proves activation and behavior in a live journal. Do not claim a sub-10-second result from a build-only success.

## Evidence references

- Timing summary: `.dev/build-timings/game-dll-20261122T221400682391Z-2b5e5e.json`
- Timing timeline: `.dev/build-timings/game-dll-20261122T221400682391Z-2b5e5e.jsonl`
- Root dev-loop status reported published generation 536; no running primary game was reported.
- New evidence and architecture priorities appended to issue `issue-136d4553ff92`.

## Follow-up compile-path inspection

src/npc/npc-world-query.cpp's cached depfile is 20,714 bytes and includes src/pch.h, GLFW, and the large transitive GLM graph, although the source's direct includes are now narrow. devscripts/game_build_worker.py force-includes src/pch.h globally when PCH is enabled (around lines 1392-1396); --no-pch is global and changes the cache flags identity. A targeted PCH opt-out is a plausible experiment, not yet measured. It must use a distinct cache identity/object key and compare matched repeated builds; globally disabling PCH risks invalidating the existing cache and forcing broad recompilation. This follow-up finding was appended to issue-136d4553ff92.

## Matched two-process AI pressure

The canary overlapped live isolated run 20261122_221352, online from 22:13:52Z. Its Fun Counter Strike server/client ran on dust2cyberiav4 with --npcs 5, pinned to EXE artifact 2026-11-22T21:32:48.195707Z and DLL generation 535. This is a live two-process AI workload match, but not Juggernaut and not candidate generation 536 runtime proof.

A later 10-second process sample while this run remained online measured host CPU 99.9% across 12 logical CPUs and 2.74 GiB available RAM. MiMITA server PID 14756 used 143.3% of one core (70.1 MiB RSS); client PID 6816 used 260.7% (219.2 MiB RSS), about 4.04 logical cores combined. Concurrent RobloxPlayerBeta PID 5772 used 196.0% (1,065.8 MiB RSS). The server journal had 101,494 events when inspected; frequent events included 
pc.movement-metric (9,786), 
pc.attack-gate (9,539), 
pc.movement-commitment-created (9,489), 
pc.collision.fall-sample (6,450), and 
pc.target-sense (3,460). This identifies high active AI/logging activity during the run but does not isolate the build's CPU share or prove MiMITA alone caused host saturation. Exact evidence was appended to latency issue issue-136d4553ff92 and CPU issue issue-ed3ac775283d.

## Live Juggernaut activation under build pressure

I started isolated run 20261122_222548 through the root dev-loop, pinned initially to generation 536, map dust2cyberiav4, launch mode 44 (Juggernaut Horde), profile 	dm-validation (5 configured NPCs). The server PID was 27676 and client PID 1996. Server journal records the initial DLL path/hash in un.started; it records game.host.probe.started for generation 536, module.local.loaded count 8, 
pc.phase-window with module_generation:536, and 
pc.target-sense events for juggernaut_swarm with visible targets. The source call chain is NPC perception module -> host query callback -> 
pcWorldLineOfSightBlocked; runtime evidence proves active NPC perception in the same candidate generation, though it does not contain a dedicated event proving that specific helper was called.

While the Juggernaut run was live, root dev-loop published new builds and the same server PID hot-activated them without restart:

- Generation 537 (attempt game-dll-20261122T222413577092Z-1031f3) completed in 103.540 s; changed TU preflight 54.785 s; link 38.385 s. Published 22:25:57.140Z, swap began 22:26:06.510Z, and activated 22:26:06.652Z: 9.512 s from publication to activation.
- Generation 538 (attempt game-dll-20261122T222557307822Z-3cc471) completed in 88.520 s; preflight 54.035 s; link 22.287 s. Published 22:27:25.845Z and activated 22:27:26.436Z: 0.591 s from publication.
- Generation 539 (attempt game-dll-20261122T223021931130Z-82ff17) completed in 47.937 s; preflight 23.352 s; link 14.778 s. Published 22:31:09.879Z and activated 22:31:10.197Z: 0.318 s from publication.

These build edits belonged to concurrent agents and are observed, not controlled same-TU repetitions. All total edit/build/activation attempts remain over the 10-second target. The evidence isolates link/compile as the primary bottleneck and shows the warm DLL swap stage itself can finish in under one second. A 10-second sample while the Juggernaut pair ran measured host CPU 100% (12 logical CPUs), 1.36 GiB available RAM, server 104.7% and client 195.0% of one logical core; concurrent RobloxPlayerBeta used 187.6%.

Run metadata nuance: run 222548 declared generation 536 at launch, but the runtime auto-followed and activated generations 537-539 in the same PID. The journal captures each transition; un.json remains at initial generation 536. This needs an explicit pin/current-generation contract for reproducible comparisons and was appended to issue-be39a4f5ee17. Latency and pressure findings were appended to issue-136d4553ff92 and issue-ed3ac775283d.

## Generation identity clarification

The module_generation field in 
pc.phase-window is emitted from ModuleHost::instance().activeGeneration("npc-behavior") in src/npc/npc.cpp near line 4203. It identifies that local behavior module, not the whole-game DLL. Its value remaining 536 after the host activates whole-game generations 537-539 is not evidence that whole-DLL hot reload failed. game.generation.activated records the whole-game DLL transition separately in the same server PID. The source trace routes NPC perception LOS through the world adapter in src/npc/npc-perception-module.cpp, src/game/module-host-query-module.cpp, and the edited src/npc/npc-world-query.cpp; the journal confirms active Juggernaut NPC/LOS outcomes, but does not have a function-specific invocation counter for the edited helper. This distinction was appended to the latency issue.

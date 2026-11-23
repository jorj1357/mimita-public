# C++ latency: per-translation-unit PCH experiment

Date: 2026-11-22

## Goal

Keep reducing save-to-live C++ latency toward 10 seconds or less under a live Juggernaut/AI workload. Preserve the current MinGW DLL fallback, correct dependency and object caching, stable module identity, and root dev-loop build authority.

## Change made

Added an experimental no-PCH compile profile for only `src/npc/npc-world-query.cpp` in `devscripts/game_build_worker.py`. Its object key carries a profile suffix; all other TUs retain their legacy object keys. PCH timestamp invalidation and the `-include src/pch.h` flag are omitted only for that TU. Normal dependency tracking remains active.

Added `devscripts/quality/test_game_build_pch_profile.py` for cache key stability, source-scoped PCH selection, PCH timestamp behavior, and normal header freshness.

Validation: Python syntax compilation, `git diff --check`, the five new cache-profile tests, and eight existing game-build timing-report tests passed (13 total).

## Measured no-PCH translation unit

An automatic build by another agent, `game-dll-20261122T231534497789Z-06de62`, included the new cache profile. That agent changed `src/network/server-npcs.cpp`; I did not edit that file. The source of `npc-world-query.cpp` was unchanged from Gen540, but its versioned no-PCH object key did not exist, so the worker compiled exactly that object without invalidating other cached objects.

- Prior PCH-on compile of the same `npc-world-query.cpp` in Gen540: 20.008 s.
- Gen545 no-PCH compile of the same source: 1.060 s.
- Gen545 object scan: 1.779 s; 640 objects skipped, with the new variant object as the single cache miss.

This is strong matched-TU evidence that loading the full game PCH dominated this thin source. The host load differed between measurements, so the 18.9x ratio is indicative rather than a controlled A/B benchmark.

## Whole-build result and remaining floor

The other agent's Gen545 build took 40.312 s total. Its changed `server-npcs.cpp` compile took 22.540 s, while the target no-PCH object took 1.060 s. Other important stages were worker pre-output at 4.375 s, full LLD link at 8.004 s, object scan at 1.779 s, publish at 0.456 s, and coherence validation at 0.191 s. This is not a one-file save-to-live benchmark, and I do not attribute its total to my change.

Even with a tiny TU compile near one second, the current whole-game path spends about 14.2 seconds in pre-worker work, cache scan, and full DLL link before adding that compile. That path still cannot meet 10 seconds. A small-module path that avoids the 642-object relink and reducing the pre-worker span are the next large wins.

## Live runtime evidence

The root dev-loop run `20261122_231306` remained live in Juggernaut Horde while the candidate was built. Server PID 29536 and client PID 17508 shared an unchanged EXE artifact. The server journal later recorded generation-545 DLL handling and `game.hotness.summary` in PID 29536, with 67 active NPCs, 53,740 server calls, zero server failures, and zero hotness violations. NPC attack-gate and navigation events continued in the same journal.

The dev-loop reported Gen545 ready at 23:16:15.138Z. The first event carrying whole-game generation 545 appeared at 23:17:40.257Z, about 85.1 seconds later. This is an upper-bound activation observation, not an exact activation timestamp; there was no explicit `game.generation.activated` event in this journal. This activation delay is far above the target and must be measured directly before treating reload time as negligible.

The build happened with two live MiMITA processes and active Juggernaut AI. A nearby resource sample measured 96.9% host CPU and 2.42 GiB available memory. The exact CPU/memory sample was shortly before the Gen545 attempt, not synchronized to its start.

## Failed attempts and source ownership

Three preceding automatic attempts failed in changed-source preflight because another agent's `src/network/server-npcs.cpp` edit referenced missing `RecastNavigationResult::backend`. The source owner corrected that field in a later edit and Gen545 succeeded. The failures did not replace the published DLL; the live root runtime continued on its previous candidate. I left the other agent's source untouched.

## Next work

Run a controlled one-file source edit through the root dev-loop under the live Juggernaut workload, after coordinating the shared translation-unit file with the other active small-TU benchmark task. Record exact save time, first worker output, changed-TU compile, object scan, link, publish, first same-PID new-generation event, and function-specific visible behavior. The current per-TU PCH improvement is validated at compile time, but the 10-second end-to-end target remains unmet.

# issue-a6890271d414: Game DLL build 408 times out after rebuilding 642 translation units and retry fails stale codegen

## Human report (verbatim, immutable)

> 2026-11-20 18:05 build progress: pending=370 pending_compile_count=641 phase=compile; 18:18 build 338310 completed success=false failed_phase=worker_timeout worker_error=worker timed out after 3600s; next attempt 8abbf9 failed codegen check: packet_registry_codegen.py


## Assignment

### 2026-11-23T17:56:36.931Z — bloodtest-bodycam-runtime

Owner: bloodtest-bodycam-runtime

## Assignment

### 2026-11-23T18:13:45.299Z — bloodtest-bodycam-runtime

Owner: build-wait-reviewer

## Agent suggestion

### 2026-11-25T22:04:24.427Z — build-wait-reviewer

New occurrence of the game-DLL worker timeout during run 20261125_212905: attempt game-dll-20261125T214558738291Z-806cc2 (started 21:45:58.738Z) failed worker_timeout after 309.344s with 'worker timed out after 300s' (target generation 648, source_generation 25), then the retry game-dll-20261125T215331554351Z-19bd08 reused the object cache and published generation 651 in 84.812s with no codegen failure. Host resource sample during the failed attempt: mean host CPU 98.592%, peak 100%, minimum available memory 48,332,800 bytes, 309.329s window, while live run 20261125_212905 was active. The requested temp limit raise is already staged uncommitted at devscripts/dev_loop/game_build.py:1862 (MIMITA_GAME_BUILD_TIMEOUT default 300.0 -> 9999.0), but the running root daemon predates it and still enforced 300s. MIMITA_GAME_ATTEMPT_TIMEOUT (default 1080s) still caps the attempt, so a 9999s worker timeout only widens the cap up to 18 minutes. Next check: coordinated root-daemon reload, then one Mode 3 game batch; verify the new run's events.jsonl shows no worker_timeout and a published generation. Reviewer made no edit; wait occurrence recorded at issue-63ec094b8859.

## Agent suggestion

### 2026-11-27T01:21:31.112Z — build-wait-reviewer

Object-cache freshness evidence from the 2026-11-27T00:44Z over-budget wait (issue-51b425637ea9). Attempt game-dll-20261127T004412307260Z-20aea5 (gen 742, run 20261127_001718) FAILED at link after 105.875s with ld.lld undefined symbols PlayerAiController::{active,meleeWeapon,buildFrame,setMeleeWeapon,setActive} and ClientCollisionWorldView ctor + vtable, while changed_paths were only engine-tick-ui-game-hud.cpp and render-player.cpp and no source definition was missing (src/actor/player-ai.cpp:635, src/combat/client-collision-world-view.cpp:23). Three gen-742 attempts failed at link (20aea5 105.9s, f583bc 56.3s, 0200fc 90.9s). A one-line "build-cache recovery" comment added to src/actor/player-ai.cpp and src/combat/client-collision-world-view.cpp forced those TUs to recompile; attempt game-dll-20261127T005644836418Z-43dc27 then SUCCEEDED (gen 748, total 173.781s, compile 99.875s, link 50.078s). Evidence: .dev/build-timings/game-dll-20261127T004412307260Z-20aea5.json; ...004949308562Z-f583bc.json; ...005142888202Z-0200fc.json; ...005644836418Z-43dc27.json; logs/2026-11-27/20261127_001718/dev-loop/events.jsonl. Inference: the object cache returned stale/incomplete objects for these two TUs. Unknown: the exact freshness predicate that let them be skipped. No fix claimed; the recurrence was mitigated by forcing recompilation. Next check: confirm whether the cache keys on source content hash plus dependency set, and whether a recovered build should automatically invalidate a TU whose cached object is missing exported symbols.

## Agent discussion and suggestions

### 2026-11-27T01:23:22.638Z — build-wait-reviewer

Message: `msg-0a6d1874a02b` (question)

Subject: Stale object cache caused gen-742 link failures

Bounded handoff, no action demanded right now. The 2026-11-27T00:44Z over-budget wait (issue-51b425637ea9) was on attempt game-dll-20261127T004412307260Z-20aea5, which then failed at link with undefined PlayerAiController::* and ClientCollisionWorldView ctor/vtable even though both definitions exist; three gen-742 attempts failed the same way. A one-line comment added to src/actor/player-ai.cpp and src/combat/client-collision-world-view.cpp forced them to recompile and gen 748 then linked successfully (173.8s, .dev/build-timings/game-dll-20261127T005644836418Z-43dc27.json). Inference: stale/incomplete cached objects for those TUs. Unknown: the exact freshness predicate that skipped them. Please treat this as another data point under the DLL build observability umbrella; I appended the full timing evidence to issue-a6890271d414. No fix claimed and no files edited by me.

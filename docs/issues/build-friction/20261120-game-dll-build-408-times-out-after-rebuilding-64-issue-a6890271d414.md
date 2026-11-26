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

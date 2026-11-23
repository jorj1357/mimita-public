# issue-be83fb5843ba: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-false-positive-wait-policy)

### 2026-11-22T23:38:43.733Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait game-dll-build:2026-11-22T22:42:14.381Z (elapsed 60.994s, stage game_build). Verdict: false positive from the flat wait policy, not a build stall. The in-flight attempt succeeded.

EVIDENCE
- Journal: logs/2026-11-22/20261122_220630/dev-loop/events.jsonl lines 150-151 (dev-loop.wait.over_budget, wall_time 2026-11-22T22:43:25.492Z). Wake record wake-ceb4d53aa898 for issue-be83fb5843ba.
- In-flight attempt in the event reason: game-dll-20261122T224214407127Z-4d2a9b. .dev/build-timings/game-dll-20261122T224214407127Z-4d2a9b.json: started 22:42:14.407Z, completed 22:45:46.829Z, success=true, generation 541, total 212.411s, compiled 26 TUs, compile 150.36s, link 4.77s, changed_source_preflight 47.04s.
- Changed header src/input/input-router.h forced a 23-TU fanout; slowest compiles engine-tick-net 107.7s, engine-tick 97.5s, engine-tick-state 95.9s, engine-tick-camera 95.5s, engine-tick-combat 91.1s. A normal header rebuild legitimately exceeds 60s on this host.
- At the 60.994s crossing (~22:43:15Z) the build was progressing: changed_source_preflight finished 22:43:08.154Z and object_cache_scan + 23 more TU compiles were underway; it linked and published at 22:45:46Z. No stall.

MECHANISM
- devscripts/dev-loop.py:109-110 defines a single WAIT_ISSUE_SECONDS=60 applied to every kind/stage. check_wait_budget() at dev-loop.py:3486-3577 uses total age since game_build_started_utc (dev-loop.py:3531-3547) and hardcodes stage game_build with no per-stage budget and no progress test. Any normal game-DLL rebuild crossing 60s files an issue.

SECONDARY DATA DEFECTS (same occurrence)
- build_id in the payload is 2026-11-22T21:32:48.195707Z, the last published EXE artifact (matches runtime_build_id), not the in-flight game-DLL attempt; the real attempt id only appears inside free-text reason.
- generation is 540 but the in-flight attempt is generation 541 (payload lags).
- reason is the last 240 chars of a raw progress line (_record_game_build_output, dev-loop.py:2832-2863), so it is unstructured and begins with a stray '57439.34,' fragment.

DUPLICATION
- docs/issues/build-friction holds 129 '*-wait-over-budget-game-dll-build-at-game-build-*' issues; 82 created 2026-11-22 alone. This is the same root cause documented under parent issue-e044a931c54b. No new fix issue is warranted. Recommend closing/merging this occurrence into the parent policy fix rather than a per-wait issue.

PROOF / ACCEPTANCE
- Acceptance for this diagnostic issue is satisfied: wait_id, stage, reason, elapsed, build/run IDs, evidence, issue record, and exactly one reviewer wake (wake-ceb4d53aa898) all exist.
- The real fix (stage/progress-aware budget + structured build_attempt_id/generation in the wait payload) remains unclaimed under issue-e044a931c54b. No code edited; no source/lease claimed.

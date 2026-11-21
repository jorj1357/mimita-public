# issue-81d40f5f20b3: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-21T20:46:33.280Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence against the pinned build attempt and the wait policy. Conclusion: this is not a hung build; it is normal game-DLL build duration exceeding a flat 60s wait budget. No source edit is needed for this occurrence.

WAIT OCCURRENCE (verified)
- wait_id: game-dll-build:2026-11-21T20:05:41.774Z
- started_utc: 2026-11-21T20:05:41.774Z; elapsed_seconds: 60.527; budget_seconds: 60.0; stage: game_build
- Captured reason is a normal stage-start line, not a stall: codegen:packet_registry_codegen.py state=started at 20:05:43.800Z, elapsed_ms=1913.782 (build only 1.9s in).
- Issue evidence generation:430 is the last-published generation when the wait was reported, not the attempt target. The attempt builds generation 431.
- Reviewer wake was queued exactly once: wake-28e8124a18a3 (.dev/agent-wake/requests/wake-28e8124a18a3.json), status launched. Acceptance clause satisfied.

BUILD ATTEMPT (pinned)
- attempt: game-dll-20261121T200541886892Z-5b40e3
- started 20:05:41.887Z, completed 20:11:22.692Z, success=true, total_seconds=340.795, worker_total_seconds=209.94 (.dev/build-timings/game-dll-20261121T200541886892Z-5b40e3.json).
- Stage split: changed_source_preflight 56.478s; object_scan 89.771s; compile 76.745s for 12 units; link 41.866s; publish 0.915s.
- The codegen stage active when the wait fired took 77.749s total (20:05:43.800Z to 20:07:01.550Z); one mandatory stage alone exceeds the 60s budget.
- Generation 431 compiled and published; it was later superseded by generation 432 (.dev/build-timings/game-dll-20261121T201417234689Z-2d9df4.json, ready.json).

FIRST DIVERGENCE (owner and line)
- devscripts/dev-loop.py:108-109 sets one global WAIT_ISSUE_SECONDS=60 for every wait kind.
- devscripts/dev-loop.py:3391-3407 fires the game-dll-build issue from wall-clock age of game_build_started_utc alone, using game_build_detail as the reason; it does not compare elapsed time to an expected stage duration and does not require stage progress to have stopped.
- devscripts/build_friction.py:143-202 creates a fresh issue per wait_id, so every attempt longer than 60s yields a new near-duplicate issue. That is why the board holds many identical wait-budget records for this stage.
- packet_registry_codegen.py runs on every attempt and its recent durations vary widely (3.48s at 19:53:15Z, 3.64s at 20:14:21Z, 77.75s here, 83.18s at 02:30:26Z, 224.53s at 03:50:46Z), so a fixed 60s threshold cannot distinguish slow-but-healthy from stuck.

ASSESSMENT / NEXT
- This occurrence is build-friction noise, not a hang. The underlying measurement/latency work is already owned by issue-136d4553ff92 (owner: build-coordinator).
- Proposed fix for that owner or a separately claimed task (needs human approval): make the wait stage-aware and progress-aware - derive the budget from the current stage's recent duration, or fire only after no stage-progress event for N seconds, and coalesce per build attempt. Do not implement here.
- No files edited, no build or test run requested. Evidence confidence: high (pinned attempt summary plus stage timeline plus wake request).

## Context

### 2026-11-21T20:46:41.053Z — build-wait-reviewer

related_build: game-dll-20261121T200541886892Z-5b40e3

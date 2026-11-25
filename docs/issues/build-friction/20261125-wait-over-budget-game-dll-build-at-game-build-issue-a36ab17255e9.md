# issue-a36ab17255e9: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (evidence-gap)

### 2026-11-25T01:40:05.114Z — build-wait-reviewer

Reviewer: build-wait-reviewer

VERDICT: evidence-gap — wait is genuine; issue evidence partially incomplete.

OBSERVED (direct):
- Not a stalled/pending queue. In-flight attempt game-dll-20261125T011406552810Z-170580 (source_generation 53, revision e31d0caa...) started 2026-11-25T01:14:06.552Z and failed at link at 2026-11-25T01:18:40.5Z: total 273.931s (compile 248.211s / 198 TUs; link 3.464s). Source: .dev/build-timings/game-dll-20261125T011406552810Z-170580.json. Link errors: undefined symbols lastAiInputFrame(), engineRenderGhost(...), engineTickNet(...).
- The over-budget event fired once at elapsed 61.037s while the build was still compiling (events.jsonl lines 271-272, wall 2026-11-25T01:15:09.573Z). Exactly one dev-loop.wait.reviewer-wake exists for wait_id game-dll-build:2026-11-25T01:14:06.479Z (wake-43fe3fd80688), so one reviewer wake per occurrence holds.
- Resource sampling for the attempt captured 261 x 1s samples: mean host CPU 97.5%, peak 100%, minimum available memory ~22.2MB; runtime_alignment unavailable (unverified).

EVIDENCE DEFECTS (observed + inference on owner):
1. reason is an unreadable mid-JSON tail (...0012.472,"build_attempt_id":"...170580",...,"record_overhead_ms":4.629}), not a human-readable detail. First incorrect step: _record_game_build_output stores the last 240 chars of any worker line into game_build_detail (devscripts/dev-loop.py:3112,3142), and check_wait_budget copies it as reason (dev-loop.py:3892). Same defect as open tech-debt issue-6e08d89b55e3; not re-filed.
2. Issue evidence does not pin the in-flight attempt: top-level build_id is the last successful build 2026-11-24T19:27:02.742428Z, there is no build_attempt_id, and the actual attempt id appears only inside the corrupted reason. Root: generation/build_id read game_build_generation/latest_build, assigned only at completion (dev-loop.py:3259). Same identity defect tracked in issue-9a4503f6ed68 (in_review, owner wake-proof-wait).

CONTRARY / UNCERTAINTY:
- Not a noisy false positive: 61s is much less than the real 274s build. But the flat 60s total-elapsed budget still fires on legitimate long/high-fanout builds; making it stage/sub-stage aware belongs to issue-9a4503f6ed68, not here.
- The reason fragment mentions generation=630 while this attempt's timings target generation 627; the fragment provenance is not proven from the current journal. Treat it as evidence that any worker line (possibly from a concurrent/other attempt) can overwrite game_build_detail.

ACCEPTANCE: satisfied for wait_id, stage, elapsed, evidence record, single reviewer wake; NOT satisfied for a usable reason or top-level build/run IDs pinning the in-flight attempt.
ACTION: set related_build/related_run to the true attempt/run. No source edit, no fix claimed; root fixes remain with issue-6e08d89b55e3 (reason) and issue-9a4503f6ed68 (identity/grouping/budget).

## Context

### 2026-11-25T01:40:12.887Z — build-wait-reviewer

related_build: game-dll-20261125T011406552810Z-170580
related_run: 20261124_221014

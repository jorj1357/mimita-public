# issue-ec19949cbd21: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T03:48:05.991Z — build-wait-reviewer

related_build: game-dll-20261122T030300829638Z-7fb0c2

## Review (confirmed-wait-policy-noise)

### 2026-11-22T03:50:16.619Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Confirmed wait-policy noise, not a stalled build. No source was edited, no build/run was started, and no closure is claimed.

WAIT IDENTITY / EVENT
- wait_id game-dll-build:2026-11-22T03:03:00.564Z, kind game-dll-build, stage game_build, elapsed 60.944s vs budget 60.0s; recorded 2026-11-22T03:04:04.544Z, request auto-4.
- logs/2026-11-22/20261122_011031/dev-loop/events.jsonl lines 373-374 carry exactly one dev-loop.wait.over_budget and one dev-loop.wait.reviewer-wake for this wait_id; wait-id dedupe held.

NOT A HANG
- In-flight attempt game-dll-20261122T030300829638Z-7fb0c2 started 03:03:00.863Z and SUCCEEDED: generation 444 published 03:14:41.487Z, total 700.612s, worker 435.991s.
- At the 60.944s deadline the build was inside codegen:packet_registry_codegen.py (started 03:03:04.043Z, completed 03:05:40.605Z = 156.6s), actively progressing.
- Dominant phases (.dev/build-timings/game-dll-20261122T030300829638Z-7fb0c2.json): changed_source_preflight 227.747s (npc.cpp compile 227.402s), link 145.984s (gnu_linker 145.846s), publish 89.796s (external_dependency_manifest 73.773s), compile wall 38.242s.

FIRST DIVERGENCE / ROOT CAUSE
- The flat 60s WAIT_ISSUE_SECONDS (devscripts/dev-loop.py:109) is below the normal whole-game DLL build cost. check_wait_budget (devscripts/dev-loop.py:3520-3536) measures age from game_build_started_utc, so ANY normal game-DLL build over 60s files an issue regardless of progress.
- Corpus .dev/build-timings/game-dll-*.json (n=177): p50 140.8s, p75 377.5s, p90 1004.1s; 110/177 (62.1%) exceed 60s. This is budget/policy calibration noise, not a build anomaly.

ATTRIBUTION DEFECT (persists)
- Wait evidence build_id=2026-11-22T02:37:06.972018Z and generation=443 describe the previously published artifact; the in-flight attempt (generation 444) appears only in the free-text reason. I set related_build to the actual attempt id game-dll-20261122T030300829638Z-7fb0c2.

RECURRENCE
- Brain holds 59 issues titled Wait over budget: game-dll-build at game_build (42 in_review, 17 new) out of 83 build-friction issues. record_wait_incident (devscripts/build_friction.py:131-169) dedupes only by exact wait_id, which embeds a timestamp, so every >60s build creates a new issue instead of an occurrence on a canonical one.

REVIEWER WAKE DELIVERY
- wake-2d0f489bc1e1 created 03:04:02.397Z, claimed/launched 03:18:32.768Z/03:18:32.891Z (~14.5 min later), adapter opencode, attempts=2. Detection and queueing were correct; delivery lagged.

COMPLETENESS
- readiness ready=true, missing=[]; wait_id, stage, reason, elapsed, evidence, and reviewer wake are present. No missing reproduction/expected/acceptance/paths/proof fields to add.

RECOMMENDED FIX (needs separate claim + human approval; not implemented here)
- Drive over-budget detection from lack of stage/TU progress (last dev-loop.game-build.stage or translation_unit timestamp) for WAIT_ISSUE_SECONDS, or use per-kind budgets calibrated from .dev/build-timings (game_build p75 ~377s), instead of elapsed since build start.
- Dedupe build-friction by kind+stage signature (or a canonical issue with occurrences), not per-timestamp wait_id.
- Fix active-attempt attribution in the wait evidence (build_id/generation should be the in-flight attempt/generation).
- Investigate the ~14.5 min reviewer-wake delivery latency.

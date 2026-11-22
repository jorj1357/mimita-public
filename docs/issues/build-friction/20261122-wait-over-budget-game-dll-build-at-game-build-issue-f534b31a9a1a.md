# issue-f534b31a9a1a: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T04:00:44.235Z — build-wait-reviewer

related_build: game-dll-20261122T033743408692Z-e5f86d
related_run: 20261122_011031

## Review (confirmed-wait-policy-noise)

### 2026-11-22T04:03:32.056Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake wake-3d8f30a1ca67)

VERDICT: confirmed wait-policy noise, not a wedged build. No source edited, no build/run started, no closure claimed.

WAIT IDENTITY
- wait_id game-dll-build:2026-11-22T03:37:43.197Z, stage game_build, elapsed 62.01s vs budget 60.0s, request auto-7, recorded 2026-11-22T03:39:00.523Z.
- logs/2026-11-22/20261122_011031/dev-loop/events.jsonl carries exactly one dev-loop.wait.over_budget (line 468) and one dev-loop.wait.reviewer-wake (line 467) for this wait_id; wait-id dedupe held for this occurrence.

NOT A HANG
- In-flight attempt game-dll-20261122T033743408692Z-e5f86d made forward progress before and after the trip: source_snapshot started 03:38:32.053 / completed 03:38:51.363 (19.23s); worker started 03:39:00.604; src/npc/npc.cpp compiled 213.951s (03:39:02.004 -> 03:42:36.352); 11 identity wrappers ~27s each finished 03:43:10; GNU link 90.722s; generation 447 published 03:45:07.963Z with status ok, total 444.366s, compile 265.042s.
- The only changed source, src/npc/npc.cpp (206,006 bytes / 3,931 lines), costs 213.951s by itself. The recorded reason is a single long TU compile, not a stall.
- changed_source_preflight (214.82s) is that same compile, not duplicate work: game_build_worker.py:1768-1799 compiles changed TUs first and the later scan skips them via preflight_handled.

ATTRIBUTION DEFECT (recurs)
- Wait evidence build_id=2026-11-22T02:37:06.972018Z and generation=446 name the previously published artifact; the in-flight attempt e5f86d / generation 447 appears only inside the free-text reason. Cause: dev-loop.py:3532 uses self.latest_build and :3533 self.game_build_generation instead of the active game-build attempt id. Set related_build to the actual attempt and related_run=20261122_011031.

ROOT CAUSE / RECURRENCE
- dev-loop.py:109-110 WAIT_ISSUE_SECONDS=60 is applied uniformly to game-dll-build at dev-loop.py:3520-3536, but normal whole-game DLL cost here is 130-450s and any npc.cpp edit adds a single 214s TU. Every such healthy build trips the flat budget.
- Brain now holds 62 issues titled "Wait over budget: game-dll-build at game_build" (44 in_review, 18 new; newest issue-2b6479feedc1 at 03:58Z) of 87 build-friction issues. build_friction.record_wait_incident dedupes only by exact wait_id (timestamp-embedded), so each >60s build mints a new issue instead of an occurrence.
- A fix candidate (per-kind budget or no-progress predicate, plus active-attempt attribution) is already recorded on canonical owner issue-136d4553ff92 at 2026-11-22T03:46:01Z, so no duplicate suggestion was appended. Human approval is still required before any code change.

WAKE DELIVERY (this occurrence)
- wake-3d8f30a1ca67 queued 03:38:47.823Z; codex adapter launched 03:39:00.184Z; wake.retry (stale launched request recovered by scheduler) at 03:40:47.622Z; re-claimed by opencode 03:46:17.216Z and launched 03:46:17.278Z. Detection/queueing were prompt, but adapter delivery lagged queueing by 7m29s.

COMPLETENESS
- readiness ready=true, missing=[]; wait_id, stage, reason, elapsed, evidence and reviewer wake are present, so no missing reproduction/expected/acceptance/paths/proof field to add.
- Evidence class: build event/journal only. A successful build does not prove runtime behavior, and none is claimed here.

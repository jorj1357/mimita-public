# issue-41a872c08642: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Scheduled independent review

### 2026-11-24T04:02:53.179Z — resource-profiler

A bounded independent review was queued as wake wake-4a9bcecd5b4c after 503 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed-wake-queued-but-not-delivered)

### 2026-11-24T04:21:37.458Z — issue-review-sweeper

Reviewer: issue-review-sweeper

REVIEWER: issue-review-sweeper (wake-4a9bcecd5b4c, 2026-11-24T04:0xZ). READ-ONLY: no source edited, no build/run started, no fix claimed, no closure, no proof recorded.

READINESS / PROOF STATUS (observed)
- issue readiness: ready=true, score 8/8, missing=[], tasks=0.
- No task_proofs rows and no issue proof record exist for this issue; proof_expectation ("dev-loop wait event plus the issue record and reviewer wake record") is not formally verified here, and no proof exemption is recorded.
- Owner empty, reviewers [], reviews [] before this append, state new since creation 2026-11-20T21:18:35.654Z.

OBSERVED FACTS
- Created for wait_id game-dll-build:2026-11-20T21:17:34.934Z, stage game_build, elapsed 60.671s vs budget 60.0s (+0.671s), generation 415, request_ids ["auto-42"], reason is a normal "[GAME BUILD] revision=4d4c42... sources=642 jobs=12 ..." full-build banner.
- Acceptance "carries wait_id/stage/reason/elapsed/build ID/evidence": PASS. Evidence has wait_id, stage, reason, elapsed_seconds, budget_seconds, generation, build_id (2026-11-20T20:20:51.788061Z = last PUBLISHED build), source_generation, request_ids.
- Acceptance "reviewer wake queued exactly once": PASS on queueing. Exactly one wake, wake-bb452d13c88d, was queued 2026-11-20T21:18:37.158Z (2s after issue creation) (.dev/agent-wake/requests/wake-bb452d13c88d.json).
- That wake did not deliver a review. Events: codex claimed/launched 21:18:39; codex log ends with model_not_found "The 'gpt-6-luna' model is not supported when using Codex with a ChatGPT account"; wake.retry to opencode 2026-11-20T23:44:04.762Z; opencode launched 2026-11-21T00:13:32.190Z; wake.failed 2026-11-21T13:50:41.110Z "adapter process exited" (attempts=2, fallbacks=[]).
- The first review did not occur until the periodic sweeper wake-4a9bcecd5b4c was queued 2026-11-24T04:02:53.151Z, after 503 completed sessions (trigger=work_end). Gap creation-to-first-review ~3d6h44m; wake queued-to-failed ~16h32m.
- Current backlog (read-only store count this sweep): 186 build-friction rows, 185 open; 147 open issues titled "Wait over budget: game-dll-build at game_build" (130 in_review, 16 new, 1 assigned) and zero of that title closed.

INFERENCE
- The "queued exactly once" acceptance holds, but the productive intent ("wakes one reviewer automatically") failed for this occurrence: a queued wake is not a delivered review.
- 60.671s is expected build duration, not a stall: check_wait_budget measures now - game_build_started_utc while status=="building" (devscripts/dev-loop.py:3884-3900), and a full 642-source build routinely exceeds 60s (generation 579 this host: compile_seconds 506.113, total 529.529, 158 compiled). So the flat 60s budget fires on healthy builds.
- The issue's own risk ("a noisy wait policy could create duplicate friction issues") has materialized: 147 identically titled open issues.

CONTRARY EVIDENCE / UNCERTAINTY
- The record is cold-handoff-usable: it contains the exact wait_id/stage/elapsed/evidence, so "missing issue fields" is NOT supported.
- Per-wait_id dedupe works: occurrences=[] means this wait_id was never duplicated; the duplication is across DIFFERENT wait_ids, which the code does intentionally (build_friction.py:143-148).
- I did not verify the 2026-11-20T21:17 build outcome from its original dev-loop journal (2026-11-20 logs may be pruned); the reason banner is the only surviving build evidence. So claiming "this build was healthy" is an inference, not an observed outcome.
- Whether wake-bb452d13c88d's opencode run reached the issue and failed to append is not determinable from surviving artifacts; the request is status=failed with no issue_reviews row.

NEXT DISCRIMINATING CHECK (smallest, no build needed)
1. For one new over-budget game-dll wait, join the dev-loop.wait.reviewer-wake event to .dev/agent-wake/events.jsonl and assert the SAME wake id reaches either an issue_reviews row within 15 min or a wake.escalated row (the existing proposal on issue-84c584957e65); baseline for this occurrence is 0 reviews and wake.failed.
2. Compare the next 5 over-budget game-dll waits to their in-flight build_attempt_id/total_seconds; expected under current policy: all are healthy >60s builds and 5 new duplicate-title issues appear. That quantifies the flat-budget false-positive rate on live data.

PROCESS NOTE
No new process improvement is supported beyond proposals already recorded; this sweep adds one distinct failed-wake occurrence to issue-84c584957e65 (the existing owner of the codex model_not_found + fallback-drain class) rather than re-deriving the already-proposed fixes.

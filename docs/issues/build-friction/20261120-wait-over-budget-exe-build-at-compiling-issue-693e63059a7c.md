# issue-693e63059a7c: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Scheduled independent review

### 2026-11-24T02:27:46.525Z — hot-reload-latency-root

A bounded independent review was queued as wake wake-286880cab6bd after 493 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-occurrence-acceptance-partially-met-wake-delivery-failed-no-fix-claimed)

### 2026-11-24T02:53:10.160Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (issue-review-sweeper, wake-286880cab6bd, 2026-11-24T02:3xZ). Read-only: no source edit, no build request, no compiler, no second dev-loop, no close, no fix claim. Prior text preserved.

OBSERVED FACTS
1. Brief/readiness: kind=build-friction, state=new before this append, owner empty, 0 tasks, readiness ready=true missing=[] score=8. No prior reviews existed on this issue (issue_reviews count = 0); this is its first review.
2. Occurrence identity is complete and single: wait_id exe-build:38:2026-11-20T20:18:26.239Z, kind exe-build, stage compiling, reason "compiling 0/45 [CXX] src\replay\replay-factory-worker.cpp", elapsed 60.476s vs budget 60.0s, generation 38, source_generation 38, started_utc 2026-11-20T20:18:26.239Z, request_ids ["2026-11-20T19:52:38.802Z-2695cd08"]. Issue auto-created 2026-11-20T20:19:26.749Z. No other issue carries this wait_id; sibling issue-00b133ae7e13 has a different wait_id (exe-build:32:2026-11-20T19:42:22.969Z), so this is a distinct occurrence, not duplicate minting for one wait.
3. Reviewer wake was queued exactly once but never delivered. .dev/agent-wake/requests/wake-f41b134224b9.json: created 2026-11-20T20:19:28.040Z (one wake.queued), attempts=2, status="failed", error "adapter process exited", failed 2026-11-20T20:59:54Z. Events: codex claimed/launched 20:19:29, retry to opencode 20:33:56, opencode claimed/launched 20:48:37, wake.failed 20:59:54. No review was appended for ~3d6h until this sweep wake-286880cab6bd (queued 2026-11-24T02:27:46Z). So "reviewer wake queued exactly once" = true; "wakes one reviewer automatically" (delivery/completion) = false for this occurrence.
4. Acceptance evidence gap (build/run IDs). The evidence JSON has no run_id at all, and build_id="2026-11-20T19:55:20.256758Z" predates the wait's own started_utc (2026-11-20T20:18:26.239Z) by ~23 min. dev-loop.py:3861 sets "build_id": self.latest_build (last published build), so build_id does not identify the waiting build; the waiting attempt is only identifiable by generation 38 + started_utc. Same lagged-identity defect recorded on sibling wait issues.
5. Exact dev-loop journal is gone: logs/2026-11-20 now contains no run directories, so the original over-budget event cannot be re-derived from its own dev-loop/events.jsonl. The 60.476s value is taken from the issue's own evidence record, not re-measured here.
6. Same-class baseline (read-only, coord.db + .dev/agent-wake/requests, this sweep): 169 open build-friction issues with via_agent=dev-loop-wait; 18 have zero issue_reviews; 16 of those 18 have only a failed wake request (this issue included). This is the "failed wake + no review" gap already proposed in issue-9a4503f6ed68; this issue is one more confirmed instance.

INFERENCE / UNCERTAINTY
- The underlying delay is most plausibly EXE compile latency under a concurrent 12-job game-DLL build (established on the sibling chain: 1-TU compile 130-172s busy vs 19.3s idle). I did not re-measure the 11-20 occurrence because the journal is pruned; confidence medium-high on class persistence, not on this exact occurrence's ratio.
- The wake failure could be transient adapter instability or a repeatable delivery defect. Direct evidence: 373 request files in .dev/agent-wake/requests currently have status=failed and 16 zero-review wait issues each have a failed wake, so a systemic component is more likely than a one-off; I did not read the reconciler.
- Whether a reviewer was expected to remediate or only record cannot be decided from this issue; treat the delivery gap as owned by the wake path (issue-9a4503f6ed68 / issue-91e3384b219b), not by this wait record.

CONTRARY EVIDENCE
- The record side worked: wait_id/stage/reason/elapsed are present and correct, exactly one wake was queued, and no duplicate was minted for this wait_id.
- Not all wakes fail: sibling issue-00b133ae7e13 produced two substantive build-wait-reviewer reviews on 2026-11-20 and an independent sweeper review on 2026-11-24, so the channel is not universally broken.

NEXT DISCRIMINATING CHECK (one)
- Run the existing read-only "failed wake + no review" predicate over all 18 zero-review dev-loop-wait issues and, for each, issue at most one deduped bounded review wake (24h cooldown), then measure: (a) after 60 min, how many still have a failed wake and zero reviews (baseline today: 16), and (b) whether any issue with an existing review is ever flagged (must be 0). This tests the issue-9a4503f6ed68 proposal directly, using this issue as one known-positive, without new instrumentation or source change.

Verdict: confirmed-occurrence / acceptance-partially-met (record+queue yes; delivery and run-id no) / recurring-class-already-owned / no-fix-claimed.

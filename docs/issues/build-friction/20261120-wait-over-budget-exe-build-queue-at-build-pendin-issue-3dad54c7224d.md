# issue-3dad54c7224d: Wait over budget: exe-build-queue at build_pending

## Human report (verbatim, immutable)

> exe-build-queue wait at build_pending exceeded the wait budget


## Review (confirmed)

### 2026-11-20T17:52:15.050Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed issue-3dad54c7224d against the dev-loop wait record and wake journal.

Evidence
- Wait: exe-build-queue:95529.460 at stage build_pending, elapsed 60.935s vs budget 60.0s. source_generation=23, build_id=2026-11-20T07:47:09.981634Z; request_ids 2026-11-20T17:01:09.699Z-5c9e4334, 2026-11-20T17:01:13.433Z-8b36b669, 2026-11-20T17:01:26.240Z-a328b20c. All three later show status success in dev-loop --status --json.
- Issue completeness: `agent-coord issue show issue-3dad54c7224d` reports readiness.ready=true, missing=[], with reproduction, expected_behavior, acceptance, paths, risk, evidence, and proof_expectation populated. No fields needed completion.
- Deduplication: querying brain.db for evidence containing wait_id 95529 returns exactly 1 issue (issue-3dad54c7224d). No duplicate occurrence was created.
- Reviewer wake: `.dev/agent-wake/events.jsonl` has exactly 1 `wake.queued` for wake-8627cef525a5 / issue-3dad54c7224d at 2026-11-20T17:02:22.528Z (build-wait-reviewer). Later claims/retries are attempts on the same wake id, not new wakes.

Gap (evidence, not a fix request)
- The dev-loop journal path in status/state (`logs/2026-11-20/20261120_170259/dev-loop/events.jsonl`) does not exist on disk; run directory 20261120_170259 contains only client-01, server-01, and run.json. The wait event was generated at 17:02:20Z before run 20261120_170259 was created (run.json created_utc 17:02:59Z), so the on-disk journal for the wait event cannot be located from the current status path. Related retained-server identity paths also point into the new empty run dir while the live server journal remains under runtime_run_id 20261120_161114. Recorded here as evidence for the existing run/journal identity work; no code change claimed by this review.
- The reviewer wake prompt on this activation reached the opencode adapter as only its first line; the full prompt (including issue id and requested work) is present in `.dev/agent-wake/requests/wake-8627cef525a5.json`. This is consistent with issue-62ab3daa615d.

Verdict: wait record and single reviewer wake are correct; issue was ready and did not need field completion. No new duplicate issue created.

## Scheduled independent review

### 2026-11-23T15:25:51.301Z — resource-efficiency-goal

A bounded independent review was queued as wake wake-2cd479fc071e after 413 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-with-open-defects)

### 2026-11-23T15:55:51.282Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-2cd479fc071e), read-only, 2026-11-23T15:26Z-15:5xZ. No source edit, no build request, no fix claimed, no close.

OBSERVED FACTS
1. Readiness/proof: `agent-coord issue readiness issue-3dad54c7224d` -> ready=true, score 8, missing=[], tasks=0, state=in_review. The issue has 0 task_proofs rows and 0 issue_evidence rows. The enabling feature's proof lives on issue-9a4503f6ed68: evidence-0c12a438dd86 is proof_kind=unit with matched_events=[]; its 2026-11-22 review records the live dev-loop journal evidence separately.
2. Wait record intact and unique: evidence carries wait_id exe-build-queue:95529.460, budget 60.0s, elapsed 60.935s, stage build_pending, build_id 2026-11-20T07:47:09.981634Z, and 3 request_ids. Exactly 1 build-friction issue matches this wait_id (no duplicate); occurrences stays empty.
3. Reviewer wake was queued exactly once but delivered late and mislabeled. `.dev/agent-wake/events.jsonl` has exactly 1 wake.queued for wake-8627cef525a5 at 2026-11-20T17:02:22.528Z. Sequence: codex claimed/launched 17:02:24.121/17:02:24.188; codex failed with `model_not_found: The 'gpt-6-luna' model is not supported when using Codex with a ChatGPT account` (same log file); wake.retry switched to opencode 17:05:46.677; opencode claimed/launched 17:18:37.291/17:18:37.322 (pid 18744); review id 16 was appended 17:52:15.049Z by build-wait-reviewer; the request was then marked wake.failed "adapter process exited" at 18:28:29.506Z (attempts=2, fallbacks=[]). So the one wake did produce the review, but the final status is failed and queued-to-review latency was about 49m53s.
4. Linked build request IDs no longer resolve: `.dev/agent-coord/brain.db` build_requests holds 6 rows total, and none of 2026-11-20T17:01:09.699Z-5c9e4334 / 2026-11-20T17:01:13.433Z-8b36b669 / 2026-11-20T17:01:26.240Z-a328b20c. The only remaining trace is the issue evidence blob (also copied into .dev/tmp-issues.json).
5. Original journal still absent, but the current path is now proven durable. `logs/2026-11-20/20261120_170259/` does not exist at all now, so the 2026-11-20 wait event has no durable dev-loop journal. The same code path now works: logs/2026-11-23/20261123_144429/dev-loop/events.jsonl contains dev-loop.game-build.started, dev-loop.wait.reviewer-wake, and dev-loop.wait.over_budget for wait game-dll-build:2026-11-23T15:24:55.624Z.
6. Live recurrence of the known policy/adapter defects. issue-266c2a856cfc was created 15:25:56 for that wait; the flagged build succeeded (.dev/build-timings/game-dll-20261123T152455729876Z-62f5f6.json: success=true, total_seconds=107.935, worker_total_seconds=95.13) against the flat WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:114-115). Its reviewer wake wake-6f2f936370f3 selected codex primary at 15:25:57 and that codex log already contains the same gpt-6-luna model_not_found. Current population: 153 "Wait over budget" issues (131 game-dll-build, 6 exe-build-queue).

INFERENCE / UNCERTAINTY
- For this issue the wait record, the dedup-by-wait_id rule, and "one reviewer wake per occurrence" acceptance all hold. The delivery half is weaker than the prior review implied: the single wake produced the review but ended failed and took ~50 minutes.
- The ~50 minutes is explained by codex-primary model failure plus the retry waiting for the next drain; this matches issue-84c584957e65 rather than a new root cause.
- I could not independently re-verify that the 3dad build requests eventually succeeded: their request IDs are pruned and the 2026-11-20 journal is gone, so the prior review's "all three later show status success" is no longer checkable from current artifacts.
- High confidence the wake itself appended review id 16: the pid-18744 opencode log contains the issue-review tool call timestamped 17:52:15.003-17:52:15.049Z.

CONTRARY EVIDENCE
- The automation is not inert: the fresh 15:25:56 occurrence produced one issue, exactly one wake.queued, and both durable journal lines.
- No duplicate issue exists for either sampled wait_id (1 issue each).

NEXT DISCRIMINATING CHECK
- Apply the wake-reliability experiment proposed on issue-84c584957e65 (prefer the adapter whose last wake succeeded; record adapter exit code; mark completed when the review landed). Then measure the next 10 over-budget wakes: queue-to-review latency and final status; expected <=5 min and status completed for all 10.
- At issue creation, confirm request_ids still resolve in build_requests for at least the review window, and append the terminal build result as an occurrence on the same issue.

Verdict: confirmed-with-open-defects (wait record and single wake are correct; delivery latency/status and linked-ID retention remain open). No source, build, or close action taken by this review.

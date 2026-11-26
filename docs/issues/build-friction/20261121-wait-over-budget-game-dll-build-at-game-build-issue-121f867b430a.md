# issue-121f867b430a: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Scheduled independent review

### 2026-11-26T06:13:18.426Z — mode3-latency-continuation

A bounded independent review was queued as wake wake-3f8f3b236d89 after 772 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-occurrence-acceptance-mostly-met; single-wake-failed-no-review-until-sweep; build_id-mismatch-and-no-run_id; recurring-class-14-issues-today; no-fix-claimed)

### 2026-11-26T06:25:36.119Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review — issue-review-sweeper @ 2026-11-26T06:25:11Z (wake-3f8f3b236d89, scheduled after 772 sessions). No source edited, no build, no fix claimed, issue not closed.

OBSERVED FACTS
1. Readiness: `issue readiness issue-121f867b430a` -> ready=true, score 8/8, missing=[]; state `new`; tasks=0; reviews=0 in `issue_reviews` before this review.
2. Occurrence evidence (issue record): wait_id `game-dll-build:2026-11-21T15:19:35.672Z`, stage `game_build`, elapsed 60.236s, budget 60.0s, generation 420, request_ids ["auto-6"], build_attempt_id `game-dll-20261121T151936891977Z-a6abf2`. Stored `build_id` is `2026-11-21T02:26:50.306117Z` (does NOT match the 15:19 wait/build_attempt_id) and no `run_id` is present.
3. Reviewer wake: exactly one wait-review wake was queued for this occurrence — `.dev/agent-wake/requests/wake-67f7ae30cfff.json` (created 2026-11-21T15:20:39.675Z, agent build-wait-reviewer). It was recorded `status=failed`, `error="adapter process exited"`, `attempts=2` at 2026-11-21T15:55:28.893Z. Its `log_path` (`.dev/agent-wake/logs/2026-11-21T15-20-39.675Z-build-wait-reviewer.log`) is MISSING on disk.
4. No review was ever appended for this occurrence until the scheduled sweep note in the issue doc (`docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-121f867b430a.md` contains only the 2026-11-26 scheduling note, no 2026-11-21 review). So the wait got no substantive review for ~5 days.
5. Sibling corroboration: the 5 build-friction issues in `brain_check.claimable_issues` (issue-450cfc80a0b9, issue-ca900e60ed4b, issue-4c12dde1d7ba, issue-ad873eff97f3, issue-f995213a15b0) each have exactly 1 wake in `.dev/agent-wake/requests`, and all 1 are `status=failed`.
6. Aggregate wake record: 647 request files; statuses failed=486 (484 "adapter process exited", 2 WinError-5), cancelled=156, launched=4, queued=1. build-wait-reviewer: 259 failed vs 4 launched. This shows the wake status field alone does not distinguish success from failure (a sweeper wake that DID append review msg-13a00ff6d98f at 06:11 is also labeled failed), so "failed" is ambiguous, not proof of zero output.
7. Distinct, not duplicate: sibling issue-f995213a15b0 (wait_id game-dll-build:2026-11-21T15:25:31.164Z, 60.676s) was created 6 min after this issue — a different wait occurrence, not a duplicate of the same wait. The wait-over-budget class is currently flooding: 13 game-dll-build wait issues were created on 2026-11-26 between 00:04 and 06:19 UTC (DB query on `issues` where kind=build-friction).
8. Recurrence today: `.dev/build-timings` for 2026-11-26 show game-dll builds completing at 85.9s, 88.9s and 419.1s (all > 60s budget). Builds completed successfully (generation 687 status ok/ready), so over-budget wait != build failure.

ACCEPTANCE ASSESSMENT
- wait_id/stage/reason/elapsed present: yes. build_attempt_id present: yes.
- build/run IDs: PARTIAL — build_id mismatches the occurrence identity and run_id is absent.
- "reviewer wake queued exactly once for this wait occurrence": YES (one wake, no duplicate for the same wait).
- Follow-through (reviewer actually reviews): NOT MET for this occurrence — the single wake failed and no review was recorded until this scheduled sweep.

INFERENCE / UNCERTAINTY
- The first divergence in the review pipeline for this issue is the failed build-wait-reviewer wake with no appended review. I cannot determine the cause: the wake log is missing/pruned, and "adapter process exited" is used for both real crashes and normal one-shot agent exits, so it does not prove either. Root cause: UNKNOWN.
- Whether the wait itself was a genuine slow build vs a budget mis-calibration is not established for this 2026-11-21 occurrence: no matched build-resource sample exists (today's `build_resource_gaps` list missing matched prebuild baseline, host commit charge/limit, and journal-aligned mode/workload/tick metrics). Unknown, not zero.
- The build_id mismatch may be an intentional different identifier or a known stale-identity defect seen on siblings; not resolved here.

CONTRARY EVIDENCE
- Acceptance "exactly one wake" passed; this is not a duplicate-wake defect.
- The build that followed was not a failure; "wait over budget" here is a 60s policy budget, not proof of a wedged build.
- Wake "failed" labels are unreliable (see fact 6), so the 486-failed aggregate should not be read as 486 reviewer activations that produced nothing.

NEXT DISCRIMINATING CHECK
Pick one wake known to have produced an issue review (e.g. the one behind msg-13a00ff6d98f) and inspect its request `status` plus the adapter exit code in `.dev/agent-wake/events.jsonl`. If such a productive wake is also `status=failed/"adapter process exited"`, the defect is in wake status accounting (devscripts/agent-wake.py marking a clean exit as failure), not in the reviewer. Independent of that, confirm whether build-wait-reviewer requests for the 5 sibling issues ever produced a review; if none did, the loss is real delivery loss, not just mislabeling.

No source, build, or fix actions taken; issue left `new`.

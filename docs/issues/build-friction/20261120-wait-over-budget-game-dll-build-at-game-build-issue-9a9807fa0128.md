# issue-9a9807fa0128: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Scheduled independent review

### 2026-11-23T12:17:31.308Z — resource-efficiency-goal

A bounded independent review was queued as wake wake-fc7f58203d1d after 401 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-real-timeout-root-cause-tracked)

### 2026-11-23T12:38:48.073Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-fc7f58203d1d), read-only, 2026-11-23T12:1xZ. No source edited, no build/test run, no close, no fix claimed. Prior text preserved; this entry is append-only.

OBSERVED FACTS
1. Auto-created 2026-11-20T17:18:34.564Z for wait_id game-dll-build:2026-11-20T17:17:33.824Z (kind=game-dll-build, stage=game_build, elapsed 60.689s vs budget 60.0s). Evidence carries wait_id, stage, reason, elapsed, generation 405, build_id 2026-11-20T17:17:26.879159Z, source_generation 23, and request_ids [2026-11-20T17:07:34.561Z-6502c417, 2026-11-20T17:13:21.275Z-14cd20cd]. The 60.689s is the first wait-check past the 60s budget, not the build duration.
2. The waited-on build was a genuine failure, not a healthy long build. .dev/build-timings/game-dll-20261120T171733871137Z-338310.jsonl (same request_ids, source_generation 23, revision 27d86abe8836968b643cd59a1832f7bd1ee9226303b60baf07c15926860d3976) records stage.failed worker_supervised reason=timeout timeout_seconds=3600 at 2026-11-20T18:18:09.488Z, then build.failed elapsed_ms=3,635,670 (~60.6 min), failed_phase=worker_timeout, error "worker timed out after 3600s", exit_code 124, generation 405. The last stage.completed before the stall was source_snapshot (12.43s); after the [GAME BUILD] banner the worker emitted no further stage events until the supervised 3600s timeout.
3. The root-cause hang is already tracked elsewhere: issue-a6890271d414 lists this exact file `.dev/build-timings/game-dll-20261120T171733871137Z-338310.json` as evidence; issue-1bf97a27238f carries the same title (duplicate). This wait issue is not linked to either, and I found no issue tracking the 2026-11-19/20 worker-timeout cluster as a class.
4. Exactly one reviewer wake was queued for this wait: wake-523d13dad61a (agent build-wait-reviewer) at 2026-11-20T17:18:35.517Z. Per .dev/agent-wake/events.jsonl and requests/wake-523d13dad61a.json it was claimed/launched via codex (17:18:36.858Z, pid 34948), retried via opencode fallback (retry 18:28:29.498Z, launch 19:05:56.359Z, pid 19348), and ended status=failed, attempts=2, wake.failed at 2026-11-20T19:43:26.197Z "adapter process exited". It appended no review to this issue (reviews=[], state was still new). Acceptance's "exactly once" is met; review delivery is not.
5. This issue then had zero reviews for ~2d19h until the scheduled independent review wake-fc7f58203d1d (created 2026-11-23T12:17:31.265Z, launched 12:17:31.883Z), which is this review.
6. The occurrence's dev-loop journal is gone: logs/2026-11-20 is absent and logs/11-20-2026 is empty, while logs/11-22-2026, logs/11-23-2026, logs/2026-11-22 and logs/2026-11-23 retain content. Cause is bounded retention: devscripts/dev_loop/game_build.py:896 _prune_logs(root, keep_days=3) removes date folders older than the 3 newest, called at game_build.py:1303 and :1311. So the dev-loop.wait.over_budget / dev-loop.wait.reviewer-wake events named in this issue's proof_expectation can no longer be re-read. Retained independently: .dev/build-timings, .dev/agent-wake/events.jsonl, .dev/agent-coord.
7. The worker-timeout cluster is bounded in time: scanning .dev/build-timings, worker_supervised timeouts appear only in 3 attempts (game-dll-20261119T234902931363Z-d55095, game-dll-20261120T005736816615Z-58c811, game-dll-20261120T171733871137Z-338310), all completed 2026-11-20 between 00:57Z and 18:18Z; no later attempt timed out. The current generation 558 build (.dev/state.json current.json) succeeded in 29.985s.

INFERENCE / UNCERTAINTY
- Unlike the ~130 later "Wait over budget: game-dll-build at game_build" issues that fired on healthy 60-130s builds under the flat 60s budget, THIS occurrence is a correct detection of a real 3600s hang. The flat budget is noisy in aggregate; it was not wrong here.
- Acceptance is mostly met: wait_id/stage/reason/elapsed/generation are present and exactly one reviewer wake was queued. The build_id field is the latest EXE-build id, not the in-flight game attempt id (the known dev-loop.py identity defect); the real attempt id is recoverable from .dev/build-timings and the matching request_ids.
- The first reviewer wake failed at the adapter layer, not wait detection. I did not determine why the adapter exited; its log shows the session was diverted into a bounded "agent-wake test activation" and produced no issue review.
- I did not reproduce the 3600s hang and do not claim its root cause is fixed; I only observe no worker_supervised timeout after 2026-11-20.

CONTRARY EVIDENCE
- Current builds are healthy (gen 558 ~30s) and the three worker timeouts all stopped after 2026-11-20, so this may have been a transient toolchain/IO stall rather than a live recurring defect.
- The wait mechanism did create the issue and exactly one wake, so this is not an inert-hook failure; the gaps are wake delivery and evidence retention, not detection.

NEXT DISCRIMINATING CHECK
- Compare this wait issue with issue-a6890271d414 and issue-1bf97a27238f: confirm whether both root-cause issues describe the same timeout class, whether either has a fix or proof, and consolidate the duplicate pair. If the root cause is unfixed, link this wait issue to it; if fixed, record the fix and the absence of any worker_supervised timeout after 2026-11-20.
- On future occurrences, check whether a wait issue whose build later ends build.failed/worker_timeout is reconciled to its root-cause issue by build_attempt_id, and whether its reviewer wake produced a review. This occurrence is a baseline miss: one wake queued, wake.failed, zero reviews until the scheduled sweep ~2d19h later.

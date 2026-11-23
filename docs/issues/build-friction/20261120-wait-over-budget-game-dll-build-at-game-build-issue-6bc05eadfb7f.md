# issue-6bc05eadfb7f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Scheduled independent review

### 2026-11-23T07:17:10.108Z — cpulight-function-map

A bounded independent review was queued as wake wake-bd9412956e26 after 374 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed-true-positive-wake-failed-evidence-gaps)

### 2026-11-23T07:24:39.535Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review by issue-review-sweeper (scheduled wake wake-bd9412956e26). Read-only: no source/config/build-script edits, no compiler/build.py/module worker/second dev-loop, no new runtime, no file leases, no issue close, no fix claim.

OBSERVED FACTS
1. Readiness/proof: issue readiness=satisfied (ready=true, score 8/8, missing=[]), no tasks and no proof record. State was "new", reviewers=[], reviews=[] when this sweep started; the only prior automated action was the 2026-11-23T07:17:10.107Z coord.issue_review.scheduled event for this wake (brain.db/agent-coord events seq 9639-9640).
2. Original occurrence: wait_id game-dll-build:2026-11-20T14:58:18.083Z, kind game-dll-build, stage game_build, elapsed 60.364s vs 60.0s budget (WAIT_ISSUE_SECONDS default 60 at devscripts/dev-loop.py:109-110). request_ids ["auto-2","auto-3"], source_generation=3, evidence.generation=null, evidence.build_id="2026-11-20T07:47:09.981634Z".
3. The waited-on build is real and long, not a stall. .dev/build-timings/game-dll-20261120T145818136704Z-502a3e.json (attempt id is present only inside the issue's reason string): started_utc=2026-11-20T14:58:18.137Z, completed_utc=2026-11-20T15:26:55.115Z, total_seconds=1716.948, success=true, generation=404, worker_total_seconds=1523.155, compile_seconds=485.627, link_seconds=295.242, changed_source_preflight_seconds=177.16, compiled=4, skipped=638; slowest TUs physics-collision-glb-main 308.024s, physics-collision-core 208.902s, player-ai 176.025s. So the 60s over-budget signal correctly flagged a ~28.6-minute build for this occurrence.
4. Evidence-fidelity gaps in the issue record: evidence.generation is null although the pinned attempt reports generation 404; evidence.build_id is the then-published EXE artifact (matches the sibling finding), while the game-DLL attempt id exists only inside the reason text; the reason is a stale "[GAME_STAGE] manifest_snapshot started elapsed_ms=15544.77" record (14:58:33.682Z), not the phase the build was actually in at the 60.364s check; related_build is empty.
5. Reviewer wake: wake-363c44b7c8d6 (.dev/agent-wake/requests/wake-363c44b7c8d6.json) was queued exactly once at 2026-11-20T14:59:20.617Z, 1.8s after issue creation. codex claimed/launched 14:59:21.453Z/14:59:21.513Z; wake.retry switched to opencode at 15:27:57.773Z; opencode claimed/launched 15:41:42.600Z/15:41:42.698Z; wake.failed "adapter process exited" at 16:03:49.906Z (attempts=2). No issue review was ever appended.
6. The delivered prompt for that wake (.dev/agent-wake/logs/2026-11-20T14-59-20.564Z-build-wait-reviewer.prompt.txt) does contain the correct "Requested work:" review text for issue-6bc05eadfb7f, but the transcript (.log) contains 0 occurrences of "6bc05eadfb7f" and ends with "Bounded test activation acknowledged ... No code" (it ran the bounded wake-test preamble). The codex log shows model-manager decode errors ("unknown variant `max`"). So the wake was queued once but did not deliver a review; the intended follow-through failed.
7. Retention: the 2026-11-20 dev-loop run journal is unavailable (logs/11-20-2026 is empty), so the primary proof expectation's "dev-loop wait event" cannot be re-read for this occurrence. Corroboration survives in .dev/agent-coord/events.jsonl (coord.issue.opened 2026-11-20T14:59:18.823Z) and .dev/agent-wake/events.jsonl (queue/claim/retry/fail, seq 213-238).

INFERENCE / UNCERTAINTY
- Acceptance clause "The issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence" is only partly met: wait_id/stage/elapsed/request_ids exist, but game build generation is null, build_id is the EXE artifact, and related_build is empty.
- Acceptance clause "a reviewer wake is queued exactly once" is met (one queue event, one wake id); the next clause's intent (a reviewer actually appends evidence) was not met.
- This is the same failure family already reviewed on sibling issue-e84cfa28c7f7 (2026-11-23T06:53Z, bounded-test preamble, wake.failed 16:03:49) and owned by issue-84c584957e65 (4 occurrences, 2026-11-22, 0 reviews). The new fact for this issue is the earlier 2026-11-20 occurrence and the pinned 1716.948s successful build.
- Cannot distinguish from artifacts whether the failure is the bounded-test prompt template being selected for a real reviewer wake, the adapter dropping the appended "Requested work" text, or the adapter process exiting (model-manager error). issue-84c584957e65 is the right owner to discriminate.

CONTRARY EVIDENCE
- This occurrence is a genuine wait, not policy noise: the build ran 1716.948s and succeeded, so do not dismiss the issue as a false positive (compare the 61.088s false-positive waits already reviewed elsewhere).
- Detection and one-issue-per-wait_id dedupe worked; no duplicate issue was created for this wait.
- Later over-budget waits do show paired dev-loop.wait.reviewer-wake / dev-loop.wait.over_budget events (sibling review, logs/2026-11-22).

NEXT DISCRIMINATING CHECK
On the owner issue-84c584957e65: queue one bounded manual reviewer wake against a scratch issue with current agent-wake.py and assert (a) the delivered prompt contains the exact "Requested work: Review build/test wait issue <id>" text and (b) an issue_reviews row appears within 15 minutes to a healthy adapter with exactly one wake.launched; if (a) fails, fix prompt assembly; if (b) fails, fix adapter-exit handling. In parallel, for this issue's fidelity: confirm whether game_build_generation should be set before the 60s check in dev-loop.py:3757 and why it recorded null while the attempt reports generation 404.

VERDICT: reviewed-true-positive-wake-failed-evidence-gaps. No fix claimed; issue state left for owner triage.

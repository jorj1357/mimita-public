# issue-f31c330d18e0: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (needs-work)

### 2026-11-22T19:13:24.166Z — build-wait-reviewer

Reviewer: build-wait-reviewer

(no body)

## Review (needs-evidence)

### 2026-11-22T19:22:21.994Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: needs-evidence. The wait record and reviewer wake are materially correct, but the dev-loop journal reports this occurrence as failed, a false-negative in the over-budget pipeline.

## Review (needs-evidence)

### 2026-11-22T19:23:50.760Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Full review detail (evidence appended; review-only, no code changed).

Direct observations:
- Issue fields are complete; readiness missing=[] (nothing to complete). wait_id game-dll-build:2026-11-22T18:19:58.794Z, stage game_build, elapsed 62.045s, budget 60.0s, build_id 2026-11-22T17:39:49.763492Z, generation 509, source_generation 9, request auto-9.
- Exactly one issue exists for this wait_id; no duplicate was created.
- Exactly one reviewer wake exists: .dev/agent-wake/requests/wake-82d785beabc8.json, created 2026-11-22T18:21:04.184Z for build-wait-reviewer. This session is that wake, so the wake was queued exactly once.
- The owning run journal logs dev-loop.wait.issue-failed at 2026-11-22T18:21:34.577Z for this exact wait_id: the agent-wake.py run --once command timed out after 30 seconds. There is NO dev-loop.wait.over_budget and NO dev-loop.wait.reviewer-wake event for this wait_id, unlike the 18:05:34, 18:23:48 and 18:34:48 occurrences. Journal: logs/2026-11-22/20261122_174002/dev-loop/events.jsonl.

First divergence / owner:
- devscripts/build_friction.py lines 237-246: wake_reviewer queues the wake with agent-wake.py add (succeeds at 18:21:04), then synchronously runs agent-wake.py run --once with timeout=30.
- devscripts/dev-loop.py lines 3552-3577: check_wait_budget wraps record_wait_incident plus wake_reviewer in one try; any exception records dev-loop.wait.issue-failed and skips the success events. Because the add succeeded and the run call then timed out, the loop reports failure although the issue was created and the wake was queued and later launched.
- Same false-failure pattern appears at 17:42:18 (17:40:44 wait), 18:16:05 (18:13:53 wait) and 18:21:34 (this wait). Inference, high confidence: the synchronous 30s call with captured output blocks on the long-lived adapter child rather than on the queue operation.

Policy observation (not this issue's defect): whole-game DLL rebuilds are routinely over 60s (current.json total_seconds 186.6, compile_seconds 126.3; the generation 509 attempt exceeded 285s), and this wait fired as source generation 9 superseded generation 509. A 60s budget will fire on most whole-DLL rebuilds, producing recurring friction issues (issue-d668de6bfc5e, issue-e7b2635a5d05, issue-9202020e4618, issue-52e6479ff974). That budget/noise question belongs to the build-stage accounting issue, not a duplicate here.

Suggested fix (reported, not claimed): create the over-budget issue and queue the wake, then launch asynchronously or treat a launch timeout as non-fatal because the wake is already queued; emit dev-loop.wait.over_budget and reviewer-wake once the issue and queue entry exist. Re-verify on a new occurrence where .dev/agent-wake/events.jsonl shows wake.queued then wake.claimed and wake.launched, and the dev-loop journal shows matching over_budget and reviewer-wake events.

Confidence: high.

# issue-e9f3b2727a9a: Wait over budget: exe-build-queue at build_pending

## Human report (verbatim, immutable)

> exe-build-queue wait at build_pending exceeded the wait budget


## Review (policy-false-positive)

### 2026-11-20T16:55:01.073Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed as the assigned wake issue (wake-667af71a82dc). Record is ready (8/8, no missing fields). Evidence: wait_id exe-build-queue:90687.583 has generation=source_generation=14, started_utc empty, request_ids [], and reason 'idle; waiting for a source change or explicit build/launch request' (.dev/agent-coord issue evidence; created 15:41:39). This is the loop's idle/stale-artifact state, not a queued build blocked behind another build. Code path: dev-loop.py:2612 _queue_build_if_needed sets build_pending=True and pending_build_since_monotonic for the latest_stale launch/start/restart paths (dev-loop.py:2839, 3017, 3170) without adding a pending_build_request_id or a non-idle active_build_detail; check_wait_budget (dev-loop.py:3286) then reports an exe-build-queue wait using the stale idle detail and empty request_ids. Result: a misleading build-friction issue and reviewer wake for a normal idle/launch-pending state. Suggested next owner-level fix (not applied): when build_pending is set by the launch/stale path, record the triggering control request and a 'launch waiting for source validation/build' detail, or skip the exe-build-queue wait when request_ids is empty; also dedupe repeated occurrences. Confidence: high from the issue fields and code path; medium on which exact 15:41 trigger called _queue_build_if_needed without runtime control-event logs. No source edits; no other agent's files claimed.

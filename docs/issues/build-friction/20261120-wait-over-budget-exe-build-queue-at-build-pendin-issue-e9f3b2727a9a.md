# issue-e9f3b2727a9a: Wait over budget: exe-build-queue at build_pending

## Human report (verbatim, immutable)

> exe-build-queue wait at build_pending exceeded the wait budget


## Review (policy-false-positive)

### 2026-11-20T16:55:01.073Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed as the assigned wake issue (wake-667af71a82dc). Record is ready (8/8, no missing fields). Evidence: wait_id exe-build-queue:90687.583 has generation=source_generation=14, started_utc empty, request_ids [], and reason 'idle; waiting for a source change or explicit build/launch request' (.dev/agent-coord issue evidence; created 15:41:39). This is the loop's idle/stale-artifact state, not a queued build blocked behind another build. Code path: dev-loop.py:2612 _queue_build_if_needed sets build_pending=True and pending_build_since_monotonic for the latest_stale launch/start/restart paths (dev-loop.py:2839, 3017, 3170) without adding a pending_build_request_id or a non-idle active_build_detail; check_wait_budget (dev-loop.py:3286) then reports an exe-build-queue wait using the stale idle detail and empty request_ids. Result: a misleading build-friction issue and reviewer wake for a normal idle/launch-pending state. Suggested next owner-level fix (not applied): when build_pending is set by the launch/stale path, record the triggering control request and a 'launch waiting for source validation/build' detail, or skip the exe-build-queue wait when request_ids is empty; also dedupe repeated occurrences. Confidence: high from the issue fields and code path; medium on which exact 15:41 trigger called _queue_build_if_needed without runtime control-event logs. No source edits; no other agent's files claimed.

## Scheduled independent review

### 2026-11-23T09:54:37.057Z — resource-efficiency-goal

A bounded independent review was queued as wake wake-543618e24ce7 after 391 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (policy-false-positive-confirmed; class-recurs(6 issues); guard-not-applied; dedupe-exact-wait_id-only; no-fix-applied)

### 2026-11-23T10:08:17.942Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent read-only review of issue-e9f3b2727a9a (issue-review-sweeper, wake-543618e24ce7). No source edit, no build, no test run, no fix claim, no issue state change.

READINESS AND PROOF
- issue readiness: ready=true, score 8/8, missing=[], state=in_review; no tasks and no owner. No proof task or exemption is linked, which is correct for a dev-loop wait-policy record.
- Original occurrence evidence: wait_id exe-build-queue:90687.583, kind=exe-build-queue, stage=build_pending, elapsed=61.113s vs budget 60.0s, generation=source_generation=14, started_utc="", request_ids=[], reason="idle; waiting for a source change or explicit build/launch request", build_id=2026-11-20T07:47:09.981634Z (the previous EXE build, not a blocker).
- Wakes: exactly one reviewer wake for this issue, wake-667af71a82dc, wake.queued at 2026-11-20T15:41:41.905Z (.dev/agent-wake/events.jsonl line 226); one prior review (id 10, build-wait-reviewer, 2026-11-20T16:55:01Z). No duplicate wake found.
- Journals: no on-disk dev-loop journal survives for 2026-11-20 15:4xZ (logs/11-20-2026 is empty; logs/2026-11-20 has no run dirs), so dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake for this occurrence cannot be inspected. proof_expectation is only partly met: issue record and wake exist, the wait event does not.

OBSERVED FACTS (current source)
- dev-loop.py:3726-3743 (check_wait_budget, exe-build-queue branch) still files a wait without inspecting pending_build_request_ids or any blocking build; an empty request_ids list does not suppress or annotate it.
- dev-loop.py:2950-2958 (_queue_build_if_needed) still sets build_pending=True and pending_build_since_monotonic without recording a triggering control request id. Its callers at :3247, :3441, :3594 are the launch / server-start / server-restart "waiting for source validation/build" paths.
- build_friction.py:131-140 (_wait_issue) and :143-169 (record_wait_incident) dedupe only on the exact wait_id; the module comment states a new wait gets a new issue.
- dev-loop.py:3731-3732 encodes wait_id with a process-local time.monotonic() float, so each loop process can mint a fresh wait_id for an unchanged condition.
- Reason truncation is unchanged (dev-loop.py:3006 detail=line.rstrip()[-240:] when the detail is read at :3735); tracked separately as issue-6e08d89b55e3.

INFERENCE
- This specific occurrence is a policy false positive: build_pending was set with zero queued requests and no active build, and the recorded reason is the loop's idle message, so a normal idle/launch-pending state crossed the flat 60s budget and produced a misleading issue plus reviewer wake. Confidence high for the record itself; the earlier review's medium confidence stands on which exact control action created build_pending at 15:41:39Z, because the journal is missing.

CONTRARY EVIDENCE
- The same title is not uniformly noise. Six issues share the signature: e9f3b2727a9a (2026-11-20T15:41Z), 3dad54c7224d (17:02Z), af958c3e10eb (19:56Z), 4891874dd02b (2026-11-22T01:35Z), f92715ba3c7f (03:53Z), 8cbff41b5590 (19:49Z). Four carried request_ids and were confirmed or assessed as real serialization behind game-DLL builds (reviews 16, 90, 113, 209). 8cbff41b5590 had empty request_ids but its review proved real contention behind game-dll-20261122T194850321333Z-5c98ea. Therefore a bare "skip when request_ids is empty" fix would also suppress real waits; the safer change records the blocker (queued EXE request ids plus blocking game-DLL attempt id) and distinguishes a genuinely idle state from a non-idle pending state.
- No new exe-build-queue occurrence since 8cbff41b5590 (2026-11-22T19:49:50Z). Current dev-loop status (2026-11-23): lifecycle=game_build_succeeded, build=published_stale, generation 554, detail="idle; waiting for explicit batch-build or launch request"; no wait is in progress.
- The over-broad fixed 60s threshold for queued EXE requests behind multi-minute game-DLL builds is already argued in reviews 90 and 113 and tracked under the wait-instrumentation program (issue-e044a931c54b task wait-instrument) and issue-136d4553ff92; do not duplicate those.

NEXT DISCRIMINATING CHECK (one, bounded)
- In the next root dev-loop process, trigger a stale-artifact launch with no sealed batch request and let build_pending persist past 65s; then repeat once with an EXE request accepted while a game-DLL build is active. For both, inspect logs/<date>/<run>/dev-loop/events.jsonl for dev-loop.wait.over_budget and check (a) whether request_ids is empty and the reason is the idle/launch message, and (b) whether the reason names the triggering control request or blocking attempt. If the launch path still yields request_ids=[] plus the idle reason, the false-positive branch is confirmed on current code; if it now records a request id or blocker, this record can be closed as fixed. This check also produces the wait events that the current record lacks.

UNCERTAINTY
- Whether class-signature grouping would mask a genuinely unique failure; grouping must retain every occurrence.
- The exact 2026-11-20 trigger remains unrecoverable without the missing journal.

No code was edited; no build, test run, or issue state was changed by this review.

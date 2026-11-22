# Issue-f92715ba3c7f Build-Wait Review

# Task

- Task ID: build/test wait review wake-8ce6b21fbc58
- Session ID: session-13e4195ce513 / work-af2d9ef19f71
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-f92715ba3c7f (exe-build-queue wait over 60s at stage build_pending)
- Summary: Reviewed the exact wait occurrence, its queue/contention window, the reviewer wake record, and the dev-loop journal. Attributed the occurrence to EXE build request `2026-11-22T03:52:50.598Z-b2290797` (build `2026-11-22T02:37:06.972018Z`, generation 10) queued behind game-DLL generations 449 and 450. Appended one evidence review (verdict `reviewed-expected-queue-contention`, review id 113) and one standalone suggestion. No source edits, no build, no run, no fix, and no closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T04:13:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d5

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review and suggestion were written through `agent-coord.py issue review` and `issue suggest`.

# Requested behavior

Review build/test wait issue issue-f92715ba3c7f without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete any missing readiness fields, append review and evidence, and report a code fix separately if one is needed.

# Findings

- Wait identity: wait_id `exe-build-queue:50736.849`, kind `exe-build-queue`, stage `build_pending`, elapsed 60.649s vs 60.0s budget, reason `build request queued through dev-loop`, request ids `2026-11-22T03:52:50.598Z-b2290797`, build_id `2026-11-22T02:37:06.972018Z`, generation 10, source_generation 10.
- Issue record completeness: readiness `ready=true`, `missing=[]`; wait_id, stage, reason, elapsed, evidence, and reviewer wake present. No missing-field work needed.
- Journal `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` carries exactly one `dev-loop.wait.over_budget` (line 531) and one `dev-loop.wait.reviewer-wake` (line 530) for this wait_id; wait-id dedupe held and one wake was queued.
- Queue window: `dev-loop.control` (line 529) accepted the EXE request at 03:52:53.167Z while game-DLL generation 449 was building (started 03:52:21.727Z, line 525). Gen 449 finished/ready 03:57:18.764Z/03:57:46.107Z (lines 546/549); gen 450 started 03:58:11.654Z (line 561) and finished/ready 04:03:02 (lines 577/578). The EXE build began compiling only at 04:03:03.139Z (`exe-build:10:2026-11-22T04:03:03.139Z`, lines 580-581).
- Outcome: the requested EXE build succeeded; `dev-loop.build.completed` (line 584) published at 04:05:15.090Z, source_generation 10, duration 66.81s, error empty. No build failure occurred.
- First divergence: `docs/specs/dev-loop/change-to-evidence-latency.md:122-127` says an over-budget wait appends later observations, but `devscripts/dev-loop.py:3537-3545` adds the wait_id to `reported_wait_ids` after the first crossing and never re-records it. The issue therefore keeps `elapsed_seconds=60.649s` (first crossing) while the true `build_pending` wait was ~610s (03:52:53 request -> 04:03:03 compile start).
- Root cause of the wait: expected serialization by design. The single EXE-builder lane (`docs/specs/dev-loop/change-to-evidence-latency.md:137`, `dev-loop.md` one EXE build at a time) was occupied by game-DLL builds 449 and 450 when the EXE request arrived. The flat `WAIT_ISSUE_SECONDS=60` (`devscripts/dev-loop.py:109`, applied by `check_wait_budget` at `devscripts/dev-loop.py:3475-3536`) treats queue contention the same as a stalled build, so a queued EXE request behind a multi-minute game-DLL build will always file a build-friction issue.

# Specification alignment

- Followed the router's issue-intake-and-triage, agent-proof-and-verification, change-to-evidence-latency, and agent-coordination guidance.
- The observed behavior matches `docs/specs/dev-loop/change-to-evidence-latency.md:122-127` for issue-and-wake creation, but diverges on appending later observations during the same wait. `docs/specs/dev-loop/agent-build-queue.md:314` still lists the default wait budget as an open review question.

# Exact implementation changes

- None to source. Append-only issue review (id 113, verdict `reviewed-expected-queue-contention`) and one `issue suggest` follow-up on issue-f92715ba3c7f.

# Validation

- Build status: no build requested; no compiler, module worker, or second dev-loop was invoked.
- Evidence read: the dev-loop journal over_budget/reviewer-wake/control/game-build-stage/game-generation-ready/build-completed events, `devscripts/build_friction.py`, `devscripts/dev-loop.py` (`check_wait_budget`, `WAIT_ISSUE_SECONDS`, `reported_wait_ids`), and the change-to-evidence-latency spec.
- Review artifact: issue-f92715ba3c7f review id 113; state now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8.

# Measured evidence

- This occurrence: recorded elapsed 60.649s; true queue wait ~610s (03:52:53 accepted -> 04:03:03 compile start).
- EXE build: published 04:05:15.090Z, request `2026-11-22T03:52:50.598Z-b2290797`, duration 66.81s, error empty.
- Concurrent game-DLL builds during the queue: gen 449 (03:52:21 -> 03:57:46) and gen 450 (03:58:11 -> 04:03:02).

# Regression review

- No regression entry appended. The wait is a correct detection of a >60s queue wait; the recurring cause is queue-contention budgeting plus the missing same-wait append, not a broken EXE build.

# Human acceptance

- Not applicable (build/test wait review). Whether a queue-aware or class-specific `exe-build-queue` budget and same-wait appends are acceptable policy remains a human decision.

# Agent handoff context

- Owner: unassigned; wait-instrumentation owner is `build-runtime-subagents` on issue-e044a931c54b; systemic build-stage/latency owner is `build-coordinator` on issue-136d4553ff92.
- Next exact step: for a still-waiting wait_id, append continued over-budget observations so `elapsed_seconds` reflects the full wait, and consider a queue-aware or class-specific budget for `exe-build-queue` while a game-DLL build holds the single EXE builder.
- Do not redo: this wait-id, wake, queue-window, and journal review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.

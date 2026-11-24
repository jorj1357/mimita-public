# issue-af958c3e10eb: Wait over budget: exe-build-queue at build_pending

## Human report (verbatim, immutable)

> exe-build-queue wait at build_pending exceeded the wait budget


## Scheduled independent review

### 2026-11-24T00:32:55.999Z — damage-lightning-backlog

A bounded independent review was queued as wake wake-34a8a4710c6d after 475 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-expected-queue-contention; wake-queued-once-but-reviewer-declined-as-test-activation; no-review-until-sweep; journal-event-missing; no-fix-claimed)

### 2026-11-24T00:54:40.598Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (issue-review-sweeper, read-only; no source edit, no build, no fix claimed, no file leased).

OBSERVED FACTS
1. Wait occurrence: from the issue evidence and coordination journal seq 2216 (coord.issue.opened 2026-11-20T19:56:23.800Z): wait_id exe-build-queue:105972.877, kind exe-build-queue, stage build_pending, elapsed 60.492s vs 60.0s budget, generation 34, source_generation 34, build_id 2026-11-20T19:55:20.256758Z, request_ids ["2026-11-20T19:52:38.802Z-2695cd08"]. reason holds a "[GAME BUILD] ... jobs=12 ..." progress line, i.e. the EXE request was pending while the game-DLL build compiled, not an idle wait.
2. Record completeness: `issue readiness` = ready true, 8/8, missing []; reproduction, expected_behavior, acceptance, paths, risk, evidence and proof_expectation are present. Unlike sibling issue-191782d7d9c5, related_build and related_run are empty here.
3. Reviewer wake was queued exactly once: .dev/agent-wake/requests/wake-020bc6842761.json created 2026-11-20T19:56:25.198Z for issue-af958c3e10eb (agent build-wait-reviewer), attempts 2, status "failed", error "adapter process exited", fallbacks [], updated 2026-11-20T20:33:55.402Z. No other wake request exists for this wait_id; the only later request is this 2026-11-24 sweep wake-34a8a4710c6d.
4. The wake did not produce a review. Its delivered prompt (.dev/agent-wake/logs/2026-11-20T19-56-25.198Z-build-wait-reviewer.prompt.txt) begins "You were woken by MiMITA agent-wake. This is a bounded test activation." and instructs begin --task "wake test". That run's log ends with the agent writing: "No issue claimed — this is a no-work test activation." The issue therefore stayed state=new with reviewers [] from 2026-11-20 until the periodic sweep at 2026-11-24T00:32:55Z (seq 12607), about 3.3 days / 475 completed sessions.
5. Journal proof is partly unavailable: logs/2026-11-20 does not exist (Test-Path False; 0 entries), so the expected dev-loop.wait.over_budget / dev-loop.wait.reviewer-wake events for this occurrence cannot be read. This is the same "journal-event-missing" class build-wait-reviewer recorded on sibling issue-8cbff41b5590 (2026-11-22T20:29:51.198Z) and overlaps the log-retention scope of issue-3f776ce60511.
6. Same-burst siblings got reviews: issue-723326f11472 (exe-build at setup, wake-b63e0968d7b7) and issue-191782d7d9c5 (game-dll-build, wake-a5dd648492be) both carry build-wait-reviewer reviews (20:14Z and 20:38Z) concluding real CPU/queue contention and a successful build, not a hang. This issue is the only one of the 3-wait burst (opened 19:51:40, 19:56:23, 19:56:26) left unreviewed.
7. Current source no longer emits the mislabel: devscripts/agent-wake.py `_prompt` contains no "bounded test activation", and `python -m unittest devscripts.quality.test_agent_wake -v` passes 3/3 (one test asserts the phrase is absent). Current dev-loop.py ~3889-3906 records dev-loop.wait.reviewer-wake and dev-loop.wait.over_budget after queueing a reviewer wake.

INFERENCE / UNCERTAINTY
- The wait is real and the policy fired correctly (one issue, one wake). The first divergence in the expected behavior "wakes one reviewer automatically" is delivery of the wake: it was presented as a test activation, the reviewer correctly declined, and the request then failed with no effective completion. "Queued exactly once" (acceptance clause 1) was met; wake effectiveness was not.
- The reason text plus the sibling evidence (EXE jobs=12 concurrently with game-DLL jobs=12 on 12 logical CPUs) support contention, not a wedge; build_id 2026-11-20T19:55:20 matches a successful publish time cited by the sibling review.
- Unknown: whether any retry after 2026-11-20T20:33:55 happened outside the request file (none appears in coordination events); and whether logs/2026-11-20 was pruned by retention or never written. A missing sample is unknown, never zero.

CONTRARY EVIDENCE
- The prompt-mislabel defect is fixed in current source (item 7), so this is evidence of a historical delivery gap plus a demonstrated stale-review window, not proof that today's prompt template still mislabels wakes.
- The issue record itself is complete and the wait_id dedupe worked for this occurrence; the actionable gap is wake-to-review completion, not issue creation.

NEXT DISCRIMINATING CHECK
- Owner: unowned (agent-wake / build-friction capability). Smallest check: enqueue one bounded reviewer wake with a real (non-test) prompt and confirm an issue review is appended; then simulate one adapter failure and confirm the request reaches a visible failed/retry state and that the issue is not left `new`. Separately, confirm run-journal retention covers at least the build/test over-budget windows (issue-3f776ce60511) so dev-loop.wait.over_budget is reconstructable.
- Do not close on a build alone. Proof requires both the reviewer-wake record and the dev-loop.wait.over_budget event for the occurrence; today only the wake record and issue record exist.

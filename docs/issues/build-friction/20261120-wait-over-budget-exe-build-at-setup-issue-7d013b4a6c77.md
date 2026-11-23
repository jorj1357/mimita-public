# issue-7d013b4a6c77: Wait over budget: exe-build at setup

## Human report (verbatim, immutable)

> exe-build wait at setup exceeded the wait budget


## Scheduled independent review

### 2026-11-23T11:11:25.361Z — mode3-latency-goal

A bounded independent review was queued as wake wake-05087275f59a after 397 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-open-wake-failed-and-journal-missing)

### 2026-11-23T11:18:47.709Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review of issue-7d013b4a6c77 (wake-05087275f59a), read-only, 2026-11-23T11:11-11:20Z. No source, build, close, or fix performed.

OBSERVED FACTS
1. Intake contract is complete. Issue readiness reports ready=true, missing=[], score 8/8. The issue carries wait_id=exe-build:23:2026-11-20T17:04:44.830Z, kind=exe-build, stage=setup, reason="preparing build", elapsed=60.363s, budget=60.0s, generation=23, source_generation=23, build_id=2026-11-20T07:47:09.981634Z, and three request_ids (17:01:09.699Z-5c9e4334, 17:01:13.433Z-8b36b669, 17:01:26.240Z-a328b20c). Created by dev-loop-wait at 2026-11-20T17:05:45.246Z.
2. "Reviewer wake queued exactly once" holds. .dev/agent-wake/events.jsonl:282 has the only wake.queued for this occurrence (wake-f82615c46512, build-wait-reviewer) at 2026-11-20T17:05:46.144Z; no second wake.queued exists for this issue in the wake or coord stores.
3. The automatic reviewer wake never produced a review. Chain from .dev/agent-wake/events.jsonl: wake.claimed+wake.launched (codex) 17:05:46.8Z; wake.retry 17:18:37.328Z "previous adapter process exited; trying fallback"; wake.claimed+wake.launched (opencode) 18:28:29.6Z; wake.failed 19:05:56.570Z "adapter process exited" (lines 285-286, 294, 304-305, 312). .dev/agent-wake/requests/wake-f82615c46512.json records attempts=2 and status=failed. The retained adapter log tail contains an unrelated "wake-bounded-test" activation, not a review of this issue. The issue therefore had reviews=[] and evidence=[] for ~3d18h until the scheduler appended a pending-review note at 2026-11-23T11:11:25.361Z and queued this review.
4. No dev-loop journal for the occurrence survives. logs/11-20-2026 is empty and no events.jsonl under logs/ contains the wait_id or dev-loop.wait.over_budget for it. The proof_expectation ("Dev-loop wait event plus the issue record and reviewer wake record") is therefore not satisfiable from retained artifacts for this occurrence. Surviving artifacts are the issue-open event (.dev/agent-coord/events.jsonl seq1815), the wake records above, and the adjacent launch report .dev/launch-reports/2026-11-20T17-02-59.374Z. That report shows build_pending=true, source_generation=3, artifact 2026-11-20T07:47:09.981634Z, and a launch failure at client_reconciliation from an 8s PowerShell process-scan timeout, i.e. the wait occurred inside a build-pending/launch window, not an idle loop.
5. Build/run identity is only partially carried. The recorded build_id=2026-11-20T07:47:09.981634Z is the last-published EXE artifact (confirmed as artifact_id/build_id by artifact_selection and artifact_validation in the 17:02:59 launch report), not an in-flight exe-build attempt id, and no run_id is stored. This is the identity-lag class already documented for game-DLL waits, here applied to the exe-build kind.
6. The condition recurs with an unchanged policy. Seven issues carry the exact title "Wait over budget: exe-build at setup": 723326f11472, dff7e8c0f758 and this issue on 2026-11-20; 17abc587c89a, 34969db31bf6, 38dd439ad62c, 8aae0621645f on 2026-11-22. devscripts/dev-loop.py:109-110 still uses a flat WAIT_ISSUE_SECONDS=60.

INFERENCE / UNCERTAINTY
- The 60.363s crossing (0.363s over budget, reason "preparing build") came 1m45s after a launch that had build_pending=true, so it is more consistent with normal EXE build/setup latency than with a stall. I cannot confirm that the underlying build was healthy or how long it took because its journal is gone; this remains inference.
- The issue-creation half of the automation worked end to end (one issue, one wake queued). The review half failed adapter-side, twice, so this is a wake-delivery/reliability defect, not a missing hook and not evidence that the wait policy is fixed.
- The exact cause of the two adapter exits is not determinable from retained artifacts; the shared log path does not contain an issue-review attempt.

CONTRARY EVIDENCE
- The acceptance clause "reviewer wake is queued exactly once" is met at 17:05:46.144Z, and the queue did launch; the failure was adapter-side, so the hook is not inert.
- The overage was marginal (0.363s), so this specific occurrence is weak evidence of real build friction.
- All required intake fields except run_id and in-flight build identity are present, and readiness is fully ready.

NEXT DISCRIMINATING CHECK
- On the next exe-build-at-setup occurrence (recurrence is demonstrated: four on 2026-11-22), read that run's dev-loop/events.jsonl and confirm: (a) dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake are present; (b) the recorded generation/build_id match the in-flight build attempt; (c) whether that build completed and its total duration; (d) whether the reviewer wake request ends completed vs failed. That separates a false-positive 60s budget (build succeeds shortly after crossing) from a real stall, and shows whether the wake failure reproduces.
- Do not close this issue; no fix is claimed.

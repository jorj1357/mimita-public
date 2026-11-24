# issue-e96486c8cc3e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Scheduled independent review

### 2026-11-24T17:08:28.466Z — vehicle-rl-physics

A bounded independent review was queued as wake wake-24722249f116 after 593 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed-policy-unchanged-identity-gap)

### 2026-11-24T17:44:08.436Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-24722249f116), read-only, 2026-11-24T17:1xZ. No source edited, no build/test started, no fix claimed, no close.

BRIEF / READINESS / PROOF STATUS (observed)
- issue readiness: ready=true, score 8/8, missing=[], state=new, tasks=0; reconsideration fields reproduction, expected_behavior, acceptance, paths, proof_expectation, evidence are all present.
- proof list --issue issue-e96486c8cc3e: [] — no proof record, no proof task, no exemption.
- Acceptance for this occurrence: wait_id, stage, reason, elapsed, build/run IDs and evidence present, and exactly one reviewer wake for the wait occurrence.

OCCURRENCE RECORD (retained artifacts)
- Coord evidence: budget_seconds=60.0, elapsed_seconds=60.281, kind=game-dll-build, stage=game_build, source_generation=4, request_ids=[auto-4], started_utc=2026-11-21T03:10:13.609Z, wait_id=game-dll-build:2026-11-21T03:10:13.609Z, build_id=2026-11-21T02:26:50.306117Z, generation=null.
- reason embedded in evidence: [GAME_STAGE] codegen:packet_registry_codegen.py started 2026-11-21T03:10:45.012Z, elapsed_ms=31060.312, build_attempt_id=game-dll-20261121T031013951147Z-cdd78c.
- .dev/build-timings/game-dll-20261121T031013951147Z-cdd78c.jsonl: build.started 03:10:13.952Z; stage codegen:resident_state_codegen.py started 03:10:16.623Z, completed 03:10:43.426Z (26.80s, exit 0); stage codegen:packet_registry_codegen.py started 03:10:45.012Z, failed 03:15:53.955Z (308.94s, exit_code=124); build.failed 03:15:55.536Z, failed_phase=codegen_timeout, error="codegen timeout: packet_registry_codegen.py", total 341.59s.
- At issuance (~03:11:13.9Z) the attempt was ~28.9s into packet_registry_codegen, after two stages had completed with exit 0: actively progressing, not stalled, when the 60s budget fired.

ACCEPTANCE RESULT
1. Field presence: PASS (wait_id, stage, reason, elapsed, budget, source_generation, request_ids, evidence).
2. Build/run identity: PARTIAL / FAIL. generation=null; build_id=2026-11-21T02:26:50.306117Z is the latest EXE build identity, not this game-DLL attempt; build_attempt_id appears only inside the free-text reason; run_id is absent. Same identity defect already recorded for this class on issue-9a4503f6ed68 (2026-11-22 review) and re-confirmed on issue-3472692a57c2 (2026-11-24T12:55Z review), so this is a third independent instance.
3. Reviewer wake exactly once: PASS. Exactly one wake.queued for this wait_id: wake-6b00f2b0af56 at 2026-11-21T03:11:23.488Z (.dev/agent-wake/events.jsonl L480). The 2026-11-24T17:08:28 wake-24722249f116 is the scheduled independent review, not a per-occurrence wake.

WAKE DISPATCH (separate defect, same occurrence)
- wake.claimed (codex) 03:48:07.456Z, wake.launched 03:48:08.055Z; codex log shows model-cache failure 03:49:22-23Z; wake.retry -> opencode 13:50:41.009Z; wake.claimed (opencode) 15:08:10.228Z, launched 15:08:10.255Z; wake.failed 15:20:49.982Z "adapter process exited" (.dev/agent-wake/events.jsonl L480/518/519/548/638/639/687; requests/wake-6b00f2b0af56.json status=failed, attempts=2).
- The reviewer session log for this wake shows the session opening this issue at 15:14:44Z and dying at 15:18:08Z with "Unexpected server error" (ref err_9dbc78ec) before appending any review.
- So queue contract held but dispatch did not: 36m44s to first claim, ~10h01m retry gap, then a crashed fallback session; 12h09m elapsed with zero review written. This is evidence for issue-84c584957e65 (late dispatch), not for the wait hook.

EVIDENCE GAP (missing sample is unknown, not zero)
- logs/11-21-2026 exists but is empty. The 2026-11-21 dev-loop run journal that would hold dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake for this wait is not retained, so the stated proof expectation ("Dev-loop wait event") cannot be re-inspected for this occurrence. Retained: coord issue-opened event (seq 2939), the wake events/request, and the build-timings record. The hook may have written the events correctly; the evidence is gone, so this occurrence cannot be used to prove or disprove the journal half of acceptance.

CONTRARY EVIDENCE (do not overstate "false positive")
- This occurrence's attempt did fail, but later and for a different reason: packet_registry_codegen hit its own 300s timeout at 308.9s (exit 124), 4.7 minutes after the 60s wait. The wait fired during healthy progress and did not predict that failure.
- A stall-based budget would not have caught this attempt earlier with per-stage granularity: packet_registry_codegen emits no timeline event between start and timeout. So the naive "progress detection fixes it" recommendation is insufficient for this stage; the codegen timeout is already the true stall guard there.

RECURRENCE / CURRENT CODE (observed 2026-11-24)
- The coordinator store now holds 193 "Wait over budget" build-friction issues (175 in_review, 17 new, 1 assigned). Newest game-dll occurrence: issue-d82b02478dff created 2026-11-24T17:13:14Z, elapsed=63.935s, reason "starting whole-game build" — same flat-budget false positive three days later, still minting one issue plus one wake per attempt. A further same-class review at 17:43Z (issue-ee437972915e) documents the policy unchanged plus an agent-wake subprocess-timeout defect in build_friction.py:237-246.
- devscripts/dev-loop.py:114-115 (WAIT_ISSUE_SECONDS default 60, applied to every kind), :3884-3900 (game-dll-build flagged on total stage elapsed), :3896-3897 (build_id=self.latest_build, generation=self.game_build_generation) are unchanged: no progress/stall test and no in-flight identity in the wait record.
- Duplicate classification: this issue is one of the per-attempt duplicates already triaged on issue-450cfc80a0b9 (reviewed-needs-policy-fix) and owned in aggregate by issue-9a4503f6ed68. This issue's own occurrence contract mostly held; the defect is policy plus identity, not a per-occurrence hook failure. Do not fix or close this issue independently.

INFERENCE / UNCERTAINTY
- The 10h retry gap matches the "no daemon drains the fallback" pattern on issue-84c584957e65; I did not measure the exact daemon idle interval, so that cause is inference, not proof.
- The build_resource_gaps returned by brain_check (44a127, 968a7e, 8723b2) are resource-sampling gaps unrelated to this wait class and are already tracked on issue-136d4553ff92 / issue-ed3ac775283d, so no duplicate gap was appended here.

NEXT DISCRIMINATING CHECK
- After a stage-aware/progress-aware (or explicitly waived) game_build budget lands, request one real game-DLL build and confirm: (a) zero new "Wait over budget: game-dll-build at game_build" issues while the attempt timeline advances; (b) wait evidence carries the in-flight build_attempt_id, generation and run_id; (c) a genuinely stalled attempt (>N s with no timeline or stage advance) still creates exactly one issue and exactly one wake.
- For this occurrence specifically, no further check is possible: the run journal is gone. Keep the issue open as evidence of the per-attempt duplication, attached to the policy owner.

Verdict: reviewed / occurrence hook contract mostly held (fields and single wake), policy false positive unchanged and recurring, build/run identity incomplete, durable journal evidence missing. Requires policy fix (issue-9a4503f6ed68) and wake-dispatch fix (issue-84c584957e65); no fix claimed. Confidence: high on retained records, medium on the dispatch-gap cause.

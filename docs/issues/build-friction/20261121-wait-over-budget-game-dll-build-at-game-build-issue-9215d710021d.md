# issue-9215d710021d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-21T16:10:30.903Z — visual-access-and-issue-review

Occurrence resolution (2026-11-21, ctx-f9ce8d89a650 "why build stuck at 18 min?"): the wait was real but the build was not hung. Attempt game-dll-20261121T150703129448Z-23d2a4 progressed through every phase; ~9.2 min was pre-compile accounting (packet_registry_codegen 56.3 s, manifest_snapshot 77.4 s, source_snapshot 110.2 s, changed_source_preflight 128.0 s, object_cache_scan 180.8 s) and compile then failed at 12.5 min with 4 cc1plus out-of-memory errors at jobs=12. It auto-retried as gen 423, which hit object_cache_scan 469.9 s and 280-363 s per TU. Recommendation lives in issue-136d4553ff92 and issue-2a5c5da53565. This over-budget issue should link that evidence rather than duplicate it.

## Scheduled independent review

### 2026-11-26T00:06:21.891Z — root-resource-loop

A bounded independent review was queued as wake wake-87bed04b95ae after 752 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (evidence-gap)

### 2026-11-26T00:14:07.299Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review 2026-11-26 by issue-review-sweeper (wake-87bed04b95ae). Read-only: no source edit, no build requested by this agent, no fix or close claimed. Examined issue brief/readiness, proof status, the issue doc, .dev/agent-wake events/request, .dev/build-timings, the live root dev-loop --status, the 2026-11-25/26 dev-loop journal run 20261125_234424, and source owners dev-loop.py/build_friction.py/agent-wake.py. This is the first appended review on this issue (reviews was empty).

OBSERVED FACTS
- Brief/readiness: ready=true, score 8, state=new, owner empty, tasks 0, reviews 0 before this entry. Created 2026-11-21T15:08:03.031Z by dev-loop-wait, kind build-friction, priority high.
- Proof status: zero task-proof rows reference issue-9215d710021d (no task). The declared proof_expectation is three artifacts for one wait occurrence: (a) dev-loop wait event, (b) issue record, (c) reviewer wake record.
- (b) ISSUE RECORD PRESENT. Evidence carries wait_id=game-dll-build:2026-11-21T15:07:02.370Z, kind=game-dll-build, stage=game_build, elapsed_seconds=60.47 vs budget_seconds=60.0, request_ids=["auto-2"], source_generation=2, generation=420, build_id=2026-11-21T02:26:50.306117Z. reason is a "[GAME_STAGE]" codegen row embedding build_attempt_id=game-dll-20261121T150703129448Z-23d2a4.
- (c) WAKE QUEUED EXACTLY ONCE, BUT NEVER DELIVERED. .dev/agent-wake/events.jsonl line 606: wake.queued wake-4e70fc4a272b, agent build-wait-reviewer, issue issue-9215d710021d, 2026-11-21T15:08:04.408Z. No other wake.queued exists for this issue (the only later one is this sweeper wake-87bed04b95ae). Same wake then: wake.claimed/launched codex 15:08:09.296/.906Z; wake.retry "previous adapter process exited; trying fallback" to opencode 15:20:49.434Z; wake.claimed/launched opencode 15:26:34.601/.609Z; wake.failed "adapter process exited" 15:53:25.984Z. .dev/agent-wake/requests/wake-4e70fc4a272b.json confirms status=failed, attempts=2, error="adapter process exited". The issue received no reviewer review between 2026-11-21 and this 2026-11-26 scheduled sweep (~5 days). The adapter log path .dev/agent-wake/logs/2026-11-21T15-08-04.403Z-build-wait-reviewer.log and logs/2026-11-21 are both absent now, so the adapter failure cause is unrecoverable from this checkout.
- (a) DEV-LOOP WAIT EVENT NOT RECONSTRUCTABLE. No retained artifact contains the original dev-loop.wait.over_budget row for this wait_id (rg over logs and .dev finds only .dev/tmp-issues.json and the wake request). logs/2026-11-21 no longer exists (rotation). The journal half of the declared proof is therefore unknown, not confirmed and not disproven.
- FIELD ACCURACY GAP REPRODUCES. The dedicated build_id is the last PUBLISHED artifact, not the in-flight attempt: dev-loop.py:3896 sets build_id=self.latest_build, and latest_build is the published artifact id (dev-loop.py:1404-1408 on startup, :4167 on publish). The true attempt id is only embedded in free-text reason (dev-loop.py:3892 reason=self.game_build_detail or last_message). Same pattern on 2026-11-25/26: issue-6d572fc03e97 wait_id game-dll-build:2026-11-25T23:50:11.848Z has build_id=2026-11-25T23:04:22.955793Z while its reason contains build_attempt_id=game-dll-20261125T235012012371Z-a9ad0a; issue-af48c43ce6c5 wait_id ...00:03:44.421Z has build_id=2026-11-25T23:04:22.955793Z. There is no dedicated in-flight attempt/run id field.
- REASON QUALITY IS INCONSISTENT. reason is the raw last stage detail. For this issue it is informative; for newer occurrence issue-af48c43ce6c5 it is a truncated mid-token JSON fragment beginning "\"p threading\",\"raw\":..." and ending "record_overhead_ms" (compiler optimization output), so the occurrence's stated reason is not independently readable.
- EXACTLY-ONCE ISSUE DEDUP STILL HOLDS. dev-loop.py:3901-3911 dedups by wait_id via reported_wait_ids; build_friction.py:131-140 _wait_issue matches evidence.wait_id. Live journal 20261125_234424 records one dev-loop.wait.over_budget + one dev-loop.wait.reviewer-wake per occurrence: wait ...23:50:11.848Z -> issue-6d572fc03e97 (created=true), wait ...00:03:44.421Z -> issue-af48c43ce6c5 (created=true), each with wake queued=true. No duplicate issue for the same wait_id observed.
- OVER-BUDGET WAITS ARE CURRENT AND ROUTINE: recent game-dll builds reached 60.4-68.9s and created new per-occurrence issues. Current live status shows generation-658 build total_seconds=98.563 on .dev/build-timings/game-dll-20261126T000344661706Z-d96bf3.json, so the >60s class persists.

INFERENCE / UNCERTAINTY
- Acceptance "issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence" is only partly met. wait_id/stage/elapsed/evidence are exact; build_id is misleading for this occurrence (previous published artifact), the true attempt id is buried in reason, and reason is not bounded. Confidence high (source + three occurrences agree).
- Acceptance "a reviewer wake is queued exactly once" is structurally met (one wake.queued) but not effectively met: exactly-once queueing did not yield dispatch. A terminal adapter failure left the issue unread and nothing re-queued it, so a reviewer only ran when the scheduled sweeper fired five days later. Confidence high for the event sequence; the root cause of the adapter exit is unknown because the log is gone.
- Whether (a) was written-then-rotated or never written (cf. issue-6e2d7b91e7d9 dropped-journal case) is unknown. Do not treat the absence as proof of either.
- The per-occurrence issue policy is functioning as specified, but normal builds now exceed 60s often; whether to keep one issue per wait_id or append occurrences to a rolling issue is a product/owner decision, not a bug.

CONTRARY EVIDENCE
- This was real friction, not a false positive: the prior suggestion on this issue (visual-access-and-issue-review, 2026-11-21) established attempt game-dll-20261121T150703129448Z-23d2a4 progressed through every phase and failed at ~12.5 min with 4 cc1plus OOM at jobs=12, then auto-retried gen 423. Time-to-first-visible-progress exceeded 60s.
- Wake dispatch is not wholly broken today: 2026-11-25/26 occurrences queued and launched wakes (wake-80862c08c2b5, wake-ec92bcb2152d; queued=true). The 2026-11-21 failure is an adapter exit, and late/duplicate fallback dispatch is already tracked in issue-84c584957e65.

NEXT DISCRIMINATING CHECKS (ordered)
1. Add a dedicated in-flight build_attempt_id (and run_id when a run exists) to the wait record from the active game-build request, separate from last-published build_id; verify on the next over-budget occurrence that both fields are present and distinct.
2. Bound/normalize reason to one readable stage line with a max length so it can never be a mid-JSON fragment; verify on the next occurrence.
3. On terminal wake failure, append an occurrence/status to the linked issue and re-queue once; measure that a deliberately failed adapter produces an issue-visible failure record within the retry window and no multi-day silent gap.
4. Retain or mirror the dev-loop.wait.over_budget row into the issue/coord record so the three-part proof survives log rotation.
5. Owner decision on per-occurrence issue volume (one issue per wait_id vs rolling occurrence on one open stage issue); do not change dedup without a decision.

This review changes no task state and claims no fix. The issue remains open, new, high priority, unowned; the exactly-once queue property holds, but evidence completeness (job A: retained wait event; job B: accurate in-flight ids; job C: failed-wake visibility) is still a gap.

## Agent discussion and suggestions

### 2026-11-26T00:14:43.984Z — issue-review-sweeper

Message: `msg-b947ad917dab` (question)

Subject: Wake failure evidence on issue-9215d710021d

Bounded review appended to issue-9215d710021d (verdict evidence-gap). For this over-budget wait occurrence, wake-4e70fc4a272b was queued exactly once but both adapters exited (wake.failed 2026-11-21T15:53:25Z, attempts=2), so no review landed until the 2026-11-26 scheduled sweep. I appended a terminal-failure proposal to issue-84c584957e65. Next check: make terminal wake failure append an issue-visible occurrence and re-queue once; also add a dedicated in-flight build_attempt_id to the wait record so build_id is not the last published artifact. Please review and claim if you agree.

# issue-25b14f436a3e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (evidence_reviewed)

### 2026-11-20T17:13:20.294Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait occurrence game-dll-build:2026-11-20T16:12:47.255Z. Evidence: .dev/build-timings/game-dll-20261120T161247800566Z-4a70f1.jsonl shows build.started 16:12:47.801Z (source_generation 19; request_ids 2026-11-20T16:08:16.089Z-befd0a6d and 2026-11-20T16:09:28.468Z-1bb65630); stage codegen:resident_state_codegen completed in 3.07s; stage codegen:packet_registry_codegen started 16:12:53.237Z and failed 16:14:36.043Z after 102.806s (exit 1); build.failed 16:14:54.163Z total 126.362s. Summary .json error=codegen check failed: packet_registry_codegen.py. Issue evidence matches this attempt (elapsed 101.105s, generation 405, source_generation 19), so the wait is real and the wait_id/stage/reason/elapsed/request_ids fields are complete. Root cause: packet_registry_codegen.py --check found stale generated artifacts (src/host/generated/packet-registry-v1.h and/or config/hot-reload/packet-ledger.json) relative to src/network/packets.h and dispatch sources; the 102.8s is machine load, not the check itself. A read-only rerun now returns '[PACKET CODEGEN] ok packets=78' exit 0, so the stale window has closed. Distinct occurrence, not a duplicate: 63dccbd8f8b6=08:31:55Z, 2008e743c710=08:45:25Z, this=16:12:47Z, b0ba8698acd7=16:38:24Z. Separate friction: concurrent packet-source edits can trip the stale-codegen guard and fail the whole build, and this reviewer wake was created 16:14:58Z but launched 16:39:30Z (2 attempts, about 24.5 min latency), so the wait-issue loop is itself slow. No source edits made by reviewer.

## Scheduled independent review

### 2026-11-23T11:47:08.830Z — resource-efficiency-goal

A bounded independent review was queued as wake wake-e29ceddadac4 after 399 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (evidence_reviewed_systemic_identity_and_noise_defects_open)

### 2026-11-23T11:53:14.091Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-e29ceddadac4), read-only, 2026-11-23T11:47-12:05Z. No source edit, build, close, or fix performed.

OBSERVED FACTS
1. The wait is real and the pinned attempt is correctly identified. .dev/build-timings/game-dll-20261120T161247800566Z-4a70f1.jsonl shows build.started 2026-11-20T16:12:47.801Z (source_generation 19; request_ids 2026-11-20T16:08:16.089Z-befd0a6d, 2026-11-20T16:09:28.468Z-1bb65630); codegen:packet_registry_codegen.py started 16:12:53.237Z and failed 16:14:36.043Z after 102.806s (exit 1); build.failed 16:14:54.163Z with error codegen check failed: packet_registry_codegen.py. This matches the issue evidence (elapsed 101.105s, generation 405, reason) and exceeded the flat 60s budget.
2. A reviewer wake was queued once for this wait occurrence: .dev/agent-wake/events.jsonl wake-f12675d71a59 queued 2026-11-20T16:14:58.370Z. It was first claimed/launched 16:15:02.3-02.5Z (codex adapter), retried at 16:22:34.498Z, then claimed/launched again 16:39:30.9Z (opencode fallback); its review was appended at 17:13:20.293Z. The same wake later logged wake.failed at 18:28:29.511Z (adapter process exited) even though the reviewer output already existed. So queued-exactly-once holds, but the wake lifecycle mislabels a wake that produced a review as failed, and an adapter exit forced an avoidable ~24 min fallback gap.
3. Acceptance field build/run IDs is only partly met. evidence.build_id = 2026-11-20T07:47:09.981634Z is a previous EXE artifact, not the in-flight game-DLL attempt game-dll-20261120T161247800566Z-4a70f1. Cause is code: dev-loop.py:3756 sets the game-dll-build wait build_id from self.latest_build, which is assigned only on completed EXE builds (dev-loop.py:1387 startup, 4027 completion). This is the same identity defect an independent review recorded for the sibling improvement issue-9a4503f6ed68 (review id 217).
4. This is not an isolated occurrence. As of 2026-11-23T11:5xZ the coordinator holds 152 open Wait over budget issues of kind build-friction (134 in_review, 17 new, 1 assigned), 130 titled Wait over budget: game-dll-build at game_build. The most recent (issue-cd97259fdc20, 2026-11-22T23:53:23Z) repeats the wrong identity (build_id 2026-11-22T21:32:48.195707Z, generation 545, source_generation 27) and its reason field is a truncated JSON fragment rather than a coherent message.

INFERENCE / UNCERTAINTY
- This issue's wait and evidence are genuine; it is one instance of the systemic per-attempt-noisy plus wrong-identity problem already tracked in issue-9a4503f6ed68, not a broken hook.
- The 102.8s codegen failure was earlier attributed to stale generated packet-registry artifacts under machine load; that stale window closed, but the same packet_registry codegen check failure recurred four times on 2026-11-22T23:28-23:49Z, so the stale-artifact/whole-build-fail friction is not fixed. That belongs to the stale-codegen path, not to this per-wait record.
- I did not reproduce the adapter exit; the wake.failed-after-review inference rests on event ordering, not on inspecting the adapter child.

CONTRARY EVIDENCE
- Automation worked end to end for this wait: one issue, one reviewer wake, durable dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake events, and a reviewer append.
- Per-wait-ID uniqueness is intentional in build_friction.record_wait_incident; distinct wait_ids are meant to create distinct issues, so per-attempt growth is a policy outcome, not necessarily a duplicate-ID bug.

NEXT DISCRIMINATING CHECK
1. Have the dev-loop.py owner decide whether a game-dll-build wait must carry the in-flight attempt id (game-dll-ATTEMPT) rather than self.latest_build; then verify a fresh wait evidence generation/build_id equals its dev-loop.game-build.started build_attempt_id and result generation.
2. Confirm in agent-wake.py run_once/reconcile (around lines 415-481) whether a launched request whose adapter exits after writing output should reconcile to completed instead of failed; measure queue-to-launch gap for game-dll-build reviewer wakes before/after.
3. Aggregate dedupe by (kind, stage, normalized reason) and the queue-to-launch target remain owned by issue-9a4503f6ed68; do not mint additional sibling issues for this same class.

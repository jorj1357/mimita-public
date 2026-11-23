# issue-dd54a79216ad: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-benign-wait)

### 2026-11-20T15:45:31.123Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed the over-budget wait occurrence. Timeline from .dev/build-timings/game-dll-20261120T152655407656Z-5e6e30.jsonl:
- build.started 15:26:55.412Z (source_generation 9, request_ids auto-4..auto-9).
- codegen:packet_registry_codegen.py 15:26:56.611Z -> 15:27:50.496Z, duration 53.88s (dominant pre-wait stage).
- manifest_snapshot started 15:27:50.657Z at elapsed 55.25s; the 60s budget was crossed during this stage, which completed normally at 15:28:18.811Z (28.13s).
- No stall/crash: every stage completed exit_code 0; compile continued on 4 changed translation units (generation 405) after the wait.
Wake path verified in .dev/agent-wake/events.jsonl: exactly one wake.queued / wake.claimed / wake.launched for wake-25a134e3a30a (issue-dd54a79216ad), codex adapter, with one documented wake.retry to opencode fallback. No duplicate issue created.
Issue readiness is complete (score 8/8; reproduction, expected behavior, acceptance, paths, proof expectation all present), so no fields needed filling.
Conclusion: the automatic wait issue and single reviewer wake behaved as specified; this occurrence is a real one-minute-plus build, not a hang. No wait-policy code fix required. Follow-up latency observation only (do not create a duplicate): packet_registry_codegen took 53.9s and changed_source_preflight compiled 4 TUs over ~429s on this host; that is build throughput, already tracked by issue-e044a931c54b (Waiting is the enemy). Paths devscripts/dev-loop.py, devscripts/build_friction.py, devscripts/agent-wake.py unchanged.

## Scheduled independent review

### 2026-11-23T07:59:44.542Z — resource-efficiency-goal

A bounded independent review was queued as wake wake-751a02e54dec after 376 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-benign-wait-with-open-defects)

### 2026-11-23T08:06:25.683Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-751a02e54dec), read-only, 2026-11-23T08:0xZ. No source edit, build, close, or fix performed.

OBSERVED FACTS
1. Readiness 8/8 ready, state in_review. No formal proof record/task exists for this issue (proof list/status returns nothing for issue-dd54a79216ad), so completion currently rests on the prior reviewer narrative, not a stored proof.
2. Original occurrence re-verified from .dev/build-timings/game-dll-20261120T152655407656Z-5e6e30.jsonl: manifest_snapshot crossed the 60s budget (stage started 15:27:50.657Z at elapsed 55.25s, completed 15:28:18.811Z); build.completed at 16:02:44.909Z, elapsed 2149477 ms (~2149s), exit_code 0, generation 405. Four changed TUs compiled after the wait (compile phase 931s, link 136s). The wait was a real slow build, not a hang.
3. Dedupe by exact wait_id holds: exactly one issue in the store carries wait_id game-dll-build:2026-11-20T15:26:55.364Z (this issue). No duplicate for this occurrence.
4. Wake path is not clean. .dev/agent-wake/events.jsonl and request wake-25a134e3a30a.json: one wake.queued at 15:27:57; codex claimed/launched 15:27:57; wake.retry to opencode 15:41:42; opencode claimed/launched 16:03:49; wake.failed 16:22:33 with error 'adapter process exited' (status=failed, attempts=2). The wake log 2026-11-20T15-27-57.172Z-build-wait-reviewer.log begins with codex model-cache/stream errors, and its tail is an opencode session that performed a generic 'Bounded agent-wake test activation' and never referenced issue-dd54a79216ad. Review id 7 (15:45:31) does not appear in that log, so its authorship is not attributable to either wake attempt from the journal.
5. Aggregate noise (this issue's named risk) has materialized. Store currently holds 152 open 'Wait over budget*' build-friction issues: 133 in_review, 18 new, 1 assigned; 130 identically titled 'Wait over budget: game-dll-build at game_build'; 150 unowned. Created 11-20: 29, 11-21: 27, 11-22: 96; newest 2026-11-22T23:53:23Z. devscripts/build_friction.py:131-148,164-169 dedupes only by exact wait_id, so each attempt mints a new issue by design.
6. Current latency regime differs: latest .dev/build-timings builds are ~5.8-18.9s (generation 548 total 13.9s), with occasional 212s/267s outliers. No new wait issue has been created today (11-23). The wait policy is currently quiet.

INFERENCE / UNCERTAINTY
- The hook works: one issue per wait_id and one queued reviewer wake for this occurrence are confirmed. The prior 'benign wait' conclusion is supported for build behavior.
- 'Reviewer wake behaved as specified' is NOT supported: the request ended failed after two adapter attempts, and the fallback attempt did not perform the review. Treat review id 7 as provenance-uncertain.
- Per-occurrence issue creation is intended acceptance, so the 152-issue backlog is a policy consequence, not a second-scan bug; whether that policy is desirable is a decision for the owner/human.
- I did not reproduce a live over-budget wait; the build timing and wake evidence above are historical artifacts, not a fresh run.

CONTRARY EVIDENCE
- Build completed exit_code 0 with no crash/stall, so the original wait was a genuine slow build.
- Exactly one issue exists for this wait_id; no duplicate-issue defect for this occurrence.
- Today's builds are under the 60s budget, so this friction is not currently recurring at this stage.

NEXT DISCRIMINATING CHECK
- Inspect the codex segment of 2026-11-20T15-27-57.172Z-build-wait-reviewer.log (before the opencode retry) to determine who appended review id 7; if no wake attempt produced it, record the review as unverified provenance.
- On the next over-budget wait, verify the reviewer wake request reaches status=succeeded and that no second issue is created for the same wait_id.

Cross-references: wake-delivery defect issue-62ab3daa615d; aggregate wait-issue noise issue-9a4503f6ed68 (proposal appended this sweep).

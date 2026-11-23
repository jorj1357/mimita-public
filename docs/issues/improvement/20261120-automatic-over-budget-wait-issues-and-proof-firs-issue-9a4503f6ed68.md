# issue-9a4503f6ed68: Automatic over-budget wait issues and proof-first waiting work

## Human report (verbatim, immutable)

> no dev-loop hook creates an issue automatically - need ot do this. More work toward the wake agent so that agents are testing and proving their things, their changes, and putting evidence. If waiting they do the missing issues that are missing fields or missing evidence.


## Task linked

### 2026-11-20T08:04:15.057Z — wake-proof-wait

Task: wait-proof-wake (step 0)

## Review (pass)

### 2026-11-20T08:12:16.978Z — wake-proof-wait

Reviewer: wake-proof-wait

Tooling slice implemented and unit-tested. Missing-context and missing-evidence waiting work returns correctly; 61s fake dev-loop wait creates one issue and one reviewer wake. Live over-60s dev-loop occurrence still required before claiming behavior completion.

## Agent suggestion

### 2026-11-21T00:02:12.072Z — build-wait-reviewer

Suggestion (from reviewing issue-f9ea4f30ad18, no source edit): make over-budget waits signature-deduped and stall-based. Evidence: 29 open Wait-over-budget issues today, 21 title-identical 'game-dll-build at game_build' (11 in_review, 10 new), each with a reviewer wake. dev-loop.py:3393 keys wait_id to game_build_started_utc and build_friction.py:131-148 looks up that exact wait_id, so every new build attempt creates a new issue + wake; the 60s budget (dev-loop.py:3389-3399) has no progress check. In the reviewed occurrence the build was actively progressing (manifest_snapshot 23:43:40.514Z -> 23:48:43.438Z), and current.json shows the prior published build total_seconds=618.2s, so 60s elapsed cannot distinguish progress from a stall. Proposed: (1) raise/derive per-stage budgets from build-timing history; (2) only file/wake when no stage/progress event for N seconds (stall), not on total elapsed; (3) dedup by kind+stage+phase signature with occurrences appended and a cooldown, so one issue/wake per repeated signature instead of per attempt. This is a shared-path change; owner approval and a build/test proof are required.

## Agent suggestion

### 2026-11-21T00:06:38.471Z — build-wait-reviewer

Second evidence point on the same automation (issue-0322fa45b0e8): wake-15cdbf8cf0e0 was created 2026-11-20T20:59:52Z but launched 2026-11-20T23:44:04Z, while its build attempt game-dll-20261120T205850979297Z-7cdcb2 had already succeeded at 21:08:14Z (total 563.53s). The reviewer activation was ~2h44m stale and could not observe the wait. Proposed: expire or re-scope queued waits whose build/run already ended (fail the wake with reason instead of launching), and never launch a reviewer wake for an issue whose wait is resolved. Evidence: .dev/agent-wake/requests/wake-15cdbf8cf0e0.json (created/launched timestamps) and .dev/build-timings/game-dll-20261120T205850979297Z-7cdcb2.json (success=true, completed_utc).

## Agent suggestion

### 2026-11-21T03:58:14.386Z — build-wait-reviewer

Duplicate and leaked wake activations observed 2026-11-21T03:1x-03:5xZ, no source edits. (1) issue-f9ea4f30ad18 wait game-dll-build:2026-11-20T23:42:59.686Z was reviewed pass at 00:01:50, but wake-03de8d93cd0e kept being claimed and relaunched: attempts=3 (codex launch 23:44:04 pid 33744; opencode claims 01:40:28 and 02:30:36; opencode launch 02:30:37 pid 28280, child 37004 still running at 03:50Z). No guard consults the issue review state or a prior successful activation. (2) Stale wake child processes are not reaped: wake-601d4df22bf1 (arcade-collision-elite-proof, launched 23:44:04, wrapper cmd 27432) still had opencode child 33216 alive at 03:20Z (216 min); wake-f0838de769a8 (root, launched 00:13:33, wrapper cmd 25932) still had opencode child 28600 alive (187 min). Both still consume CPU. (3) At 03:11:30-03:11:50Z six wakes launched within 20s, including two concurrent sessions per agent identity: fcs-npc-live-fix (16354ab22ef8, 1f9e04a64eae), root (1ba970858b57, 36e357e64eb6), arcade-collision-elite-proof (361a4098315d, 532c12bc0337). Dedup is per issue/agent, not per agent, so concurrent same-agent sessions are allowed. Suggested direction: treat in_review/pass issues as non-wakeable for the same wait signature; reap the full child process tree (cmd wrapper and opencode) on exit or max age; dedupe queued/launched wakes by agent and issue at launch; add a heartbeat so wedged sessions become failed with a reason. Evidence paths: .dev/agent-wake/requests, .dev/agent-wake/events.jsonl, devscripts/agent-wake.py launch and run_once.

## Agent suggestion

### 2026-11-21T14:23:21.316Z — build-wait-reviewer

Live occurrence issue-9aeec1fbae9d exposed a defect in this feature. Detection and issue/wake creation succeeded, but dev-loop logged dev-loop.wait.issue-failed at 2026-11-21T01:26:17 because agent-wake.py run --once timed out after 30s; no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake event was recorded, and the reviewer did not launch until 2026-11-21T13:50:40 (44,700.5s after the wake was queued).

Cause: devscripts/build_friction.py:244-246 synchronously runs agent-wake.py run --once with timeout=30 inside the wait handler. devscripts/dev-loop.py:3458 marks the wait_id reported before that launch, and dev-loop.py:3450-3479 catches the timeout as "could not record over-budget wait". Recommendation: enqueue and return (or launch detached), record over_budget/reviewer-wake before any adapter launch, and report adapter launch failure separately. Recheck the existing "proven" claim against this live occurrence.

## Agent suggestion

### 2026-11-21T16:08:30.354Z — visual-access-and-issue-review

Deduplication evidence (2026-11-21): despite the acceptance "a second scan does not duplicate the issue", the board currently holds 23 open `Wait over budget: game-dll-build at game_build` issues (12 in_review, 11 new) plus 9 others, 32 wait-over-budget issues total. Examples: issue-9215d710021d, issue-121f867b430a, issue-9aeec1fbae9d, issue-ad873eff97f3, issue-450cfc80a0b9, issue-f9ea4f30ad18. They are separate waits (different wait_ids) but identical kind/stage, and the risk field already warned about noisy duplicate friction issues. Suggest coalescing by kind+stage (one rolling issue with occurrence evidence) or a hard cap before queuing more reviewer wakes.

## Agent suggestion

### 2026-11-22T02:25:16.760Z — build-wait-reviewer

Evidence from issue-4409099b6172 review (build-wait-reviewer): flat 60s WAIT_ISSUE_SECONDS is below the structural minimum of a real game-DLL build. Attempt game-dll-20261122T013844014631Z-d09614 succeeded (generation 440) but took 524.297s total; changed_source_preflight alone was 111.719s and npc.cpp compile alone 110.879s (.dev/build-timings/game-dll-20261122T013844014631Z-d09614.json). So every real npc.cpp build trips the budget and, because record_wait_incident keys on per-attempt wait_id, mints a new issue plus reviewer wake (about 50 open duplicate Wait over budget: game-dll-build at game_build issues exist). Suggest: stage/workload-aware budget (no_op vs real rebuild), and cluster repeat waits by kind+stage within a bounded window appending occurrences instead of one issue per attempt. Also add first-class build_attempt_id to the wait evidence instead of only inside reason. See issue-4409099b6172 review id 92.

## Agent suggestion

### 2026-11-22T05:30:37.349Z — build-wait-reviewer

Evidence defect from issue-d2466e463a18 (build-wait-reviewer, 2026-11-22): for a game-dll-build wait, the created issue records the wrong build identity. Example: wait_id game-dll-build:2026-11-22T04:36:01.508Z for in-flight attempt game-dll-20261122T043601681032Z-fc1d05 (generation 456) was filed with build_id=2026-11-22T02:37:06.972018Z and generation=455 (the previously published build). Cause: devscripts/dev-loop.py:3532 uses self.latest_build (last published EXE artifact) for build_id and dev-loop.py:3533 uses self.game_build_generation, which is only set at result time (dev-loop.py:2967), not when start_game_build_worker marks building (dev-loop.py:2879-2884). The in-flight build_attempt_id is available from the dev-loop.game-build.started event but is not carried into the issue. This violates this issue's acceptance that an over-budget wait issue carries build/run IDs. Suggested fix: thread the active game-build attempt id/generation into check_wait_budget and stop using latest_build for game-DLL waits. Reviewer: build-wait-reviewer.

## Agent suggestion

### 2026-11-22T07:00:17.472Z — build-wait-reviewer

Wait-hook evidence from review of issue-00e2e9243beb (2026-11-22, reviewer build-wait-reviewer):

1) The wait evidence carries the wrong build identity. For wait game-dll-build:2026-11-22T06:21:19.547Z the actual attempt was game-dll-20261122T062119640904Z-8db74c / generation 465, but the issue recorded build_id=2026-11-22T02:37:06.972018Z (latest EXE build) and generation=464 (previous published generation). devscripts/dev-loop.py:3525-3535 uses self.game_build_generation, which is only assigned on completion at dev-loop.py:2967. Recommend using game_build_active_revision or the worker build_attempt_id during building.

2) The proof expectation requires a dev-loop wait event, but .dev/state.json events_path for run 20261122_011031 (logs/2026-11-22/20261122_011031/dev-loop/events.jsonl) does not exist, and record_runtime_event (dev-loop.py:2292-2297) only prints OSError. The issue and wake records were created, but dev-loop.wait.over_budget / reviewer-wake were not persisted. Recommend ensuring the parent directory exists (or a known-good fallback) before append.

3) The 60s budget is below normal game-DLL build duration: 34 of 44 attempts on 2026-11-22 exceeded 60s (median 122.9s), yielding 94 open identical wait issues. The single over-budget occurrence reviewed here spent 68.873s compiling src/npc/npc.cpp plus 20.477s linking (total 121.59s). Recommend a budget above normal build duration and/or one open issue per kind+stage aggregated over a window with occurrence counts, while keeping one reviewer wake per new signature.

Full review is on issue-00e2e9243beb.

## Agent suggestion

### 2026-11-22T08:01:21.694Z — build-wait-reviewer

Recurrence evidence (2026-11-22T08:00Z, agent build-wait-reviewer): wait game-dll-build:2026-11-22T07:27:50.398Z fired at 60.04s/60s while attempt game-dll-20261122T072750493926Z-7ac0b3 (generation 472, src/npc/npc.cpp) was healthy and succeeded in 191.924s. changed_source_preflight 85.144s and link 66.853s each exceed the flat 60s budget alone. Open backlog is now 114 build-friction issues (87 game-dll-build). Recommend a stage-aware budget above normal duration and aggregation per kind+stage with occurrence counts, instead of one issue+wake per wait_id. Identity defect: wait evidence records stale generation 471/build_id 2026-11-22T02:37:06.972018Z because dev-loop.py:2967 assigns game_build_generation only on completion. Journal gap: state events_path logs/2026-11-22/20261122_011031/dev-loop/events.jsonl is missing (issue-0498cbef4ef3).

## Agent suggestion

### 2026-11-22T08:05:44.902Z — build-wait-reviewer

Stage-aware wait-budget proposal from build-wait-reviewer (details in issue-c845e5ba04db review). The fixed 60s WAIT_ISSUE_SECONDS (dev-loop.py:109-110, applied at 3520-3536) is below a healthy incremental game-DLL build: attempt game-dll-20261122T073356741874Z-7719e3 succeeded in 118s (changed_source_preflight 53.5s, npc.cpp compile 53.3s, link 31.9s) yet raised the wait issue at 60.83s while compile was actively progressing. This produced dozens of duplicate 'Wait over budget: game-dll-build at game_build' issues/wakes on 2026-11-21/22. Proposal: derive the game-dll-build budget from measured p95 (>=180s) and/or alert only on absence of new attempt timeline/progress events for a bounded interval (stall detection) rather than raw elapsed; keep one issue per genuine stall. Metric target: remove >90% of false game-dll-build wait issues while still waking on a real no-progress stall.

## Agent suggestion

### 2026-11-22T08:33:00.638Z — build-wait-reviewer

Recurrence evidence from review of issue-18050cc9b42e (build-wait-reviewer, 2026-11-22T08:32Z, review id 159). wait game-dll-build:2026-11-22T08:03:41.642Z fired at 60.671s/60s while attempt game-dll-20261122T080341760815Z-26fbbf (generation 477, src/npc/npc.cpp) was healthy and succeeded in 126.231s; changed_source_preflight 48.184s and link 26.198s. The issue evidence again misattributed identity as generation 476 / build_id 2026-11-22T08:00:31.376619Z (previous EXE build) instead of the in-flight attempt, matching the dev-loop.py:3532-3533 defect. Journal gap persists: state events_path logs/2026-11-22/20261122_011031/dev-loop/events.jsonl is missing. Board now ~98 open build-friction wait issues. Reinforces: stage/workload-aware budget (p95 >=180s) plus stall-based detection, aggregate occurrences per kind+stage instead of one issue+wake per wait_id, and carry the active game-build attempt id in wait evidence.

## Agent suggestion

### 2026-11-22T11:37:08.725Z — build-wait-reviewer

Occurrence added for issue-d5a7e874a0db: wait game-dll-build:2026-11-22T11:04:12.764Z (60.17s) was a legitimate 105.781s build (gen487, src/actor/player-ai.cpp compile 40.457s + link 21.938s, compiled=2/skipped=641), not a stall. This reinforces the fixed-60s-budget false-positive class: player-ai.cpp alone compiles 27.7-40.7s across nearby attempts. Suggest an adaptive stall-based budget and/or per-kind+stage aggregation with occurrence counts. Evidence: .dev/build-timings/game-dll-20261122T110412831692Z-baa638.json and .jsonl.

## Agent suggestion

### 2026-11-22T14:10:16.937Z — build-wait-reviewer

Evidence from wait build-wait-reviewer issue-1cbec48ac17a (resolved as a healthy long build). Three distinct game-dll-build waits exceeded the fixed 60s WAIT_ISSUE_SECONDS in ~13 minutes, each a single-TU edit: attempt 567c5b succeeded in 80.443s (single TU compile 41.522s + link 12.666s); attempt 7896ef failed compile at 87.828s; attempt ada12d succeeded in 94.681s. The 60s budget is below the normal cost of any incremental whole-game DLL rebuild here, so it fires on normal builds. Recommend deriving the wait budget from measured incremental build cost (or exempting/reporting at a higher, size-aware threshold) and, separately, fixing the reason field: devscripts/dev-loop.py:2832-2867 sets game_build_detail to line.rstrip()[-240:] for every output line, so a long [GAME_RESULT]/progress JSON line becomes a truncated JSON tail in wait['reason'] (seen in issue-1cbec48ac17a and issue-b52a0621a29e). Keep a human-readable stage/last-action string for reason and store raw output separately.

## Agent suggestion

### 2026-11-22T14:45:17.334Z — build-wait-reviewer

Occurrence evidence for the over-budget wait umbrella: wait_id game-dll-build:2026-11-22T14:22:57.758Z (61.866s, flat 60s budget) coincided with active compilation of src/network/server-npcs.cpp (67.005s) inside successful attempt game-dll-20261122T142258012829Z-326bd4 (generation 499, total 137.01s, worker 117.309s). This is intrinsic incremental whole-game DLL cost, not a stalled queue. Suggest per-stage budgets from measured p50/p95 and grouping repeated wait issues by this signature. Recorded under issue-5870fafa670e (review verdict confirmed-intrinsic-build-cost).

## Scheduled independent review

### 2026-11-22T14:59:36.626Z — cpp-latency-digest-cache

A bounded independent review was queued as wake wake-5c78c0cfc430 after 299 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Agent suggestion

### 2026-11-22T21:33:06.337Z — build-wait-reviewer

Recurring wait intake has created 122 open identical Wait over budget: game-dll-build at game_build issues out of 157 build-friction issues as of 2026-11-22T21:32Z. record_wait_incident dedupes only by wait_id, and the flat 60s threshold is below normal incremental game-DLL latency (gen523 65.5s, gen525 76.2s). Evidence: issue-407244573505 review and issue-a236165fa6fc review. Consider a progress-aware threshold or one rolling issue with occurrences.

## Scheduled independent review

### 2026-11-22T22:18:00.097Z — build-wait-reviewer

A bounded independent review was queued as wake wake-504e447204fb after 343 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-live-with-open-defects)

### 2026-11-22T22:28:59.213Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-504e447204fb), read-only, 2026-11-22T22:18-22:25Z. No source, build, close, or fix performed.

OBSERVED FACTS
1. Live end-to-end path now works. Root dev-loop run 20261122_220630 journal logs/2026-11-22/20261122_220630/dev-loop/events.jsonl lines 59-60 contains dev-loop.wait.reviewer-wake and dev-loop.wait.over_budget for wait game-dll-build:2026-11-22T22:14:00.638Z; it created issue-6943764e0d8e and queued/launched wake-14eefe244552 (.dev/agent-wake/requests/wake-14eefe244552.json: created 22:15:03.964Z, claimed 22:15:19.362Z, launched 22:15:19.441Z). This resolves the earlier note that a live over-60s occurrence was still required: issue record, reviewer wake, and durable journal events are all present, and .dev/state.json events_path for this run exists.
2. Budget is still the flat 60s: devscripts/dev-loop.py:109-110 (WAIT_ISSUE_SECONDS), applied at 3497/3516/3534. The occurrence it flagged was a healthy build: attempt game-dll-20261122T221400682391Z-2b5e5e changed only src/npc/npc-world-query.cpp; journal line 61 reports status=ok, generation 536, total_seconds=94.797 (compile 59.862, link 25.624).
3. Wait evidence still carries the wrong identity. For this wait, evidence records generation=535 and build_id=2026-11-22T21:32:48.195707Z (previous EXE build), while the in-flight attempt is generation 536 / game-dll-20261122T221400682391Z-2b5e5e. dev-loop.py:3544 uses self.game_build_generation, which is assigned only on completion at dev-loop.py:2976. This violates the issue acceptance that the wait issue carries build/run IDs.
4. Duplication is still per-attempt, not per-signature. record_wait_incident dedupes only by wait_id (devscripts/build_friction.py:131-148 and 164-169) and wait_id embeds the per-attempt start time (dev-loop.py:3536), so each new attempt mints another issue plus reviewer wake. The coordinator store currently holds 127 issues titled 'Wait over budget: game-dll-build at game_build' (23 new, 103 in_review, 1 assigned) and 149 total open 'Wait over budget:*' issues. The risk this issue itself names ('a noisy wait policy could create duplicate friction issues') has materialized.
5. The over_budget/reviewer-wake journal lines were written at 22:15:19.538, about 16-18s after the wake request was created and after the synchronous agent-wake.py run --once drain (build_friction.py:244-246, timeout=30) returned. The reviewer still launched ~15.4s after creation, inside the <=5 min target of issue-84c584957e65, but the call blocks the wait-check path for its duration.

INFERENCE / UNCERTAINTY
- The automation (detect wait -> one issue -> one wake -> durable journal) is functioning; remaining defects are policy (flat budget) and identity (wrong generation/build_id), not a broken hook.
- The 'proven' evidence attached to this issue (evidence-0c12a438dd86) is a unit proof with empty matched_events/run/build IDs. I treat the live journal lines above as current behavioral evidence, not that unit record.
- I did not prove a literal same-wait duplicate within one process vs across restart. Code inspection (reported_wait_ids in-process; _wait_issue across restart) suggests one wait_id is not duplicated. The acceptance is per-wait, so per-attempt repetition may be intended; the noise is the aggregate consequence rather than a 'second scan' bug.

CONTRARY EVIDENCE
- The live occurrence was a genuine >60s wait and the automation handled it correctly end to end, so the feature is not inert.
- The launched reviewer wake was a normal payload/queue wake under the documented contract, not a duplicate launch for the same wait.

NEXT DISCRIMINATING CHECK
- After the review session for wake-14eefe244552 finishes, inspect .dev/agent-wake/events.jsonl and requests/wake-14eefe244552.json to confirm exactly one launch for this wait_id (no retry/duplicate).
- On the next game-DLL attempt, compare the wait issue's generation/build_id to that run's dev-loop.game-build.started build_attempt_id and result generation; a mismatch confirms the identity defect with a fresh pair.

## Agent suggestion

### 2026-11-22T22:32:22.588Z — build-wait-reviewer

Over-budget wait review of game-dll-build:2026-11-22T22:04:56.185Z (issue-a240a6dd4eab, review 219) shows the fixed 60s WAIT_ISSUE_SECONDS budget is below normal healthy single-TU incremental game-DLL builds: server-npcs.cpp changes took 61.7s/77.0s/98.9s (generations 532/533/530) and npc-world-query.cpp 95.0s (generation 536), all success=true; npc-spawn.cpp builds stayed at 23-30s. Recommend: (1) make the budget stage/TU-size-aware (for example >=180-240s for game-dll-build, or derived from a measured per-TU baseline) so a normal large-file rebuild is not reported as friction; (2) coalesce recurring identical wait signatures into one rolling occurrence issue instead of one issue per occurrence, because 127 identical Wait over budget: game-dll-build at game_build issues already exist (23 new, 103 in_review, 1 assigned); (3) keep the recorded reason as the readable [GAME BUILD] ... summary rather than a truncated raw output line (this occurrence stored a JSON fragment); (4) expire stale reviewer wakes so a review cannot arrive ~12 minutes after the wait already resolved. Evidence: .dev/build-timings/game-dll-20261122T220456195519Z-2dad5a.json, .dev/agent-wake/requests/wake-75c2d2420471.json.

## Agent suggestion

### 2026-11-22T23:07:39.169Z — issue-review-sweeper

2026-11-22 independent review of issue-63dccbd8f8b6 adds one measurable consolidation proposal (see the review on that issue).

Evidence: the store now holds 129 same-titled 'Wait over budget: game-dll-build at game_build' issues since 2026-11-20, including 97 build-friction issues on 2026-11-22 alone, and build-wait-reviewer has already written 134 reviews across 127 of them. Current code dedupes only by per-attempt wait_id (devscripts/build_friction.py:131-169; devscripts/dev-loop.py:3536), so each healthy >60s attempt mints another issue plus wake. The original first occurrence (issue-63dccbd8f8b6) also had its reviewer wake fail and never received a review.

Proposal: aggregate over-budget waits by (kind, stage) inside a rolling window into one issue with occurrences, instead of one issue per attempt; and add a missing-review task when a *_reviewer wake ends failed and the linked issue still has no review.

Measurable target: at most 1 new game-dll-build wait issue per UTC day while preserving every occurrence's wait_id/build_attempt_id in occurrences; and 0 wait issues whose reviewer wake entered failed with no recorded review after 60 minutes. Baseline for the comparison is the current 129 issues over ~3 days.

Smallest experiment: in build_friction.record_wait_incident, look up an existing open issue for the same kind+stage created within 24h and append an occurrence (store.issue_occurrence) instead of issue_add; keep wait_id as the occurrence key. Then count same-titled issues created in the following 24h and compare with the 129/3-day baseline. This changes the per-occurrence policy in this issue's acceptance, so it needs a human decision before implementation.

## Agent suggestion

### 2026-11-23T08:06:43.162Z — issue-review-sweeper

Process proposal (issue-review-sweeper, 2026-11-23T08:0xZ), evidence from the bounded review of issue-dd54a79216ad. Prior review id 217 (2026-11-22) documented per-attempt duplication as a fact; this adds a measurable target and smallest experiment. No source edited, no fix claimed.

EVIDENCE
- The store now holds 152 open 'Wait over budget*' build-friction issues: 133 in_review, 18 new, 1 assigned; 130 identically titled 'Wait over budget: game-dll-build at game_build'; 150 unowned. Created 29 on 11-20, 27 on 11-21, 96 on 11-22; newest 2026-11-22T23:53:23Z.
- devscripts/build_friction.py:131-148,164-169 dedupes only by exact wait_id, so every new attempt mints a fresh issue plus reviewer wake. This is the risk named in issue-dd54a79216ad.

PROPOSAL
- Group by signature f'{kind}:{stage}'. In record_wait_incident: (a) keep the exact-wait_id check first; (b) else reuse the most recent open build-friction issue with the same signature within a 24h window and append the occurrence instead of issue_add; (c) queue at most one reviewer wake per signature-window. One-time coalesce the existing 152 open issues into per-signature umbrellas while preserving every occurrence.

MEASURABLE TARGET
- New open wait issues for one signature <=1 per 24h (overall creation was up to 96/day).
- Total open 'Wait over budget*' issues reduced from 152 to <=10 by one-time coalescing.
- Zero duplicate-title issues for the same signature within 24h.
- No wait occurrence lost: occurrence count preserved and readable.

SMALLEST EXPERIMENT
- In a scratch CoordStore (no live build), call record_wait_incident twice with two distinct wait_ids, same kind/stage, 1 minute apart. Baseline (current code): issue count grows by 2. Expected (grouped): issue count grows by exactly 1, occurrence count = 2, wake_reviewer invoked once. No live build required.

DECISION / RISK
- Moving from per-wait to per-signature changes this issue's stated acceptance ('one issue per wait occurrence'), so an owner/human decision is required before implementation. Grouping is the smallest change that removes the noise without discarding occurrences. Suggested owner: wake-proof-wait.

## Agent suggestion

### 2026-11-23T11:19:16.026Z — issue-review-sweeper

Process proposal from independent review of issue-7d013b4a6c77 (review id 256, 2026-11-23T11:18Z). Read-only; no source edit, build, or fix claimed.

EVIDENCE (observed)
- This exe-build wait (kind=exe-build, stage=setup, generation 23, 2026-11-20T17:04:44.830Z) had exactly one reviewer wake queued (wake-f82615c46512 at 17:05:46.144Z) but produced no review: codex launch 17:05:46.8 exited, retry 17:18:37.328Z, opencode launch 18:28:29.6Z, wake.failed 19:05:56.570Z "adapter process exited" (.dev/agent-wake/events.jsonl:282,285-286,294,304-305,312; requests/wake-f82615c46512.json status=failed, attempts=2).
- The issue then had zero reviews for ~3d18h until the general scheduler swept it (coord event seq10205 at 2026-11-23T11:11:25Z).
- Its dev-loop journal is not retained (logs/11-20-2026 empty), so the issue's proof_expectation cannot be met for that occurrence.

PROPOSAL (extends, not duplicates, the 2026-11-22T23:07:39Z missing-review-task proposal on this issue; adds exe-build scope and a concrete experiment)
- At sync/wait, run a read-only query: build-friction issues with via_agent=dev-loop-wait, no issue_reviews row, older than 60 min, whose linked wake request status=failed; then append a "missing review" task or queue one bounded review wake per issue, deduped per issue per 24h. Cover every wait kind (exe-build, game-dll-build, queues), not only game-dll-build.

MEASURABLE TARGET
- 0 build-friction wait issues with a failed reviewer wake and no review after 60 min (this occurrence is baseline 1, gap 3d18h).
- At most 1 extra review wake per issue per 24h.

SMALLEST EXPERIMENT
- Implement only the read-only query and run it once against .dev/agent-coord. It must flag issue-7d013b4a6c77 (wake-f82615c46512 failed, 0 reviews) and must not flag any issue that already has a review. Record the flagged ids and elapsed time; only then add the deduped missing-review task. No gameplay, build, or proof-state change.

## Agent suggestion

### 2026-11-23T11:53:35.578Z — issue-review-sweeper

PROCESS PROPOSAL (issue-review-sweeper, 2026-11-23, derived from the bounded review of issue-25b14f436a3e). Evidence: 152 open Wait-over-budget build-friction issues remain (134 in_review, 17 new), 130 for game-dll-build at game_build; one reviewer wake (wake-f12675d71a59) took about 24 min to fall back after an adapter exit and was finally recorded wake.failed despite already producing a review. Measurable target: within 24h of the change, (a) new build-friction issues for repeated (kind=game-dll-build, stage=game_build) waits drop from about 130 over about 2 days to <=1 open issue per (kind, stage, normalized-reason) signature, and (b) game-dll-build reviewer-wake queue-to-launch p95 stays <=5 min with no wake.failed record once the adapter has written the review. Smallest experiment: in build_friction.record_wait_incident, compute a signature key from kind + stage + normalized reason (strip timestamps, paths, revision hashes, and JSON fragments) and, when an open issue with that signature exists, append an occurrence with the new wait_id instead of calling issue_add; keep the existing per-wait_id dedupe. Do not auto-close or merge the existing 152 issues; a human decides. Compare, from .dev/agent-wake/events.jsonl and the issue store, the count of new wait issues and the queue-to-launch gap for the 24h before vs after.

## Agent suggestion

### 2026-11-23T13:28:26.311Z — issue-review-sweeper

Process-review proposal from the 2026-11-23 issue-review-sweeper bucket sweep (wake-3093c566f04f), evidence-grounded, read-only.

EVIDENCE (observed this sweep, 2026-11-23T13:2xZ): the coordinator holds 130 open issues titled "Wait over budget: game-dll-build at game_build" (115 in_review, 14 new, 1 assigned), 152 open "Wait over budget:*" issues, and 165 build-friction issues total. Per-wait_id minting is explicit by design in devscripts/build_friction.py:143-148 ("a new wait gets another issue even when its stage and reason are identical") and record_wait_incident (164-180). The prior independent review already measured 127 of these at 2026-11-22T22:28Z, so the count grew by 3 in ~15h. This issue's acceptance already requires "a second scan does not duplicate the issue", but that holds only per wait_id, not across repeated occurrences of the same stage/signature.

PROPOSAL: Group over-budget wait issues by a stable signature (kind + stage; optionally a normalized reason class) instead of by wait_id. On each over-budget occurrence, append an occurrence row to the one open signature issue (store.issue_occurrence) and keep that occurrence's wait_id plus the true in-flight attempt identity (generation + build_attempt_id) in its evidence; mint a new issue only when no open issue exists for the signature. Keep exactly one reviewer wake per occurrence (current behavior already satisfies that).

MEASURABLE TARGET: across the next 5 over-budget game_build occurrences, net-new duplicate-title "Wait over budget: game-dll-build at game_build" issues = 0, while each occurrence adds exactly 1 occurrence row and queues exactly 1 reviewer wake, and each occurrence row carries the true in-flight generation/build_attempt_id rather than the previous EXE build. Secondary: mark the existing 130 duplicates as duplicate/merged (state or resolution note) without deleting any evidence.

SMALLEST EXPERIMENT: extend devscripts/quality/test_build_friction.py with one unit case that feeds two synthetic waits with distinct wait_ids but identical kind/stage and asserts: one issue created, two occurrence rows, two reviewer wakes. Then confirm with one real over-budget wait (force a >60s game-DLL build or lower the budget only in a scratch dev-loop) that the grouped issue's occurrence count increments and no new duplicate-title issue appears.

RISK / GUARD: grouping can bury a fresh distinct cause under a stale issue. Keep reason and attempt id per occurrence, group only (kind, stage) unless the reason class is provably stable, and never drop the per-occurrence wait_id.

## Agent suggestion

### 2026-11-23T14:30:56.936Z — issue-review-sweeper

Duplicate derived-failure issues from one failed build (development-process proposal, sweep issue-review-sweep-408; evidence: review id 262 on issue-b91438ac2972).

Observed: attempt game-dll-20261120T163826804025Z-9b84b5 produced two near-identical build-friction issues, issue-b91438ac2972 (created 2026-11-20T17:24:14Z, depends on issue-b0ba8698acd7) and issue-b7216286bdcc (created 2026-11-20T17:23:44Z), with identical human_quote and the same revision/changed_paths. record_wait_incident in devscripts/build_friction.py:143 dedupes by wait_id, but derived compile-failure findings have no dedupe key, so separate reviewer sessions each called issue_add for the same failure. Both remain open, and this sweep reviewed the duplicate at 2026-11-23T14:30Z after the sibling was already reviewed at 13:59Z (review id 261), duplicating reviewer effort.

Measurable target: 0 pairs of open build-friction issues sharing (build_attempt_id, first compile-error file:line:message) per 10 failed builds (currently 1 known pair from one attempt), and at most 1 independent review per failure signature.

Smallest experiment (read-only intake dedupe): before issue_add for a derived build failure, search open/recent build-friction issues for the same build_attempt_id and first error signature; if found, append an occurrence/evidence link and surface the existing issue id to the reviewer prompt instead of creating a second issue. Validate against this attempt's two records without creating a new issue. No source, policy, or closure change by this reviewer; human retains the decision.

## Agent suggestion

### 2026-11-23T15:03:17.506Z — issue-review-sweeper

Proposal (issue-review-sweeper, from bounded review of issue-bb273ac1386c): consolidate repeated wait signatures instead of filing one issue per wait_id.

Evidence: 130 identical `Wait over budget: game-dll-build at game_build` issues and 326 `build-wait-reviewer` wake artifacts; 182/350 recorded DLL builds exceed the fixed 60s budget, and the last >60s build (2026-11-22T23:56, 267.2s) produced a duplicate rather than an occurrence. `build_friction.record_wait_incident` keys on the timestamp-bearing wait_id, so the append-occurrence branch is unreachable and the docstring makes duplication intentional.

Measurable target: for one (kind, stage) signature, keep at most 1 open canonical issue; repeat waits append an occurrence/capped counter instead of a new issue. Reviewer wakes per signature per 24h <= 1 unless the wait has a new failure-reason class. Baseline: 130 open duplicates and up to 98 reviewer wakes/day for one signature.

Smallest experiment: keep wait_id/timestamps in evidence but key dedup on (kind, stage, failure-reason-class); a second identical wait appends an occurrence and does not call `wake_reviewer`. Validate in `devscripts/quality/test_build_friction.py` by calling `record_wait_incident` twice with distinct wait_ids and asserting one issue + one wake; then drive one real >60s build and confirm one issue, not two.

Cross-links: issue-bb273ac1386c (prior and current review), issue-6e08d89b55e3 (reason field), issue-84c584957e65 (late wakes).

## Agent suggestion

### 2026-11-23T16:25:42.840Z — build-wait-reviewer

Fresh pair confirming the open identity defect (the next check requested in this issue's 2026-11-22 review): wait game-dll-build:2026-11-23T15:24:55.624Z (issue-266c2a856cfc) recorded evidence generation=null and build_id=2026-11-23T13:26:39.822193Z, while the in-flight journal attempt was dev-loop.game-build.started build_attempt_id game-dll-20261123T152455729876Z-62f5f6, generation=561, source_generation=10, started_utc 15:24:55.730Z, success total_seconds=107.935. The wait was 60.82s into a healthy 6-file build (slowest compile npc.cpp 50.792s), so this is a flat-budget/identity issue, not a stalled loop. Also reproduced: same-run per-attempt duplicates issue-8a2e0ac7236b and issue-ec790f30f5ad, and a garbled evidence.reason (truncated compiler progress JSON). Wake dispatch delay for this occurrence is recorded on issue-84c584957e65.

## Occurrence

### 2026-11-23T16:39:39.118Z — build-wait-reviewer

Wait game-dll-build:2026-11-23T16:00:36.698Z (issue-ec790f30f5ad) confirms the stale-identity defect with a fresh pair, answering the NEXT DISCRIMINATING CHECK in review 217. Wait evidence records generation=561 and build_id=2026-11-23T13:26:39.822193Z (previous published build), while the in-flight attempt was generation 563 / game-dll-20261123T160037032706Z-485a05 (status ok, total 69.123s, compile 36.524s, link 21.293s, 2 TUs compiled, 641 skipped). The flagged build was healthy and finished 0.15s past the flat 60s budget at a normal worst-case single-file cost, so the flat budget also fired on a non-stall.

Evidence: {"actual_generation": 563, "build_attempt_id": "game-dll-20261123T160037032706Z-485a05", "build_outcome": "success", "compile_seconds": 36.524, "identity_root": "dev-loop.py:3259 vs dev-loop.py:3879", "issue": "issue-ec790f30f5ad", "journal": "logs/2026-11-23/20261123_144429/dev-loop/events.jsonl", "link_seconds": 21.293, "total_seconds": 69.123, "wait_build_id": "2026-11-23T13:26:39.822193Z", "wait_generation": 561, "wait_id": "game-dll-build:2026-11-23T16:00:36.698Z"}

## Agent suggestion

### 2026-11-23T17:29:11.083Z — build-wait-reviewer

DEFECT F (evidence from review of issue-991c632f1ca0, occ wait_id game-dll-build:2026-11-23T16:27:01.355Z):
dev-loop.wait.issue-failed masks a successful record and suppresses the required wait events. In check_wait_budget (dev-loop.py:3887-3912): record_wait_incident() succeeds and wait_id is added to reported_wait_ids (3891), then wake_reviewer() (3895) raises because its inline subprocess.run([... agent-wake.py run --once], timeout=30) in build_friction.py:244-246 exceeds 30s. The except at 3908 then logs issue-failed and skips dev-loop.wait.reviewer-wake (3897-3902) and dev-loop.wait.over_budget (3903-3907).
Direct evidence: logs/2026-11-23/20261123_144429/dev-loop/events.jsonl line 264 is the only event for that wait_id (dev-loop.wait.issue-failed 16:28:34.966Z, error 'timed out after 30 seconds'); there is no over_budget/reviewer-wake event for it, yet issue-991c632f1ca0 (created 16:28:02.149Z) and wake-56368e82bb5b (queued 16:28:04.703Z, .dev/agent-wake/events.jsonl:2234) both exist. The build itself succeeded (generation 564, 724.396s, npc.h fanout).
Suggested safe fix (needs human approval): emit dev-loop.wait.over_budget immediately after a successful record_wait_incident and before the best-effort inline launch; make the inline 'run --once' fire-and-forget (short/swallowed timeout) so launch latency cannot retract a successful record; reserve issue-failed for genuine record failures. This belongs to the automatic over-budget wait machinery owned here, so reporting as a suggestion rather than a new issue.

## Occurrence

### 2026-11-23T17:34:25.048Z — build-wait-reviewer

Fresh identity pair and flat-budget recurrence for wait game-dll-build:2026-11-23T16:54:31.578Z (issue-3d02ab0de320): evidence records build_id 2026-11-23T13:26:39.822193Z and generation 564 (previous published), while the in-flight journal attempt was dev-loop.game-build.started build_attempt_id game-dll-20261123T165431661826Z-6c221c, generation 565, source_generation 16, started 16:54:31.803Z. It succeeded at 90.92s total (compile 76.122s, link 6.27s, 29 compiled / 614 skipped), so the 60s wait fired during a healthy compile (queue_wait_ms up to ~55.6s with 10 jobs), not a stall. Same-signature duplicates today: issue-991c632f1ca0 (16:28Z) and issue-8fdc520a07c5 (17:10Z). Reason-field contamination is recorded on issue-6e08d89b55e3 and the wake dispatch delay on issue-84c584957e65. Line numbers drifted: identity is now dev-loop.py:3878-3879, reason at dev-loop.py:3112/3142/3874.

Evidence: {"actual_generation": 565, "build_attempt_id": "game-dll-20261123T165431661826Z-6c221c", "build_outcome": "success", "compile_seconds": 76.122, "compiled": 29, "identity_root": "dev-loop.py:3878-3879 (latest_build / game_build_generation) vs completion at dev-loop.py:3259", "issue": "issue-3d02ab0de320", "journal": "logs/2026-11-23/20261123_144429/dev-loop/events.jsonl", "link_seconds": 6.27, "skipped": 614, "source_generation": 16, "summary": ".dev/build-timings/game-dll-20261123T165431661826Z-6c221c.json", "total_seconds": 90.92, "wait_build_id": "2026-11-23T13:26:39.822193Z", "wait_generation": 564, "wait_id": "game-dll-build:2026-11-23T16:54:31.578Z"}

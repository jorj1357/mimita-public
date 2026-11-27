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

## Agent suggestion

### 2026-11-23T19:26:34.152Z — build-wait-reviewer

Fresh identity pair + flat-budget recurrence (build-wait-reviewer, wake-bb564fe4680a, review 292 on issue-88caca2905d1). Wait game-dll-build:2026-11-23T19:13:41.970Z fired at 60.792s/60s with evidence build_id=2026-11-23T18:39:16.832027Z and generation=null, while the in-flight attempt was game-dll-20261123T191346937240Z-35f07c, generation 570, source_generation 4 (Mode 3 batch 2026-11-23T19:13:37.823Z-2dae4ae4:mode3:game sealed 19:13:46.938Z). That attempt succeeded: ready 19:20:49.235Z, total 420.906s (worker 398.887s, compile 385.061s, link 7.555s, 67 compiled / 576 skipped, fanout gamemode.h=51 post-fx.h=20). So the flat 60s total-elapsed budget fired on a legitimate high-fanout recompile, not a stall, and the wait evidence again omitted the true attempt identity (dev-loop.py check_wait_budget latest_build/game_build_generation). Journal logs/2026-11-23/20261123_184017/dev-loop/events.jsonl lines 400/426/427/566. This confirms the NEXT DISCRIMINATING CHECK from review 217; identity source still unfixed as of this occurrence.

## Occurrence

### 2026-11-23T22:41:17.051Z — build-wait-reviewer

Recurrence — flat 60s budget on a healthy long build, per-attempt duplicate (issue-5643605344b4).
Wait game-dll-build:2026-11-23T21:51:15.080Z (elapsed 60.37s, issue-5643605344b4) fired while attempt game-dll-20261123T215115211280Z-e6886c (generation 572) was still compiling; the build succeeded 11.3s later at total 71.695s (compile 57.774s, slowest src/network/server-gamemode.cpp 56.274s, link 5.347s). Host CPU mean 99.578% / peak 100%, min available memory ~829 MiB. Nothing stalled. The immediately following wait game-dll-build:2026-11-23T22:09:45.196Z (60.745s) minted sibling issue-1f0c919bdbaa, confirming per-attempt dedupe still produces one issue+wake per build. Journal: logs/2026-11-23/20261123_192224/dev-loop/events.jsonl; timings: .dev/build-timings/game-dll-20261123T215115211280Z-e6886c.json. Reviewer: build-wait-reviewer.

Evidence: {"build_attempt_id": "game-dll-20261123T215115211280Z-e6886c", "build_outcome": "success", "compile_seconds": 57.774, "duplicate_sibling": "issue-1f0c919bdbaa", "elapsed_seconds": 60.37, "generation": 572, "issue": "issue-5643605344b4", "journal": "logs/2026-11-23/20261123_192224/dev-loop/events.jsonl", "slowest_seconds": 56.274, "slowest_source": "src/network/server-gamemode.cpp", "total_seconds": 71.695, "wait_id": "game-dll-build:2026-11-23T21:51:15.080Z"}

## Occurrence

### 2026-11-23T22:45:01.945Z — build-wait-reviewer

Fresh identity pair + flat-budget recurrence (build-wait-reviewer, wake-6008dd7db0f2, review on issue-1f0c919bdbaa).

Wait game-dll-build:2026-11-23T22:09:45.196Z fired at 60.745s/60s with evidence build_id=2026-11-23T19:22:15.804284Z and generation=572 (the previously published build), while the in-flight attempt was game-dll-20261123T220945368970Z-09cabb, generation 573, source_generation 12, started_utc 22:09:45.369Z. That attempt succeeded: generation 573 ready 22:23:53Z, total_seconds 845.325 (compile 772.452, link 50.334, 72 compiled / 571 skipped) after a 3-header high-fanout change (aimbody-config.h=12, gamemode.h=51, post-fx.h=20). So the flat 60s total-elapsed budget fired on a legitimate high-fanout rebuild, not a stall, and the wait evidence again omitted the true attempt identity. reason was clean this occurrence (the reason-truncation defect did not recur). This confirms the NEXT DISCRIMINATING CHECK requested in the 2026-11-22 review; identity source remains unfixed at dev-loop.py check_wait_budget (latest_build / game_build_generation assigned only at completion).

Evidence: {"actual_generation": 573, "build_attempt_id": "game-dll-20261123T220945368970Z-09cabb", "build_outcome": "success", "compile_seconds": 772.452, "compiled": 72, "changed_header_fanout": {"src/entities/aimbody-config.h": 12, "src/gamemode/gamemode.h": 51, "src/render/post-fx.h": 20}, "identity_root": "check_wait_budget uses previously published latest_build/game_build_generation", "issue": "issue-1f0c919bdbaa", "journal": "logs/2026-11-23/20261123_192224/dev-loop/events.jsonl", "link_seconds": 50.334, "skipped": 571, "source_generation": 12, "summary": ".dev/build-timings/game-dll-20261123T220945368970Z-09cabb.json", "total_seconds": 845.325, "wait_build_id": "2026-11-23T19:22:15.804284Z", "wait_generation": 572, "wait_id": "game-dll-build:2026-11-23T22:09:45.196Z"}


Evidence: {"actual_generation": 573, "build_attempt_id": "game-dll-20261123T220945368970Z-09cabb", "build_outcome": "success", "changed_header_fanout": {"src/entities/aimbody-config.h": 12, "src/gamemode/gamemode.h": 51, "src/render/post-fx.h": 20}, "compile_seconds": 772.452, "compiled": 72, "identity_root": "check_wait_budget uses previously published latest_build/game_build_generation", "issue": "issue-1f0c919bdbaa", "journal": "logs/2026-11-23/20261123_192224/dev-loop/events.jsonl", "link_seconds": 50.334, "skipped": 571, "source_generation": 12, "summary": ".dev/build-timings/game-dll-20261123T220945368970Z-09cabb.json", "total_seconds": 845.325, "wait_build_id": "2026-11-23T19:22:15.804284Z", "wait_generation": 572, "wait_id": "game-dll-build:2026-11-23T22:09:45.196Z"}

## Occurrence

### 2026-11-24T00:16:02.538Z — build-wait-reviewer

Wait game-dll-build:2026-11-23T23:43:01.179Z (issue-b1744402b0a5) reconfirms the stale-identity defect and adds a failing-build case. Wait evidence records generation=574 and build_id=2026-11-23T19:22:15.804284Z (previously published EXE), while the in-flight journal attempt was game-dll-20261123T234301570308Z-6c7a9c (source_generation 49, started 23:43:26.977Z), which FAILED at changed_source_preflight on src/network/multiplayer-projectiles.cpp after 157.347s (result 23:46:11.624Z). The flat 60s budget fired at 61.338s only 1.338s over while ~25.8s of that had elapsed before game-build.started; this occurrence's build was genuinely failing, unlike the healthy 22:09/22:57 rebuilds in the same run. The active attempt id and run_id are absent from wait evidence (dev-loop.py:3878-3879 use last-published/generation-on-completion values). Backlog unchanged: 144 open game-dll-build waits, 180 open build-friction.

Evidence: {"build_attempt_id": "game-dll-20261123T234301570308Z-6c7a9c", "build_outcome": "failed", "failed_phase": "changed_source_preflight", "failed_source": "src/network/multiplayer-projectiles.cpp", "identity_root": "dev-loop.py:3878-3879", "issue": "issue-b1744402b0a5", "journal": "logs/2026-11-23/20261123_192224/dev-loop/events.jsonl", "pre_worker_seconds": 25.798, "preflight_seconds": 157.347, "source_generation": 49, "wait_build_id": "2026-11-23T19:22:15.804284Z", "wait_elapsed_seconds": 61.338, "wait_generation": 574, "wait_id": "game-dll-build:2026-11-23T23:43:01.179Z"}

## Agent suggestion

### 2026-11-24T00:57:39.766Z — issue-review-sweeper

Occurrence for this feature's acceptance ("wakes one reviewer automatically"), from the bounded review of issue-af958c3e10eb (wait_id exe-build-queue:105972.877, 2026-11-20T19:56:23Z, elapsed 60.492s). The productive half held: exactly one issue was created (coord.issue.opened seq 2216, wait_id dedupe worked) and exactly one reviewer wake was queued (wake-020bc6842761, created 2026-11-20T19:56:25.198Z). The wake did not produce a review: the delivered prompt (.dev/agent-wake/logs/2026-11-20T19-56-25.198Z-build-wait-reviewer.prompt.txt) opened with "This is a bounded test activation" and instructed begin --task "wake test"; the woken build-wait-reviewer wrote "No issue claimed — this is a no-work test activation", the request ended status=failed ("adapter process exited", fallbacks []) at 2026-11-20T20:33:55Z, and the issue stayed state=new for ~3.3 days / 475 sessions until a periodic sweep on 2026-11-24. The prompt mislabel is fixed in current source (devscripts/quality/test_agent_wake.py asserts the phrase is absent; 3/3 pass), but nothing asserts that a reviewer wake ends in an issue review. Full evidence, uncertainty, contrary evidence, and next check are in the issue-af958c3e10eb review. A general completion-assertion proposal is recorded on issue-91e3384b219b; this feature's acceptance could add "a queued reviewer wake appends an issue review or an explicit reasoned decline within one daemon cycle."

## Agent suggestion

### 2026-11-24T01:09:36.060Z — issue-review-sweeper

PROCESS PROPOSAL (issue-review-sweeper, bounded review of issue-84ec2bf27598) — turn the two recurring wait-evidence defects into ONE regression test, deliberately distinct from the already-proposed signature-dedup test.

Evidence this sweep:
- The original occurrence's own evidence records the previous published build (generation 409 / build_id 2026-11-20T17:17:26.879159Z) while the in-flight attempt was generation 410 / game-dll-20261120T190454164010Z-f76868 (success, total 375.934s).
- The fresh gen576 wait (issue-ecdc697b2415, wait_id game-dll-build:2026-11-24T00:39:01.308Z) again records build_id 2026-11-23T19:22:15.804284Z / generation null against in-flight generation 576 / game-dll-20261124T003901444604Z-3c8d4e (success, total 949.243s) and again stores a mid-JSON reason tail.
- 145 open 'Wait over budget: game-dll-build at game_build' issues now exist.
- Existing tests in devscripts/quality/test_build_friction.py check one-issue-per-wait, the waiting-queue task, and a 61s wait; none asserts evidence identity or reason readability.

PROPOSAL: add one test case (two assertions) to devscripts/quality/test_build_friction.py that feeds a synthetic over-budget game-dll-build wait whose in-flight attempt differs from the last published build, and asserts on the evidence dict passed to record_wait_incident: (1) evidence.build_id/generation identify the in-flight attempt, not the last published build; (2) evidence.reason is a readable summary, never the last-240-char JSON tail. This is deliberately separate from the signature-dedup unit test already proposed on 2026-11-23; it guards the identity fix here and the reason fix in issue-6e08d89b55e3 so neither regresses silently.

MEASURABLE TARGET: the new test fails on HEAD with exactly 2 failing assertions; both pass after the identity fix (this issue) and the reason fix (issue-6e08d89b55e3) land; the rest of test_build_friction.py stays green. Secondary: across the next 5 over-budget game-dll-build occurrences, evidence.build_id/generation match the in-flight attempt in 5/5 and evidence.reason is non-JSON in 5/5.

SMALLEST EXPERIMENT: add the test only, run it on HEAD to capture the 2-failure baseline, then re-run after the first of the two fixes lands; keep assertions on the evidence dict, not implementation details. Human approval is needed before editing the shared test surface; no source or build change was made in this sweep.

## Agent suggestion

### 2026-11-24T01:39:13.882Z — issue-review-sweeper

Process-review proposal (issue-review-sweeper, wake-aca5a63477a9, 2026-11-24). Evidence from the bounded review of issue-00b133ae7e13. Read-only; no source edit, build, or fix claimed. This extends, and does not duplicate, the per-signature grouping proposals already recorded here (currently lines 26, 58, 90, 126, 146, 166, 180, 186-205, 232, 238-242).

REPEATED DELAY, MEASURED
- The same per-signature grouping + flat-budget fix has been independently re-derived on this issue at least 8 times between 2026-11-21 and 2026-11-24 (lines 26, 58, 90, 126, 146, 166, 180, 205, 232, 238-242), each with measurable targets.
- It has not landed: devscripts/build_friction.py last commit d651e133 2026-11-20T18:25:38-0500 still dedupes only by exact wait_id, and build.py:125-136 still defaults to os.cpu_count().
- Backlog grew from 127 game-dll / 149 total (2026-11-22T22:25Z) to 145 game-dll / 168 total (2026-11-24T01:2xZ).

INFERENCE
- The binding constraint is decision latency on a shared-path change requiring human/owner approval, not missing analysis. Re-deriving the analysis adds reviewer cost and issue noise without shrinking the backlog.

PROPOSAL (one; escalation/dedupe of proposals, not a re-proposal of grouping)
- Add a decision-deadline signal: when an independent review re-derives a proposal already present on the same improvement issue (matched by signature), append a one-line cross-reference "already proposed <first-id/date>; awaiting decision" plus a board-visible blocked-on-decision marker instead of a new multi-paragraph proposal. Escalate to the human after a second independent re-derivation.
- Measurable target: within 24h, every repeated-signature proposal has an explicit decision or blocked-on-decision marker; same-signature proposal re-derivations drop from >=8 in 4 days to <=1 until a decision is recorded; no new wait issue is minted for an existing signature (tied to the grouping decision).
- Smallest experiment: add a read-only pre-append check to the issue-review-sweeper improvement step that scans the target issue for an existing proposal containing the same signature (kind+stage) and, on a match, writes the one-line cross-reference instead of a proposal. Run it on the next 3 sweeps and count same-signature proposals and time-to-decision before vs after. No build, no source behavior change.
- Ownership: this issue's owner/human for the grouping decision; the sweeper tooling change is separate and does not auto-close or merge the existing 168 issues.

## Agent suggestion

### 2026-11-24T02:54:57.831Z — issue-review-sweeper

Process evidence + proposal update (issue-review-sweeper, 2026-11-24, from the bounded independent review of issue-693e63059a7c).

This strengthens, and does not replace, the "failed wake + no review" proposal already recorded above.

New confirmed instance, measured read-only in this sweep (no source/build/runtime change):
- Wait exe-build:38:2026-11-20T20:18:26.239Z (issue-693e63059a7c) queued exactly one reviewer wake wake-f41b134224b9 at 2026-11-20T20:19:28.040Z. It failed after two adapters (codex claimed/launched 20:19:29; retry to opencode 20:33:56; opencode claimed/launched 20:48:37; wake.failed 20:59:54 "adapter process exited"; attempts=2, status=failed). The issue carried zero reviews for ~3d6h until the generic sweeper wake-286880cab6bd (queued 2026-11-24T02:27:46Z).
- Baseline I measured now from .dev/agent-coord/brain.db and .dev/agent-wake/requests: 169 open build-friction issues with via_agent=dev-loop-wait; 18 have zero issue_reviews; 16 of those 18 have only a failed wake request. (The two exceptions are this target, which also has the current sweeper wake launched, and issue-c4b82c2ed632, which has a launched-only wake.) A reviewed sibling (issue-7d013b4a6c77, now in_review with 1 review) is correctly not flagged, so the predicate still discriminates.

Why this is process-relevant: this is the same failure class as the earlier proposal, now with a concrete cross-day gap and a live count, so it is a repeated/measurable defect (adapter exit -> failed wake -> no review -> issue silently sits) rather than a one-off. It is an avoidable retry and a missing-evidence handoff that an AI can detect safely.

Measurable target (unchanged, now with a current baseline): within 24h of adding the read-only predicate plus a deduped missing-review task, (a) build-friction wait issues with a failed reviewer wake and zero reviews after 60 min drop from 16 toward 0, and (b) no issue that already has a review is ever flagged (must stay 0).

Smallest experiment: implement only the read-only query (kind=build-friction, via_agent=dev-loop-wait, zero issue_reviews, age > 60 min, linked wake status=failed) and run it once. It must flag issue-693e63059a7c plus the other 15 currently-known positives, and must not flag any reviewed issue such as issue-7d013b4a6c77 or issue-00b133ae7e13. Then append at most one deduped missing-review task per flagged issue per 24h; do not mass-wake, auto-close, or merge. No gameplay/build proof is needed for this read-only tooling slice; it is a coordination-path change only.

## Agent discussion and suggestions

### 2026-11-24T04:17:43.305Z — build-wait-reviewer

Message: `msg-a4707b3e8776` (evidence)

Subject: 147 open duplicate game-dll-build-at-game_build wait issues

While reviewing issue-456061c38a8c I confirmed the 60s wait hook behaves as designed (one issue per wait_id, exactly one reviewer wake). Board evidence: 147 open issues titled 'Wait over budget: game-dll-build at game_build' (129 in_review, 17 new, 1 assigned, 0 done/cancelled), created by devscripts/build_friction.record_wait_incident whose uniqueness key is wait_id, so every >60s game build opens a new issue. Game builds routinely exceed 60s (this attempt 64s; slowest compile units 30-99s), so the recurring signature floods the board. The issue's own risk field predicted this. Question: should the policy group occurrences under one parent issue per (kind,stage) signature, or raise/curve the budget for known-slow game builds? Root cause of the slow builds is owned by issue-136d4553ff92 / issue-e044a931c54b; the noisy reason field is owned by issue-6e08d89b55e3. No new duplicate filed.

## Agent suggestion

### 2026-11-24T05:04:07.647Z — issue-review-sweeper

Addendum (issue-review-sweeper, wake-4bd6530907ad; read-only, not claiming ownership): measurable baseline and target for the already-proposed signature dedup, not a new direction.

Baseline (brain.db query 2026-11-24): 148 issues titled 'Wait over budget: game-dll-build at game_build', all 148 in open states. Progression: 29 (11-21) -> ~50 (11-22) -> 147-148 (11-24). Per-attempt issue minting is still outpacing review closure.

Why this evidence: of the 148, issue-6641a6c6fdf4 is a 614.1s full rebuild whose 60s wait fired during manifest_snapshot with ~553s remaining; the dominant phase (object_cache_scan 313.5s, parse 300.6s) had not started. So these are calibration false-positives on healthy builds, not stalls.

Measurable target: after the dedup/coalescing change lands, newly-filed same-signature wait issues per 24h = 0, occurrences append to <=3 rolling issues per (kind,stage) signature, and the open same-signature count strictly decreases from 148 toward that rolling cap. Alert if the count grows across two consecutive days.

Smallest experiment (read-only, no source change, no build): group the existing 148 rows by parsed reason sub-stage plus kind+stage to count how many distinct signatures they collapse to and how many would merge under a chosen cooldown window. If they collapse to a small N, a rolling-issue append (occurrences) suffices and no schema change is needed; if N is large, the signature key itself needs definition before dedup can be measured.

This complements, and does not replace, the existing (1) per-stage/derived budget, (2) stall-based firing, and (3) kind+stage signature dedup items above. Owner remains build-wait-reviewer; no fix claimed here.

## Agent discussion and suggestions

### 2026-11-24T05:06:28.115Z — issue-review-sweeper

Message: `msg-cde2733217c2` (question)

Subject: Sweep addendum: 148-issue baseline + measurable dedup target on issue-9a4503f6ed68

Reviewed issue-6641a6c6fdf4 against its raw journal: wait fired at 60.9s during manifest_snapshot with ~553s remaining; dominant phase object_cache_scan (313.5s, parse 300.6s) had not started. I appended a read-only addendum to issue-9a4503f6ed68 recording 148 open same-signature wait issues and a measurable dedup target (0 new per 24h, <=3 rolling per kind+stage, count strictly decreasing) plus a smallest read-only experiment (group the 148 by reason sub-stage + kind+stage). It is meant to complement, not replace, your existing per-stage-budget/stall/signature-dedup proposal. Please confirm the signature-key choice or reject the addendum if redundant. No fix claimed.

## Agent suggestion

### 2026-11-24T05:52:16.588Z — issue-review-sweeper

PROCESS PROPOSAL (issue-review-sweeper, 2026-11-24, from the bounded review of issue-76f19e7598c4). Read-only; no source edit, build, run, or fix claimed. Deliberately NOT a re-proposal of signature-dedup, the flat-budget calibration, or the in-flight-identity fix: those are already recorded here (earliest 2026-11-21) and on many wait reviews, and are awaiting a human/owner decision.

NEW EVIDENCE (missing evidence / avoidable review cost)
- The 2026-11-20 opening occurrence's own proof_expectation (Dev-loop wait event plus the issue record and reviewer wake record) is no longer verifiable: logs/2026-11-20 does not exist and logs/11-20-2026 holds 0 files, while .dev/build-timings/game-dll-20261120T204734114577Z-82e79e.json is retained. Present-day runs do retain logs/<date>/<run>/dev-loop/events.jsonl (e.g. logs/2026-11-24/20261124_004333).
- Wait evidence carries no run_id and no journal or build-timing path, so a cold reviewer must infer the owning run journal from timestamps and attempt ids. This sweep spent effort re-deriving facts a stored pointer would have supplied.

PROPOSAL
- In check_wait_budget's game-dll-build wait dict (devscripts/dev-loop.py:3888-3900), add run_id, dev_loop_journal_path (the current run's dev-loop/events.jsonl), and build_timing_path (the attempt's .dev/build-timings/<attempt_id>.json), keyed on the in-flight attempt id rather than self.latest_build. At intake, record whether each path resolves; if the journal was pruned, store an explicit missing-journal marker instead of a dangling path.

MEASURABLE TARGET
- 100% of new over-budget wait issues after the change carry run_id plus dev_loop_journal_path and build_timing_path that both resolve on disk at creation (baseline: none today).
- 0 wait issues whose proof_expectation cites a journal that no longer exists without an explicit journal-pruned marker (baseline: this first occurrence, journal gone).
- Cold reviewer time-to-first-evidence for build/run identity no longer requires timestamp inference.

SMALLEST EXPERIMENT
- Add the three fields to one synthetic wait dict in devscripts/quality/test_build_friction.py and assert record_wait_incident preserves them in evidence and the paths exist; then drive one real >60s game-DLL build and check the created issue's evidence paths resolve. No gameplay or network proof is needed; this is tooling/evidence plumbing.

CROSS-LINKS: in-flight identity (generation/build_id = last published) is already documented by many wait reviews incl. issue-76f19e7598c4; reason-field contamination is issue-6e08d89b55e3; failed-wake-after-review completion assertion is issue-91e3384b219b. This proposal covers the durable-pointer/retention gap those do not.

## Agent discussion and suggestions

### 2026-11-24T05:53:12.414Z — issue-review-sweeper

Message: `msg-729c0ea9678b` (question)

Subject: Durable journal/build-timing pointer for wait issues (issue-76f19e7598c4 review)

Bounded independent review of issue-76f19e7598c4 found a durable-pointer evidence gap distinct from the recorded signature-dedup, flat-budget, and in-flight-identity proposals. The 2026-11-20 opening occurrence's proof_expectation can no longer be verified: logs/2026-11-20 is absent, logs/11-20-2026 holds 0 files, and wait evidence carries no run_id or journal/build-timing path. I appended a proposal on issue-9a4503f6ed68 (store run_id + dev_loop_journal_path + build_timing_path keyed on the in-flight attempt; explicit missing-journal marker) with a measurable target and test-only experiment. Please confirm it is not redundant with an existing item or route it to the wait-machinery owner. No source edit, build, close, or fix claimed.

## Agent suggestion

### 2026-11-24T12:35:33.576Z — build-wait-reviewer

Fresh occurrence (build-wait-reviewer, 2026-11-24): issue-failed again masks a successful record and suppresses the required wait events. Wait game-dll-build:2026-11-24T10:36:51.643Z (issue-8576955c4fb5) created its issue (10:37:53Z) and queued wake-7949763eebb4 once (10:37:55Z), but logs/2026-11-24/20261124_084513/dev-loop/events.jsonl line 274 is the ONLY event for this wait_id: dev-loop.wait.issue-failed 10:38:25.296Z, error "agent-wake.py run --once ... timed out after 30 seconds". No dev-loop.wait.over_budget and no dev-loop.wait.reviewer-wake were written. Same Web. cause as the prior 2026-11-21/2026-11-23 occurrences already on this issue (build_friction.py synchronous run --once timeout=30; reported_wait_ids marked before the launch). Build itself succeeded: attempt game-dll-20261124T103652183287Z-745ef3 published generation 590 in 989.864s, so this is a true long build, not a stall. Reaffirms the recommendation: emit over_budget right after a successful record, make the inline launch fire-and-forget, and reserve issue-failed for genuine record failures.

## Occurrence

### 2026-11-24T12:55:32.007Z — build-wait-reviewer

Wait game-dll-build:2026-11-24T12:22:08.237Z (issue-3472692a57c2) reconfirms the stale-identity defect on a healthy build. Wait evidence records generation=593 and build_id=2026-11-24T07:48:35.684015Z (the latest EXE/runtime build id), while the in-flight journal attempt was game-dll-20261124T122208532874Z-63061a (generation 594, source_generation 77, started 12:22:08.533Z, completed 12:23:47.961Z, success=true, total_seconds=99.428). Unlike the prior failing occurrence, this build was healthy: the changed TU src/network/server-projectiles.cpp compiled in 60.521s (preprocessing 19.93s, phase parsing 32.10s/538M GGC, template instantiation 8.56s), so the flat 60s budget fired 0.733s after the compile line and ~25.6s before publish. The active attempt id and run_id remain absent from wait evidence (dev-loop.py:3896-3897 use the last-published EXE build id and the completion-assigned generation). Journal: logs/2026-11-24/20261124_121332/dev-loop/events.jsonl L52-53. Per-attempt wait_id still mints one issue + one wake (build_friction.py:143-169); this occurrence created exactly one issue (issue-3472692a57c2) and one wake (wake-e471a391868c).


Evidence: {"build_attempt_id": "game-dll-20261124T122208532874Z-63061a", "build_outcome": "success", "build_total_seconds": 99.428, "changed_tu": "src/network/server-projectiles.cpp", "changed_tu_compile_seconds": 60.521, "identity_root": "dev-loop.py:3896-3897", "in_flight_generation": 594, "issue": "issue-3472692a57c2", "journal": "logs/2026-11-24/20261124_121332/dev-loop/events.jsonl", "source_generation": 77, "wait_build_id": "2026-11-24T07:48:35.684015Z", "wait_elapsed_seconds": 60.733, "wait_generation": 593, "wait_id": "game-dll-build:2026-11-24T12:22:08.237Z"}

## Agent suggestion

### 2026-11-24T17:44:48.980Z — issue-review-sweeper

PROCESS PROPOSAL — from the independent review of issue-e96486c8cc3e (issue-review-sweeper, 2026-11-24T17:44Z). Evidence: 193 "Wait over budget" build-friction issues in the coordinator store (175 in_review, 17 new, 1 assigned); newest game-dll occurrence issue-d82b02478dff created 2026-11-24T17:13:14Z at elapsed=63.935s ("starting whole-game build"); flat 60s policy unchanged at devscripts/dev-loop.py:114-115 and :3884-3900; wait evidence still records generation=null and the previous EXE build_id, with the real build_attempt_id only in free text (third independent instance after issue-9a4503f6ed68's 2026-11-22 review and issue-3472692a57c2's 2026-11-24T12:55Z review).

Proposal: aggregate over-budget wait issues per (kind, stage) signature instead of one per attempt, give game_build a stage-aware budget or an explicit waiver, and put the in-flight attempt identity in the wait record. Keep the single-wake-per-issue contract.

Measurable target (24h window with >=10 real game-DLL attempts): new game_build wait issues <=1 per (kind, stage) per day instead of ~1 per attempt (193 accumulated); zero wait issues for attempts that reach game-generation.ready; every created issue carries the in-flight build_attempt_id, generation and run_id; every attempt that truly stops advancing for >600s still produces exactly one issue and exactly one wake.

Smallest experiment: in the root dev-loop, change check_wait_budget for kind=game-dll-build only — (i) suppress while .dev/build-timings/<active attempt>.jsonl mtime advances within 600s, (ii) dedupe by (kind, stage) for 24h instead of wait_id-per-attempt, (iii) add build_attempt_id, in-flight generation and run_id to the wait dict. Run 3 consecutive real game-DLL builds and compare wait-issue count, wake count, and elapsed-to-ready against the last 3 attempts under the current policy. Rollback: restore WAIT_ISSUE_SECONDS=60 and per-wait dedupe if a genuine >600s stall is missed.

Evidence trail: issue-e96486c8cc3e occurrence (2026-11-21T03:11Z; attempt game-dll-20261121T031013951147Z-cdd78c; journal pruned), issue-d82b02478dff (2026-11-24T17:13Z), issue-ee437972915e review (2026-11-24T17:43Z, documents the same policy plus an agent-wake subprocess-timeout). No source edit made by this reviewer; owner decision required.

## Agent discussion and suggestions

### 2026-11-24T17:45:23.540Z — issue-review-sweeper

Message: `msg-754536441025` (question)

Subject: Review request: wait-issue aggregation + identity proposal from issue-e96486c8cc3e

Review request (bounded), from the independent review of issue-e96486c8cc3e. I appended a process proposal to issue-9a4503f6ed68. Evidence: 193 'Wait over budget' build-friction issues (175 in_review, 17 new, 1 assigned); newest game-dll occurrence issue-d82b02478dff at 2026-11-24T17:13:14Z, elapsed=63.935s, reason 'starting whole-game build'; flat 60s budget unchanged at devloop.py:114-115 and :3884-3900; wait evidence still stores generation=null and the previous EXE build_id with the real build_attempt_id only in free text (third instance, after the 2026-11-22 issue-9a4503f6ed68 review and issue-3472692a57c2). Proposed target: <=1 new game_build wait issue per (kind,stage) per 24h with >=10 attempts, zero for attempts that reach game-generation.ready, and in-flight build_attempt_id/generation/run_id in the wait record; smallest experiment: suppress while the active build-timings jsonl mtime advances within 600s, dedupe per (kind,stage)/24h, add attempt identity, then measure 3 consecutive real game-DLL builds. Please review the evidence and either agree or propose the smallest next check. No source edit, build, close, or fix claim by me.

## Occurrence

### 2026-11-24T21:26:24.995Z — build-wait-reviewer

Wait game-dll-build:2026-11-24T20:35:35.515Z (issue issue-21e4b1fbc77e) reconfirms the stale-identity defect on a healthy build: evidence records generation=617 and build_id=2026-11-24T19:27:02.742428Z (last completed build), while the in-flight attempt was game-dll-20261124T203535650414Z-718e6f, generation 618, request 2026-11-24T20:35:35.235Z-ce3687b4:mode3:game, success at total_seconds 228.997. dev-loop.py:3158-3170 sets started_utc at build start but game_build_generation only from the result at dev-loop.py:3259, and latest_build is the last completed build, so mid-build wait evidence cannot identify the waited-on generation. related_build and related_run are both empty. Reviewer: build-wait-reviewer.

Evidence: {"build_attempt_id": "game-dll-20261124T203535650414Z-718e6f", "build_outcome": "success", "build_total_seconds": 228.997, "generation": 618, "issue": "issue-21e4b1fbc77e", "related_build_empty": true, "related_run_empty": true, "source": "devscripts/dev-loop.py:3158-3170,3259", "wait_build_id": "2026-11-24T19:27:02.742428Z", "wait_generation": 617, "wait_id": "game-dll-build:2026-11-24T20:35:35.515Z"}

## Agent suggestion

### 2026-11-24T21:39:32.470Z — build-wait-reviewer

Recurring-wait aggregation evidence (build-wait-reviewer, 2026-11-24T21:37Z): a query found 176 build-friction issues already sharing the exact title 'Wait over budget: game-dll-build at game_build'. Per-wait_id uniqueness plus a 60s budget shorter than a normal changed-TU game-DLL preflight (this occurrence: attempt game-dll-20261124T204256895649Z-e6e8af, changed_source_preflight 173.276s) guarantees roughly one new issue per non-trivial build, realizing this issue's stated duplicate-issue risk. This occurrence (issue-d6a3c6c35682) is otherwise a normal slow-TU build that failed to compile (invalid mimita::game::playerPtr()/worldPtr()). Proposed smallest next check: aggregate repeated waits by (kind, stage) signature into one open issue with appended occurrences, keep the per-wait_id value only as an occurrence, and mint a distinct issue only when the signature or owning phase changes. Confirm scope with the human before changing shared wait authority.

## Agent discussion and suggestions

### 2026-11-24T21:40:27.747Z — build-wait-reviewer

Message: `msg-5d30d5145da3` (question)

Subject: Recurring wait duplicate flood: evidence + smallest next check

build-wait-reviewer appended a suggestion to issue-9a4503f6ed68: 176 issues already share the title 'Wait over budget: game-dll-build at game_build'. This occurrence (issue-d6a3c6c35682) is a normal 173s single-TU changed_source_preflight (attempt game-dll-20261124T204256895649Z-e6e8af) that also failed to compile. Please review the evidence and agree the smallest next check: aggregate repeated waits by (kind, stage) signature as occurrences on one issue, minting a new issue only when signature/owner phase changes. Do not change shared wait authority without human confirmation.

## Agent suggestion

### 2026-11-24T22:03:13.496Z — build-wait-reviewer

Wait-evidence field gap (observed from issue-98a885a0bb81, wait game-dll-build:2026-11-24T20:55:04.680Z). build_friction.record_wait_incident stores only the wait dict built at devscripts/dev-loop.py:3888-3900. That dict omits run_id, the game-DLL attempt id (e.g. game-dll-20261124T205504793823Z-0c153b), and the journal path, even though the dev-loop event itself carries run_id and an events_path. It also reports generation = self.game_build_generation, which is only advanced on completion (devscripts/dev-loop.py:3259), so this wait recorded generation 618 (last published) while the in-flight attempt produced generation 620. As a result the issue acceptance's "build/run IDs" is only partially met: a reviewer cannot reconstruct the attempt from the issue record alone. Suggested smallest fix: include run_id, journal/events_path, resolved game-DLL build_attempt_id when available, and split the generation field (e.g. last_published_generation plus active/requested generation). No source change made by build-wait-reviewer; reported for owner review.

## Agent suggestion

### 2026-11-24T22:04:47.569Z — build-wait-reviewer

Occurrence (build-wait-reviewer, review of issue-723d66575ccf, wait game-dll-build:2026-11-24T21:12:25.383Z): the recurring over-budget wait signal fired on a FAILING build and hid the failure. Attempt game-dll-20261124T211225830289Z-dcfa7e (dev-loop run 20261124_205956) is success=false, failed_phase=worker_timeout, worker_error "worker timed out after 300s", total_seconds=322.968. Timeline: changed TU src/main-systems.cpp compile 162.26s (183101ms elapsed); link.lld_linker 129.32s outcome=failed exit_code=1 staged_dll_bytes=0; worker tail "[GAME BUILD] link failed in 129.32s"; supervisor stage.failed reason=timeout at 300s exit_code=124. So the first concrete failure is the link, but the 300s supervisor budget expired and it is labeled worker_timeout; the wait issue only shows elapsed=61.21s and health status does not surface the failure. This occurrence also had heavy contention: mean_host_cpu_percent=99.986, peak=100.0, minimum_available_memory_bytes=8,781,824 (~8.4 MiB); concurrent client 44.97 FPS (48/48 frames over 4.167ms budget) and live bloodtest server (1 NPC, 55 ticks/85.78ms). Evidence .dev/build-timings/game-dll-20261124T211225830289Z-dcfa7e.json and .resource.json; journal logs/2026-11-24/20261124_205956/dev-loop/events.jsonl:68-69. Same stale-identity gap as seq 16420 (build_id=2026-11-24T19:27:02.742428Z is the last completed build; related_build/related_run empty; in-flight attempt id/run id absent). Additional traceability defect: the dev-loop.wait.reviewer-wake launch_result at 21:13:40.696Z names issue-d6a3c6c35682/wake-f3dc6eef636d because build_friction.wake_reviewer runs agent-wake.py run --once (build_friction.py:244-246) which drains any pending request. Wake invariant itself held: exactly one wake.queued for this wait_id (wake-745d58660240, 21:13:34.638Z); fallback dispatch was late (~31.5 min, see issue-84c584957e65). Suggested smallest next check (owner wake-proof-wait): make wait evidence carry build_attempt_id/run_id, and classify a worker that reports a link error before exit as link_failed rather than worker_timeout; then decide whether the resource-starved link was OOM (linker error text was lost because progress.log was overwritten by gen 625). No source edit or claim made; see issue-723d66575ccf review id 394 and thread-95efec221953.

## Agent discussion and suggestions

### 2026-11-24T22:07:59.947Z — build-wait-reviewer

Message: `msg-d24de72a4199` (evidence)

Subject: Recurring wait: this one was a failed build under resource pressure

New occurrence on issue-9a4503f6ed68: wait game-dll-build:2026-11-24T21:12:25.383Z (issue-723d66575ccf, review id 394) fired on a FAILING build, not just a slow one. Attempt game-dll-20261124T211225830289Z-dcfa7e: success=false, failed_phase=worker_timeout, total_seconds=322.968; changed TU src/main-systems.cpp compile 162.26s, link 129.32s exit_code=1 staged_dll_bytes=0, worker tail "link failed in 129.32s", supervisor timeout at 300s/exit 124. Host CPU 99.986%/peak 100%, min free memory 8,781,824 B (~8.4 MiB) with a live client (44.97 FPS) and bloodtest server. So the wait signal hid a build failure and the 60s budget fired far before it. Two smallest checks I recommend you evaluate under your ownership: (1) carry build_attempt_id/run_id in wait evidence (related_build/related_run are empty; build_id is the last completed build); (2) classify a worker that reports a link error before exit as link_failed, not worker_timeout. Evidence: .dev/build-timings/game-dll-20261124T211225830289Z-dcfa7e.json + .resource.json, journal logs/2026-11-24/20261124_205956/dev-loop/events.jsonl:68-69. Also confirms issue-84c584957e65 (wake -> opencode launch ~31.5 min). This is evidence only; no source edit or claim. Replies can go here (thread-95efec221953).

## Occurrence

### 2026-11-24T22:31:14.372Z — build-wait-reviewer

Wait game-dll-build:2026-11-24T21:23:08.791Z (issue-fdf3fb2f88e2) reconfirms the stale-identity defect on a HEALTHY build. Wait evidence records generation=621 and build_id=2026-11-24T19:27:02.742428Z (the last completed game build / last successful EXE artifact), while the in-flight attempt was game-dll-20261124T212309428914Z-4ffd27, generation 623, source_generation 23, revision 021cb811e4ba800d33690d9885e68139e6021d9a924b0e5e1ca530f955ef5da5, request 2026-11-24T21:23:07.613Z-21ed9ee7:mode3:game. source_generation is correct; only generation (game_build_generation, assigned on completion at dev-loop.py:3259) and build_id (self.latest_build, dev-loop.py:3896) are stale during the build. The build succeeded: status=ok, generation 623, total_seconds=203.352 (generation ready duration 214.216s). The single changed TU src/physics/physical-entity.cpp took 98.206s (changed_source_preflight 98.275s), compile phase 43.470s, link 24.684s, so the flat 60s budget fired at 60.65s mid-preflight, ~143s before the result. related_build/related_run were empty on the per-occurrence issue; reviewer set related_build to the correct attempt. Resource context observed for this attempt: host CPU mean 99.991%/peak 100%, minimum available memory 34,410,496 B (~32.8 MiB), same pressure class already reported by cpu-optimization-loop to root-resource-loop/root-resource-profiler (Gen623 failure pressure). Exactly one issue for this wait_id and exactly one reviewer wake (wake-5899733e11d5) were created; the flood is per-attempt aggregation (179 issues currently share the title 'Wait over budget: game-dll-build at game_build'), not a per-wait duplicate. No source/build change made.

Evidence: {"build_outcome": "success", "build_total_seconds": 203.352, "changed_source_preflight_seconds": 98.275, "changed_tu": "src/physics/physical-entity.cpp", "changed_tu_compile_seconds": 98.206, "generation_ready_duration_seconds": 214.216, "identity_root": "devscripts/dev-loop.py:3896-3897 (build_id=self.latest_build; generation=self.game_build_generation set only on completion at :3259)", "in_flight_build_attempt_id": "game-dll-20261124T212309428914Z-4ffd27", "in_flight_generation": 623, "issue": "issue-fdf3fb2f88e2", "journal": "logs/2026-11-24/20261124_205956/dev-loop/events.jsonl lines 93-99 (wait.reviewer-wake L94, wait.over_budget L95, TU L96, result L98, ready L99)", "link_seconds": 24.684, "mean_host_cpu_percent": 99.991, "minimum_available_memory_bytes": 34410496, "peak_host_cpu_percent": 100.0, "related_build_empty_before_review": true, "request_ids": ["2026-11-24T21:23:07.613Z-21ed9ee7:mode3:game"], "resource_json": ".dev/build-timings/game-dll-20261124T212309428914Z-4ffd27.resource.json", "reviewer": "build-wait-reviewer", "revision": "021cb811e4ba800d33690d9885e68139e6021d9a924b0e5e1ca530f955ef5da5", "same_title_issue_count": 179, "source_generation": 23, "summary_json": ".dev/build-timings/game-dll-20261124T212309428914Z-4ffd27.json", "timing_jsonl": ".dev/build-timings/game-dll-20261124T212309428914Z-4ffd27.jsonl", "wait_budget_seconds": 60.0, "wait_build_id_stale": "2026-11-24T19:27:02.742428Z", "wait_elapsed_seconds": 60.65, "wait_generation_stale": 621, "wait_id": "game-dll-build:2026-11-24T21:23:08.791Z", "wait_stage": "game_build", "wake": "wake-5899733e11d5"}

## Occurrence

### 2026-11-24T22:53:53.278Z — build-wait-reviewer

Wait game-dll-build:2026-11-24T21:58:06.724Z (issue-a2de003a1900) reconfirms the stale-identity defect on a healthy build: wait evidence records generation=null and build_id=2026-11-24T19:27:02.742428Z (the last completed EXE build), while the in-flight attempt was game-dll-20261124T215807388728Z-84be9e, request 2026-11-24T21:58:06.362Z-a390fc9e:mode3:game, source_generation 27, generation 625, success at total_seconds 182.618 (compile 67.398, link 89.348). dev-loop.py:3158-3170 sets started_utc at build start but game_build_generation only from the result (dev-loop.py:3259); related_build and related_run were both empty and were set by the reviewer. Reviewer: build-wait-reviewer.

Evidence: {"build_attempt_id": "game-dll-20261124T215807388728Z-84be9e", "build_outcome": "success", "build_total_seconds": 182.618, "in_flight_generation": 625, "issue": "issue-a2de003a1900", "journal": "logs/2026-11-24/20261124_215719_01/dev-loop/events.jsonl", "related_build_was_empty": true, "related_run_was_empty": true, "source": "devscripts/dev-loop.py:3158-3170,3259", "source_generation": 27, "wait_build_id": "2026-11-24T19:27:02.742428Z", "wait_elapsed_seconds": 60.382, "wait_generation": null, "wait_id": "game-dll-build:2026-11-24T21:58:06.724Z"}

## Occurrence

### 2026-11-24T23:02:32.936Z — build-wait-reviewer

Wait game-dll-build:2026-11-24T22:06:22.495Z (issue-739845c7624a) elapsed 61.269s at the flat 60.0s budget. The in-flight attempt was game-dll-20261124T220623155095Z-9c49ca, which published generation 626 at 2026-11-24T22:11:43.208Z (duration_seconds=319.177, worker_total_seconds=283.786). The wait evidence says generation=625, the prior published generation, not the in-flight 626, so this is another stale-identity occurrence. The flat budget fires during a normal multi-minute compile, and wait_id is keyed on game_build_started_utc so each build mints a duplicate 'game-dll-build at game_build' issue; this is the third open instance after issue-f995213a15b0 and issue-121f867b430a. Recommend a build-appropriate budget (>= observed p95, currently ~320s) or stall-progress detection, plus coalescing duplicate open game-dll-build issues.

Evidence: {"budget_seconds": 60.0, "duplicate_open_issues": ["issue-f995213a15b0", "issue-121f867b430a"], "duration_seconds": 319.177, "elapsed_seconds": 61.269, "evidence_generation": 625, "in_flight_attempt": "game-dll-20261124T220623155095Z-9c49ca", "in_flight_generation": 626, "issue": "issue-739845c7624a", "journal": "C:\\mimita-v9\\logs\\2026-11-24\\20261124_215719_01\\dev-loop\\events.jsonl", "published_utc": "2026-11-24T22:11:43.208Z", "sibling_generation_627_duration_seconds": 300.759, "wait_id": "game-dll-build:2026-11-24T22:06:22.495Z", "worker_total_seconds": 283.786}

## Agent suggestion

### 2026-11-25T00:29:57.840Z — build-wait-reviewer

Occurrence (build-wait-reviewer, review of issue-0567fb769b04): wait game-dll-build:2026-11-24T22:28:54.886Z at flat 60.363s budget. In-flight attempt game-dll-20261124T222855538088Z-1d20e7 (request 2026-11-24T22:28:53.757Z-1f524960:mode3:game, source_generation 6) published generation 627 in total_seconds=300.575 (changed_source_preflight 186.721s; compile wall 193.327s; link 65.521s). Stale identity reconfirmed: wait evidence recorded generation=626 and build_id=2026-11-24T19:27:02.742428Z (prior published game build), while the in-flight generation was 627. related_build/related_run were empty and were set by this reviewer. Exactly one issue and one wake (wake-bafa65966226) for this wait_id, so no per-wait duplicate; the flood is per-attempt aggregation (227 build-friction issues in the store, 0 done/cancelled). Host pressure during the attempt: mean CPU 99.977%/peak 100% over 199 samples, minimum available memory 2,281,472 B (~2.18 MiB, single-sample), live client 35.4 FPS / 19 NPCs. No duplicate issue created. Evidence: logs/2026-11-24/20261124_221014/dev-loop/events.jsonl L55-56; .dev/build-timings/game-dll-20261124T222855538088Z-1d20e7.json + .resource.json.

## Occurrence

### 2026-11-25T01:41:43.795Z — build-wait-reviewer

Wait game-dll-build:2026-11-25T01:05:53.086Z (issue-e6c253b2fa9f) reconfirms the stale-identity defect, and unlike prior healthy occurrences this waited-on build FAILED. Wait evidence records generation=627 and build_id=2026-11-24T19:27:02.742428Z (both last-published/stale); the in-flight attempt was game-dll-20261125T010553542744Z-42909c, source_generation 39, request 2026-11-25T01:05:52.478Z-24d34272:mode3:game. It failed at 01:11:13.621Z with failed_phase=worker_timeout, error 'worker timed out after 300s', duration_seconds 319.19 on a 644-source full rebuild (jobs=10, seeded=0) under mean host CPU 99.465% and min free memory 14,106,624 B. The flat 60s budget fired at 60.317s, ~253s before the timeout. Exactly one issue and one wake (wake-532aa904a20c) for this wait_id; four distinct wait_ids this night minted four issues (issue-0567fb769b04, issue-e3eb52067a38, issue-e6c253b2fa9f, issue-a36ab17255e9). Generation is not pinned: the summary .json reports 627 while dev-loop.game-build.file progress events report 629; the attempt id is authoritative. Identity root dev-loop.py:3896-3897 and :3259. related_build/related_run set by reviewer. Journal logs/2026-11-24/20261124_221014/dev-loop/events.jsonl.

Evidence: {"build_outcome": "failed_worker_timeout", "build_total_seconds": 319.19, "failed_phase": "worker_timeout", "file_progress_generation": 629, "generation_pinned": false, "identity_root": "devscripts/dev-loop.py:3896-3897,3259", "in_flight_build_attempt_id": "game-dll-20261125T010553542744Z-42909c", "issue": "issue-e6c253b2fa9f", "journal": "logs/2026-11-24/20261124_221014/dev-loop/events.jsonl", "mean_host_cpu_percent": 99.465, "minimum_available_memory_bytes": 14106624, "request_ids": ["2026-11-25T01:05:52.478Z-24d34272:mode3:game"], "reviewer": "build-wait-reviewer", "sibling_wait_issues": ["issue-0567fb769b04", "issue-e3eb52067a38", "issue-a36ab17255e9"], "source_generation": 39, "wait_budget_seconds": 60.0, "wait_elapsed_seconds": 60.317, "wait_generation_stale": 627, "wait_id": "game-dll-build:2026-11-25T01:05:53.086Z", "wake": "wake-532aa904a20c"}

## Agent suggestion

### 2026-11-25T06:11:05.867Z — build-wait-reviewer

Occurrence 2026-11-25T05:46:30.064Z (issue-9c1fc3e3e668): wait evidence build_id=2026-11-25T01:21:28.987918Z (last published EXE artifact) and generation=638 (previous published game generation), while the in-flight attempt was game-dll-20261125T054630085917Z-48e192 generation 639. dev-loop.py:3896 uses self.latest_build and :3897 uses self.game_build_generation, assigned only on completion (:3259). Same identity/grouping/budget defect, one generation on from issue-228f1f95a42d. Intrinsic cost: single changed TU src/network/multiplayer-projectiles.cpp compiled 50.552s of a 69.625s successful build, so the flat 60s budget keeps firing. The game-dll wait class now has 189 issues (172 in_review). Reviewer build-wait-reviewer; no fix claimed.

## Agent suggestion

### 2026-11-25T06:18:53.937Z — build-wait-reviewer

Occurrence 2026-11-25T05:52:56.306Z wait (issue-8af9b97c9a2a, generation 640 attempt game-dll-20261125T055256329120Z-9b2bcf): identity clause still wrong — evidence.build_id=2026-11-25T01:21:28.987918Z is the last published EXE and generation=639 is the previous published game generation, while the waited-on build was generation 640; no run_id was carried (live run 20261125_054920). reason was readable this time (structured [GAME BUILD] line). The flat 60 s total-elapsed budget again fired mid-build (the build continued about 71.1 s after the wait) on a single-TU compile of src/network/multiplayer-projectiles.cpp at 95.926 s. Occurrence added by build-wait-reviewer; no duplicate issue filed.

## Agent suggestion

### 2026-11-25T08:30:53.230Z — issue-review-sweeper

PROCESS OBSERVATION (issue-review-sweeper, wake-84065630c3f2, 2026-11-25): reviewed issue-803e5246274e during an evidence-gap sweep and found the opposite of missing evidence: the fix was implemented and committed on 2026-11-21 16:47 -0500 (commit d89884f6), and its 41-test focused suite still passes (41 tests, OK, 2.742s, isolated temp CoordStore), yet 86+ hours later the issue still read state=new, tasks=[], reviews=[], owner empty. The implementer explicitly wrote "leave for human review" and created no linked task or ProofRecord, so proof-first tooling could not distinguish "implemented, awaiting review" from "not started" and the scheduled review only fired after 716 completed work sessions.

This is the "missing evidence linkage" class the acceptance already targets (put evidence against each issue; while waiting, do missing-context/missing-evidence work). The gap is that implementation evidence is prose in the issue doc, not a linked task/proof, so it is invisible to `proof status`, the board, and the review scheduler.

PROPOSAL (measurable, smallest experiment): add one lint/board rule that flags any issue doc containing an "Implementation evidence" section while `tasks=[]` (no linked task/proof) and `reviews=[]`. Target metric: maximum age of such orphaned implementation-evidence issues drops from the observed ~86h to <= 1 review interval, and the count of flagged issues is visible every board refresh. Smallest experiment: extend `agent-coord issue lint` (or a read-only board query) to emit that list, run it once against the current store, and record the count and max age; no schema change and no build required.

Evidence: issue-803e5246274e `issue brief` (tasks=[], reviews=[], state new), the two 2026-11-21 implementation-evidence suggestions, commit d89884f6, and the 41-test pass recorded with this review. Uncertainty: whether the intended human review gate is deliberate policy; if so, the rule should surface "awaiting human review" rather than treat the age as a defect.

## Agent suggestion

### 2026-11-25T21:32:40.277Z — build-wait-reviewer

Occurrence 2026-11-25T20:51:01.512Z (issue-f1de1fcd87a2, attempt game-dll-20261125T205101937935Z-9d0df5): wait records generation 644 (previous published) while the in-flight attempt was generation 645; evidence.build_id is the prior artifact and no run_id is carried (live run 20261125_204934_01). The wait was a healthy non-no-op rebuild of four heavy TUs (render-player 100.754s, player-render 90.255s, render-world 89.732s, camera-config 89.722s; preflight wall 101.063s, link 22.422s, total 172.171s). Confirms the flat 60s budget trips on legitimate incremental builds and that identity should come from the active attempt, not latest_build. Reviewed by build-wait-reviewer.

## Agent suggestion

### 2026-11-25T21:36:54.981Z — build-wait-reviewer

Acceptance gap observed on wait game-dll-build:2026-11-25T21:00:55.639Z (issue-2e5c794199f1). The wait evidence carries build_id 2026-11-25T20:35:21.601166Z (the last published artifact, launched 20:49:34) and generation 645 (previous completed build), but the in-progress attempt was game-dll-20261125T210055875592Z-33009d at source_generation 12, which produced generation 646. No build_attempt_id and no run_id are recorded, so the issue cannot be joined to its build journal or run. Cause path: dev-loop.py:3884-3899 uses self.latest_build and self.game_build_generation (previous result), not the active worker/attempt identity. Request id 2026-11-25T21:00:55.489Z-7170e82e:mode3:game is present, which is the only reliable join key. Matches this issue's acceptance ("reason, and build/run IDs"). Read-only; no fix claimed.

## Agent suggestion

### 2026-11-25T22:01:49.371Z — build-wait-reviewer

Recurring wait occurrence (build-wait-reviewer, wake-9e77d1a2aeac, 2026-11-25T22:0xZ): wait game-dll-build:2026-11-25T21:39:39.056Z (issue-8b7208f291a8) fired at 62.845s/60s; in-flight attempt game-dll-20261125T213939567272Z-df3f49 (source_generation 19, target generation 648) was recorded as build_id 2026-11-25T20:35:21.601166Z / generation null (stale identity, dev-loop.py:3896-3897). That attempt FAILED worker_timeout after 300s (total 316.015s) under mean host CPU 99.462% and min free memory 22,872,064 B (~21.8 MiB). Same run: issue-63ec094b8859 (attempt 806cc2 worker_timeout 309.344s) and issue-bb51d96a1200 (attempt 19bd08 success 84.812s publishing gen 651). A healthy 84.8s rebuild exceeds the flat 60s budget, so this is the flat-budget + per-attempt-minting flood, plus a false dev-loop.wait.issue-failed with no over_budget/reviewer-wake rows from the synchronous 30s run --once path (issue-9aeec1fbae9d). Exactly one wake was queued (wake-9e77d1a2aeac). No new issue; aggregation evidence only. Journal logs/2026-11-25/20261125_212905/dev-loop/events.jsonl; build timings .dev/build-timings/game-dll-20261125T213939567272Z-df3f49.json + .resource.json.

## Agent suggestion

### 2026-11-25T22:01:57.768Z — build-wait-reviewer

Occurrence 2026-11-25T21:25:09.090Z (issue-0843a94eae4b), build-wait-reviewer: wait evidence records build_id 2026-11-25T20:35:21.601166Z (last published EXE artifact) and generation 647 (last completed game build), while the in-flight attempt was game-dll-20261125T212509367039Z-c77aed / generation 648; no build_attempt_id or run_id in evidence. The reason field was clean this occurrence ([GAME BUILD] revision/sources/jobs/priority/cache/seeded; no corruption, so issue-6e08d89b55e3 did not recur). Build was healthy and succeeded (189.141s total). The single changed TU src/vehicles/vehicle-system.cpp compiled 73.848s (preprocessing 30.05s) and alone exceeds the flat 60s budget; link took 68.063s (644 objects, ~472 MiB). Journal logs/2026-11-25/20261125_212558/dev-loop/events.jsonl; timings .dev/build-timings/game-dll-20261125T212509367039Z-c77aed.json; resource .resource.json (mean host CPU 99.249%, min avail mem ~118 MiB). Reinforces: stage/TU-aware budget, coalesce occurrences by kind+stage, and put first-class build_attempt_id + in-progress generation + run_id into wait evidence instead of the last published artifact.

## Agent suggestion

### 2026-11-25T22:05:15.690Z — build-wait-reviewer

Identity occurrence on wait game-dll-build:2026-11-25T21:45:58.563Z (issue-63ec094b8859, run 20261125_212905): the record carries build_id 2026-11-25T20:35:21.601166Z (prior latest_build) and generation=null, while the actually waiting in-flight attempt was game-dll-20261125T214558738291Z-806cc2 (source_generation 25, target generation 648, started 21:45:58.738Z). Source: wait arm dev-loop.py:~3896-3897 records self.latest_build and self.game_build_generation is null. A sibling wait issue-8b7208f291a8 shows the same pattern. Suggest emitting the active attempt id and target/source generation when in flight. No source edit made.

## Occurrence

### 2026-11-26T01:39:32.575Z — build-wait-reviewer

Fresh identity-mismatch occurrence for wait game-dll-build:2026-11-26T00:55:56.696Z (issue-ac23ab33d184). The wait evidence records build_id 2026-11-26T00:49:32.788771Z (last published artifact), generation=null, and no run_id. The in-flight attempt was game-dll-20261126T005556909723Z-fa8154, source_generation 2; dev-loop.game-build.started was 00:56:15.676Z; the terminal result was generation 662, status ok, total 194.141s, run 20261126_005302. That attempt identity is recoverable only from the truncated reason tail. This confirms the known gap in the acceptance phrase "build/run IDs" on the wait record. No source fix claimed and no duplicate issue filed.

Evidence: {"in_flight_build_attempt_id": "game-dll-20261126T005556909723Z-fa8154", "in_flight_generation": 662, "issue": "issue-ac23ab33d184", "recorded_build_id": "2026-11-26T00:49:32.788771Z", "recorded_generation": null, "run_id": "20261126_005302", "wait_id": "game-dll-build:2026-11-26T00:55:56.696Z"}

## Occurrence

### 2026-11-26T04:14:53.553Z — build-wait-reviewer

Fresh identity-mismatch + reason-tail occurrence for wait game-dll-build:2026-11-26T02:29:43.891Z (issue-5137be4d2f57). Evidence records build_id=2026-11-26T01:00:35.913746Z (last published artifact), generation=null and no run_id; the in-flight attempt was game-dll-20261126T022944049233Z-822054, source_generation 8, real generation 665, run 20261126_022514, status ok total 94.875s (compile 52.391s, link 19.922s, 7 compiled / 638 skipped). reason is a mid-JSON tail starting `59297.0,"build_attempt_id":...` from dev-loop.py:3112 [-240:]. One reviewer wake was queued (wake-bdaae328c6d8) but its first launch 02:30:50Z produced no review; wake.retry 04:01:56Z and the review only arrived at 04:02:45Z (~92 min later, issue-84c584957e65). This confirms the known identity (build/run IDs) and reason defects on a fresh 2026-11-26 pair. No source fix claimed and no duplicate issue filed.

Evidence: {"budget_seconds": 60.0, "host_cpu_mean_percent": 97.36, "host_cpu_peak_percent": 100.0, "in_flight_build_attempt_id": "game-dll-20261126T022944049233Z-822054", "in_flight_build_outcome": "success", "in_flight_compile_seconds": 52.391, "in_flight_compiled": 7, "in_flight_generation": 665, "in_flight_link_seconds": 19.922, "in_flight_skipped": 638, "in_flight_source_generation": 8, "in_flight_total_seconds": 94.875, "issue": "issue-5137be4d2f57", "journal": "logs/2026-11-26/20261126_022514/dev-loop/events.jsonl", "journal_lines": [54, 60, 61, 64, 65], "kind": "game-dll-build", "min_available_memory_bytes": 88260608, "reason_is_json_tail": true, "recorded_build_id": "2026-11-26T01:00:35.913746Z", "recorded_generation": null, "recorded_run_id": null, "reviewer_wake": "wake-bdaae328c6d8", "reviewer_wake_count_for_wait": 1, "run_id": "20261126_022514", "stage": "game_build", "timing_summary": ".dev/build-timings/game-dll-20261126T022944049233Z-822054.json", "wait_elapsed_seconds": 60.464, "wait_id": "game-dll-build:2026-11-26T02:29:43.891Z", "wait_record_utc": "2026-11-26T02:30:50.237Z", "wake_first_launched_utc": "2026-11-26T02:30:50Z", "wake_queue_to_working_review_seconds": 5520, "wake_queued_utc": "2026-11-26T02:30:46.325Z", "wake_relaunched_utc": "2026-11-26T04:02:45.520Z", "wake_retry_utc": "2026-11-26T04:01:56Z"}

## Occurrence

### 2026-11-26T15:55:46.070Z — build-wait-reviewer

Fresh identity-mismatch + reason-tail occurrence for wait game-dll-build:2026-11-26T15:43:01.573Z (issue-28feee119403). DIRECT from the recorded issue evidence and dev-loop journal logs/2026-11-26/20261126_145549/dev-loop/events.jsonl lines 146-147: the in-flight attempt was game-dll-20261126T154301815009Z-41b473, generation 691, source_generation 13, revision 8029c2c70b770842bceca677784928a48d5f050dcc693aa5b14467b88f275a8d, status ok, worker_total 54.766s. The issue record stored build_id=2026-11-26T06:10:41.962780Z (last published artifact) and generation=688 (stale), so the build/run identity is wrong; source_generation and request_ids are correct. The reason field is a mid-JSON tail `:55766.0,"build_attempt_id":"game-dll-20261126T154301815009Z-41b473",...` (issue-6e08d89b55e3). One reviewer wake (wake-3b1d8985a679, opencode) was queued once for this wait and launched ~99s later (created 15:44:03.071Z, launched 15:45:42.502Z) and did produce this review, so delivery worked here. No source fix claimed and no duplicate issue filed.

Evidence: {"actual_generation": 691, "build_attempt_id": "game-dll-20261126T154301815009Z-41b473", "issue": "issue-28feee119403", "journal": "logs/2026-11-26/20261126_145549/dev-loop/events.jsonl", "journal_lines": [146, 147], "reason_is_json_tail": true, "recorded_build_id": "2026-11-26T06:10:41.962780Z", "recorded_generation": 688, "reviewer_wake": "wake-3b1d8985a679", "reviewer_wake_count_for_wait": 1, "revision": "8029c2c70b770842bceca677784928a48d5f050dcc693aa5b14467b88f275a8d", "run_id": "20261126_145549", "source_generation": 13, "wait_elapsed_seconds": 60.025, "wait_id": "game-dll-build:2026-11-26T15:43:01.573Z", "wait_record_utc": "2026-11-26T15:44:06.249Z", "wake_created_utc": "2026-11-26T15:44:03.071Z", "wake_launched_utc": "2026-11-26T15:45:42.502Z"}

## Agent suggestion

### 2026-11-26T17:25:38.287Z — build-wait-reviewer

Recurrence 2026-11-26: three distinct over-budget game-dll-build/game_build issues in ~19 min - issue-404bd5b44b6e (wait 16:49:17.979Z, gen 696, 141.05s), issue-4fb7a24c54b0 (wait 16:56:24.029Z, gen 698, 151.16s), issue-45733b82bbbc (wait 17:07:53.638Z, gen 702, 103.31s); all builds succeeded, all exceeded the flat 60s budget. build_friction.py:143-169 keys uniqueness on wait_id, so each build opens a new same-titled issue, matching this issue's risk note 'noisy wait policy could create duplicate friction issues'. Recommend a decision: per-kind budget (game-DLL full compile is normally 100-150s) and/or a consolidation/cooldown that appends occurrences to one open issue instead of opening a new issue per wait_id. Cross-ref wait-policy owner issue-e044a931c54b.

## Agent suggestion

### 2026-11-26T17:40:10.604Z — build-wait-reviewer

Fresh identity-mismatch occurrence for wait game-dll-build:2026-11-26T17:13:10.456Z (issue-24863e1f595e). The wait record stores build_id=2026-11-26T16:59:26.767416Z and generation=702 (last published artifact) and no run_id, while the in-flight attempt was game-dll-20261126T171310909318Z-faf8c7, generation 703, source_generation 10, revision 5eb31cd568eb9799724c6551dff363d6545634d3110f55b88affc7cb091a1374, run 20261126_170323, status ok, total 137.203s. The actual attempt id is recoverable only from adjacent dev-loop.game-build.started lines and the reason revision; the reason field was clean here (not a JSON tail). One reviewer wake (wake-04a89c03a141) was queued exactly once. Direct from .dev/build-timings/game-dll-20261126T171310909318Z-faf8c7.json and logs/2026-11-26/20261126_170323/dev-loop/events.jsonl. No source fix claimed; no duplicate issue filed.

## Agent suggestion

### 2026-11-26T19:09:16.313Z — issue-review-sweeper

Evidence-retention gap found by an independent review of issue-ca900e60ed4b (2026-11-26). The 2026-11-21 review cited .dev/build-timings/game-dll-20261121T155224262249Z-d0908c.json/.jsonl and .dev/agent-wake/logs/2026-11-21T15-53-24.986Z-build-wait-reviewer.log; by 2026-11-26 all three were gone (oldest retained game-dll timing is 2026-11-24T230934953855Z-9ace92). A scheduled independent reviewer therefore cannot re-verify the prior 122.986s / failed_phase=changed_source_preflight / stage-timeline claims, so the issue is not cold-handoff-verifiable. This is missing evidence created by the wait pipeline itself, not a defect in the build.

Measurable target: 100% of wait issues created by this hook stay independently re-verifiable for >=14 days. Baseline today: 0/1 checked (issue-ca900e60ed4b) re-verifiable from retained artifacts.

Smallest experiment (proposal only; nothing changed in this review): at record_wait_incident() time, copy an immutable compact snapshot from the active attempt's .dev/build-timings/<attempt_id>.json into the issue evidence — total_seconds, success/failed_phase, and the phase-boundary list (phase, start_utc, end_utc) — instead of pointing at the rotating timing files. Then over the next 10 auto-created wait issues measure how many expose a retrievable phase-boundary snapshot (target 10/10) versus the current decay baseline. If the 10/10 target holds and review re-verification time drops, keep it; otherwise revert to pointer-only.

## Occurrence

### 2026-11-26T23:37:57.674Z — build-wait-reviewer

Fresh wake-launch-timeout defect occurrence for wait game-dll-build:2026-11-26T22:56:31.735Z (issue-e87167e20d7d). Unlike the 2026-11-22 success path already reviewed here, the inline `agent-wake.py run --once` drain in build_friction.wake_reviewer (devscripts/build_friction.py:244-246; hard timeout=30; no try/except) exceeded 30 s and raised TimeoutExpired (host was at 100% CPU while the compile was still running). Because dev-loop.check_wait_budget (devscripts/dev-loop.py:3905-3930) wraps record_wait_incident AND wake_reviewer in a single try, the raise skipped dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake and emitted dev-loop.wait.issue-failed with the misleading reason "could not record over-budget wait" (false: the issue was created at 22:57:35.047Z and one wake-c7a184e091be was queued at 22:57:42.545Z). reported_wait_ids already held the wait_id, so there was no retry, and this wait's over_budget/reviewer-wake evidence is permanently lost. Suggested smallest fix: make the inline run --once launch best-effort (catch TimeoutExpired and return queued/launch_error) and/or emit over_budget/reviewer-wake independently of inline-launch success with an explicit launch-status field. Evidence: logs/2026-11-26/20261126_225234/dev-loop/events.jsonl line 129 (dev-loop.wait.issue-failed, wall 22:58:12.872Z); .dev/agent-wake/events.jsonl wake.queued wake-c7a184e091be 22:57:42.545Z; .dev/agent-coord/events.jsonl seq 20598. No source edit claimed. Reported by build-wait-reviewer (wake-c7a184e091be); review on issue-e87167e20d7d.


Evidence: {"attempt": "game-dll-20261126T225631910089Z-cf9679", "failure_utc": "2026-11-26T22:58:12.872Z", "issue": "issue-e87167e20d7d", "journal": "logs/2026-11-26/20261126_225234/dev-loop/events.jsonl", "journal_line": 129, "reporter": "build-wait-reviewer", "run": "20261126_225234", "wait_id": "game-dll-build:2026-11-26T22:56:31.735Z", "wake": "wake-c7a184e091be"}

## Occurrence

### 2026-11-27T00:57:00.226Z — build-wait-reviewer

Fresh identity + reason-tail occurrence for wait game-dll-build:2026-11-27T00:23:02.531Z (issue-2aa79d9fe477, wake-f93906ee6177). Evidence records build_id 2026-11-26T22:52:33.481041Z (the last published artifact), generation=null and no run_id. The in-flight attempt was game-dll-20261127T002302708617Z-988794, source_generation 11, real generation 744, run 20261127_001718; it ran 1015.86s and FAILED at link (25.703s). reason is a mid-JSON tail starting `:47093.0,"build_attempt_id":...` from dev-loop.py:3112 (detail = line.rstrip()[-240:]) assigned at :3142. Exactly one reviewer wake was queued for this wait (events.jsonl lines 96-97). This reproduces the known build/run-ID gap and reason-truncation defect on a fresh 2026-11-27 pair; the real attempt identity is recoverable only from the truncated reason tail. No source fix claimed and no duplicate issue filed.

Evidence: {"budget_seconds": 60.0, "in_flight_build_attempt_id": "game-dll-20261127T002302708617Z-988794", "in_flight_failed_phase": "link", "in_flight_generation": 744, "in_flight_outcome": "failed", "in_flight_source_generation": 11, "in_flight_total_seconds": 1015.86, "issue": "issue-2aa79d9fe477", "journal": "logs/2026-11-27/20261127_001718/dev-loop/events.jsonl", "journal_lines": [96, 97], "kind": "game-dll-build", "open_duplicate_game_dll_wait_issues": 43, "reason_is_json_tail": true, "reason_owner": "devscripts/dev-loop.py:3112,3142", "reason_prefix": ":47093.0,\"build_attempt_id\"", "recorded_build_id": "2026-11-26T22:52:33.481041Z", "recorded_generation": null, "recorded_run_id": null, "reviewer_wake": "wake-f93906ee6177", "reviewer_wake_count_for_wait": 1, "run_id": "20261127_001718", "stage": "game_build", "wait_elapsed_seconds": 60.044, "wait_id": "game-dll-build:2026-11-27T00:23:02.531Z", "wait_record_utc": "2026-11-27T00:24:22.236Z"}

## Agent discussion and suggestions

### 2026-11-27T00:59:16.410Z — build-wait-reviewer

Message: `msg-020cd9f7e06a` (question)

Subject: Fresh identity+reason-tail occurrence for wait game-dll-build:2026-11-27T00:23:02.531Z

Fresh occurrence appended to issue-9a4503f6ed68 for wait game-dll-build:2026-11-27T00:23:02.531Z (issue-2aa79d9fe477, run 20261127_001718). It reproduces the known identity gap: recorded build_id is the last published artifact 2026-11-26T22:52:33.481041Z, generation=null, no run_id; the in-flight attempt was game-dll-20261127T002302708617Z-988794 (source_generation 11, generation 744). reason is still a mid-JSON tail from dev-loop.py:3112/3142. Exactly one wake was queued (wake-f93906ee6177). Question: is the in-flight build_attempt_id now available on the wait record, or is reading the reason tail still the only recovery path? No source fix claimed by me.

## Occurrence

### 2026-11-27T02:10:40.044Z — build-wait-reviewer

Occurrence 2026-11-27 for wait game-dll-build:2026-11-27T01:38:23.090Z (issue-e99701641a1c). The wait evidence records generation=749 and build_id=2026-11-27T00:44:04.497616Z, but the in-flight attempt was game-dll-20261127T013823160277Z-8b3773 at generation 750, source_generation 6, revision 7c0ee5b5..., request 2026-11-27T01:38:22.462Z-eb5ebd68:mode3:game (.dev/build-timings/game-dll-20261127T013823160277Z-8b3773.json started 01:38:23.160Z, succeeded in 186.672s). generation and build_id are last-published values captured before completion; source_generation is correct. No attempt id is recorded in the wait evidence. This is the same stale-identity behavior previously noted for wait 2026-11-24T21:23:08.791Z (issue-fdf3fb2f88e2). The build was healthy, so this is telemetry identity, not a stalled build. Reviewer build-wait-reviewer.

Evidence: {"build_outcome": "success", "build_total_seconds": 186.672, "in_flight_build_attempt_id": "game-dll-20261127T013823160277Z-8b3773", "in_flight_generation": 750, "issue": "issue-e99701641a1c", "related_prior": "issue-fdf3fb2f88e2", "request_ids": ["2026-11-27T01:38:22.462Z-eb5ebd68:mode3:game"], "revision": "7c0ee5b52e694af48d8f0f55cd742f8612d23cef1ecb468075ae41264b28ee87", "source_generation": 6, "wait_build_id_stale": "2026-11-27T00:44:04.497616Z", "wait_generation_stale": 749, "wait_id": "game-dll-build:2026-11-27T01:38:23.090Z"}

## Agent suggestion

### 2026-11-27T02:27:17.962Z — build-wait-reviewer

Identity occurrence (build-wait-reviewer) on wait game-dll-build:2026-11-27T01:44:56.218Z (issue-f6177f624ba9, run 20261127_014331). Wait evidence records generation=750 and build_id=2026-11-27T01:43:27.658897Z, i.e. the previously published game generation and the last EXE artifact, while the in-flight attempt was game-dll-20261127T014456458183Z-893a11 (request 2026-11-27T01:44:56.049Z-3f8f9b01, source_generation 0), which succeeded as generation 751 (ready 01:47:42.710Z). No run_id or build_attempt_id is stored; the only reliable join key is request_ids. Cause unchanged: check_wait_budget reads self.latest_build / self.game_build_generation (devscripts/dev-loop.py:3896-3897), and game_build_generation is assigned only on completion (dev-loop.py:3259). The reason field was clean this occurrence (reason-truncation defect issue-6e08d89b55e3 did not recur). Build was healthy, not a stall: total 166.0s, changed_source_preflight 57.703s (src/vehicles/vehicle-system.cpp 57.564s), link 80.297s (645 inputs / 502,153,396 B). Exactly one issue and one wake queued, so no per-wait duplicate. related_build/related_run were empty and were set by this reviewer to the true attempt/run. No source fix claimed.

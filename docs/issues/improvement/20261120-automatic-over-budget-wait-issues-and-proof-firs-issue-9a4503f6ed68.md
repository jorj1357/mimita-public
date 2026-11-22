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

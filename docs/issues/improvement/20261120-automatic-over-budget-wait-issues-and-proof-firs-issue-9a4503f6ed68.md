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

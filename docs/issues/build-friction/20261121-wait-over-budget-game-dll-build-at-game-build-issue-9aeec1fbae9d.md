# issue-9aeec1fbae9d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-21T14:21:30.903Z — build-wait-reviewer

evidence: {'budget_seconds': 60.0, 'build_id': '2026-11-20T20:20:51.788061Z', 'elapsed_seconds': 151.855, 'generation': 417, 'kind': 'game-dll-build', 'reason': '[STATE CODEGEN] ok domains=8 header=src/host/generated/resident-state-v1.h', 'request_ids': ['auto-45'], 'source_generation': 45, 'stage': 'game_build', 'started_utc': '2026-11-21T01:21:41.147Z', 'wait_id': 'game-dll-build:2026-11-21T01:21:41.147Z', 'review': {'reviewed_utc': '2026-11-21T14:20:33.275Z', 'reviewer': 'build-wait-reviewer', 'journal': 'logs/2026-11-20/20261120_232200/dev-loop/events.jsonl', 'wait_observed_utc': '2026-11-21T01:21:41.147Z', 'build_stage_sequence': ['01:22:40 codegen:resident_state_codegen.py started', '01:24:13 codegen:resident_state_codegen.py completed after 97.728s', '01:24:50 codegen:packet_registry_codegen.py started'], 'issue_created_utc': '2026-11-21T01:24:38.308Z', 'wake_id': 'wake-3f40d6151b79', 'wake_queued_utc': '2026-11-21T01:25:40.502Z', 'dev_loop_wait_issue_failed': {'wall_time': '2026-11-21T01:26:17.912Z', 'error': "Command '[pythonw.exe, devscripts/agent-wake.py, run, --once]' timed out after 30 seconds"}, 'first_wake_claim_utc': '2026-11-21T03:11:47.299Z', 'first_adapter': 'codex', 'fallback_retry_utc': '2026-11-21T03:48:06.389Z', 'final_adapter': 'opencode', 'reviewer_launched_utc': '2026-11-21T13:50:40.985Z', 'wait_to_reviewer_launch_seconds': 44939.838, 'wake_queue_to_launch_seconds': 44700.483, 'finding': 'The over-budget wait was detected and one issue plus one queued reviewer wake were created, but dev-loop logged dev-loop.wait.issue-failed instead of reviewer-wake/over_budget because build_friction.wake_reviewer synchronously ran agent-wake.py run --once with a 30s timeout. The queued wake was not launched by that run and was only claimed much later by the background wake path.', 'code_paths': ['devscripts/build_friction.py:244-246', 'devscripts/dev-loop.py:3450-3479'], 'recommended_fix': 'Do not block dev-loop wait handling on a synchronous 30s agent-wake.py run --once. Return after the wake add succeeds, or launch detached, and record dev-loop.wait.over_budget plus reviewer-wake before any adapter launch attempt. Handle adapter launch failure as a separate launch-status event.'}}

## Review (needs_fix)

### 2026-11-21T14:22:12.878Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review evidence (2026-11-21T14:20Z, build-wait-reviewer):

- The wait was real and the 151.855s elapsed is explained by active work: resident_state_codegen started at 01:22:40, completed at 01:24:13 (97.728s), and packet_registry_codegen started at 01:24:50. The 60s budget fired while the build was still progressing.
- Issue creation and wake enqueue succeeded: issue-9aeec1fbae9d was created at 01:24:38 and wake-3f40d6151b79 was queued at 01:25:40.
- Immediate reviewer launch did not succeed. logs/2026-11-20/20261120_232200/dev-loop/events.jsonl:100 records dev-loop.wait.issue-failed at 01:26:17 with "agent-wake.py run --once timed out after 30 seconds". There is no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake event for this wait.
- The wake was not claimed until 03:11:47 (codex), then fallback opencode at 03:48, and this reviewer finally launched at 13:50:40. That is 44,939.8s from wait start and 44,700.5s from wake queue, so "wakes one reviewer automatically" was not satisfied in practice.
- First incorrect step: devscripts/build_friction.py:244-246 synchronously runs agent-wake.py run --once with timeout=30 inside the dev-loop wait handler. devscripts/dev-loop.py:3450-3479 wraps issue creation and that launch in one try and converts the timeout into "could not record over-budget wait"; dev-loop.py:3458 marks the wait_id reported before the failed launch, so the wait is not retried.
- Recommended fix: enqueue and return, or launch detached; record over_budget and reviewer-wake before any adapter launch attempt; report adapter launch failure separately without discarding the successful issue/wake evidence. Cross-reference issue-9a4503f6ed68.
- No source or leased files were edited. This review does not close the issue; the automatic-wake acceptance remains unmet until the launch path is fixed and proven.

## Agent suggestion

### 2026-11-22T19:37:39.078Z — build-wait-reviewer

Additional corroborating occurrence of the same false failure (issue-52e6479ff974): wait exe-build:5:2026-11-22T17:37:18.164Z in run 20261122_144609_01 created its issue and queued wake-f0479e4f7c10 once, but dev-loop recorded dev-loop.wait.issue-failed at logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl:1602 because build_friction.wake_reviewer blocked on 'agent-wake.py run --once' with timeout=30. The EXE build itself completed normally (155.35s, artifact 2026-11-22T17:39:49.763492Z). Fix remains: queue/add the wake, return, log over_budget/reviewer-wake before adapter launch, and report adapter launch separately.

## Scheduled independent review

### 2026-11-25T06:12:40.041Z — build-wait-reviewer

A bounded independent review was queued as wake wake-03f2d03054b4 after 701 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs_fix)

### 2026-11-25T06:18:53.762Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Review evidence (2026-11-25T06:2xZ, issue-review-sweeper, read-only bounded independent review; no source/build/close/fix):

OBSERVED FACTS
1. The waited-on occurrence is real and over budget: wait_id game-dll-build:2026-11-21T01:21:41.147Z, stage game_build, elapsed 151.855s vs 60s budget, with resident_state_codegen 01:22:40-01:24:13 (97.728s) and packet_registry_codegen starting 01:24:50 (from this issue evidence block).
2. "Queued exactly once" holds for this occurrence. .dev/agent-wake/events.jsonl:474 has a single wake.queued for wake-3f40d6151b79 at 2026-11-21T01:25:40.502Z. Rows 498/499 (codex claim/launch 03:11:47.299/03:11:48.174Z), 510 (wake.retry to opencode 03:48:06.389Z), 542/543 (opencode claim/launch 13:50:40.964/13:50:40.988Z), and 621 (wake.failed 15:08:08.625Z, "adapter process exited") show 2 attempts / 2 launches for that one queued wake; .dev/agent-wake/requests/wake-3f40d6151b79.json has attempts=2, status=failed.
3. Immediate auto-launch at the wait did not happen: this issue evidence records dev-loop.wait.issue-failed at 01:26:17.912Z, "Command '[pythonw.exe, devscripts/agent-wake.py, run, --once]' timed out after 30 seconds", and there is no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake row for this wait_id. First claim was 6,606s after the wait started; working-adapter launch was 44,939.8s after it.
4. The recommended fix is NOT present in current source. devscripts/build_friction.py:244-246 still runs `agent-wake.py run --once` synchronously with timeout=30 inside the dev-loop wait handler. devscripts/dev-loop.py:3909 adds wait_id to reported_wait_ids before that call; 3912-3925 emit reviewer-wake/over_budget only after wake_reviewer returns; a subprocess.TimeoutExpired still reaches the except at 3926-3930 and is logged as dev-loop.wait.issue-failed, with the wait_id already marked reported so it is not retried. The same failure mode is still reachable.
5. The per-wait identity in the wait record is still stale. devscripts/dev-loop.py:3896 uses build_id=self.latest_build (last completed build) and 3897 uses generation=self.game_build_generation, which is assigned only on completion at 3259. Fresh occurrences on 2026-11-25: wait game-dll-build:2026-11-25T02:11:23.680Z recorded build_id 2026-11-25T01:21:28.987918Z / generation 632 while the in-flight attempt was game-dll-20261125T021123740140Z-5e8bdb / generation 633; wait game-dll-build:2026-11-25T01:05:53.086Z recorded generation 627 while the in-flight attempt game-dll-20261125T010553542744Z-42909c reached 629. This issue itself has related_build="" and related_run="", and its evidence build_id 2026-11-20T20:20:51.788061Z is a prior completed build, so the acceptance clause "carries ... build/run IDs" is still unmet for the waited-on attempt.
6. Artifact durability: the journal path in this issue evidence, logs/2026-11-20/20261120_232200/dev-loop/events.jsonl, does not exist now (logs/2026-11-20 is absent; on-disk layout is logs/11-20-2026, which is empty). .dev/agent-wake/logs/2026-11-21T01-25-40.224Z-build-wait-reviewer.log is also absent. The original adapter output and journal row for this occurrence cannot be re-verified; only the durable issue record, request JSON, and .dev/agent-wake/events.jsonl rows remain.

INFERENCE / UNCERTAINTY
- The automation detects the wait and creates one issue plus one queued wake; the defect is the launch/dispatch path, not detection. 2026-11-25 journals show wait.over_budget and wait.reviewer-wake rows are written when run --once returns inside 30s.
- Because the original journal and adapter log are gone, the 01:26:17 TimeoutExpired claim rests on this issue recorded evidence, not a re-read journal row.
- Exactly-once queue is upheld; exactly-one-launch is not. This occurrence had 2 launches; wake-47aeb34e7160 (wait game-dll-build:2026-11-25T02:11:23.680Z) also had attempts=2 and 594.16s queue-to-final-launch.

CONTRARY EVIDENCE
- The feature is not inert. 2026-11-25 over-budget waits (01:05:53, 01:14:06, 02:11:23, 02:21:17, 02:38:02) each produced a wait issue plus reviewer-wake/over_budget rows, and this occurrence was eventually reviewed (review id 42 at 2026-11-21T14:22:12Z), so the review obligation was discharged late, not dropped.
- The reviewer that eventually ran confirmed active codegen progress during the wait, so this was a progressing build under a flat 60s budget; the budget policy, not a stalled build, drove the incident. A separate defect also mangles the wait reason (tail of a [GAME_FILE] row) per issue-6e08d89b55e3.

NEXT DISCRIMINATING CHECK
1. On the next game-DLL over-budget wait with a known in-flight attempt, compare wait evidence build_id/generation to the attempt id/generation; a mismatch confirms the 3896-3897 identity defect on fresh data and that acceptance still fails.
2. Reproduce the original branch by holding a queued wake so `agent-wake.py run --once` exceeds 30s, then confirm dev-loop logs dev-loop.wait.issue-failed and omits reviewer-wake/over_budget for that wait_id; that proves the synchronous-launch defect is unfixed.
3. For that occurrence, count wake.launched rows for one wait_id in .dev/agent-wake/events.jsonl; more than one proves the exactly-one-launch target fails.

VERDICT: needs_fix. The two defects behind the original failure (synchronous 30s launch inside the wait handler; stale build_id/generation in the wait record) are still present in current code; only the happy-path auto-launch now succeeds. This review does not close the issue, edit source, or claim a fix.

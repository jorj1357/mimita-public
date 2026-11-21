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

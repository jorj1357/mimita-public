// 2026-11-21 11:25 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Build-wait review for issue-4c12dde1d7ba

# Task

- Task ID: session-de07f4e49b6f
- Session ID: session-de07f4e49b6f
- Owner: build-wait-reviewer
- Focus: issue-4c12dde1d7ba (wait game-dll-build:2026-11-21T15:54:27.410Z)
- Summary: Review the over-budget game-DLL wait, identify its first divergence, and append reviewer evidence. No code change was made or claimed.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-21T16:25:09+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

# Pre-existing changes

- The shared checkout was already heavily modified before this session. That state is not attributed to this review. The untracked issue document and the existing `docs/issues/README.md` modification were created by the automatic dev-loop wait path at 15:55:27Z, not by this session.
- This session changed only the shared issue record for issue-4c12dde1d7ba, its append-only detail document, the issue discussion, the linked user context, and this changelog. It edited no source, compiler input, build script, or leased file.

# Requested behavior

- The wake prompt asked to review build/test wait issue-4c12dde1d7ba, inspect the issue and the dev-loop journal, complete any missing fields, and append the review and evidence. It explicitly forbade code edits without human approval and asked that a code fix be reported and claimed separately.

# Specification alignment

- `docs/specs/dev-loop/change-to-evidence-latency.md:122-127` mandates the current behavior: any game-DLL build over budget for 60 seconds records one build-friction issue for that exact wait ID and queues one reviewer wake; the wait ID is the deduplication key.
- `docs/specs/dev-loop/change-to-evidence-latency.md:105-114` asks agents to investigate a wait that will exceed 60 seconds, record the phase and evidence, and append rather than duplicate issues for the same cause.
- The review therefore treats the issue as compliant with the written policy. The friction is that the policy measures total stage elapsed instead of lack of progress, so it fires on long but healthy builds. Changing that is a specification/self-improvement decision for a human, not an implementation bug to patch in this session.

# Exact implementation changes

- None. This session is an independent review.
- Appended to issue-4c12dde1d7ba: occurrence identity, attempt timings, wake history, the policy first divergence, the missing-attempt-id evidence defect, the duplicate-daemon trace, and owner recommendations. Verdict: `reviewed-needs-policy-fix`.

# Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-4c12dde1d7ba.md` | Appended the review and evidence message by the coordination tool. | Append-only issue history; do not remove. |
| `.dev/agent-coord` issue record/discussion (not a tracked source file) | Stored review id 48, the evidence message, and the context link. | Coordination state is append-only history; do not delete. |
| `docs/changelog/2026-11-21/20261121_1625-wait-review-issue-4c12dde1d7ba.md` | Added this session changelog. | Remove only if the record is superseded. |

# Contract / ABI changes

- None.

# State / schema changes

- None. No build, run, configuration, or game state changed. The game-DLL attempt under review had already ended before this activation.

# Diagnostics

- Wait policy owner: `DevLoop.check_wait_budget` in `devscripts/dev-loop.py:3346-3437`. `WAIT_ISSUE_SECONDS` is a flat 60 at `devscripts/dev-loop.py:108-109`; the game-DLL branch at `devscripts/dev-loop.py:3391-3407` tests total elapsed in stage `game_build` with no progress or stall check.
- Wait evidence owner: `devscripts/build_friction.py:143-202`. `build_id` is set from `self.latest_build` at `devscripts/dev-loop.py:3403`, so it names the last published artifact rather than the in-progress attempt. The attempt ID survives only in the free-text reason.
- Reason truncation: `devscripts/dev-loop.py:2701` stores `line.rstrip()[-240:]`, so the persisted reason begins mid-JSON and drops the stage prefix. This review saw the reason start at `59068.684,...`.
- Wake path: `devscripts/build_friction.py:205-252`. Request `wake-8360523c37ab` recorded `attempts=2`; the first codex launch failed because the configured model `gpt-6-luna` was rejected for a ChatGPT account, and the scheduler retried with opencode about 12 minutes later.

# Validation

- Issue readiness: `ready=true`, `missing=[]`, score 8/8 before review; the issue already had reproduction, expected behavior, acceptance, paths, proof expectation, and evidence.
- Attempt evidence: `.dev/build-timings/game-dll-20261121T155427526457Z-6240d5.json` records start 15:54:27.526Z, completion 16:07:57.840Z, `total_seconds=810.257`, `success=false`, `failed_phase=link`, `worker_error=link failed in 47.206s`, with undefined references to `mimita::game::replayRecorderPtr()`, `npcSystemPtr()`, `mpContextPtr()`, `gameStatePtr()`, `replayPlayerPtr()`, and `queryTransportSwapState()`.
- Timeline evidence from the matching `.jsonl`: changed-source preflight completed 15:55:26.631Z after 21.471s and object-cache scan started at elapsed 59.105s. The wait fired at 60.421s while the attempt was progressing, so the wait was a false positive, not a stall.
- Wake evidence: `.dev/agent-wake/events.jsonl` lines 729/732/733/739/745/746 record one queued wake, a failed codex launch, the 16:07:34Z retry, and the opencode claim. `.dev/agent-wake/requests/wake-8360523c37ab.json` records `attempts=2`.
- Journal trace: the issue-open event in `.dev/agent-coord/events.jsonl` line 3742 records `pid=33892`. No retained dev-loop `events.jsonl` contains `dev-loop.wait.over_budget` or `dev-loop.wait.reviewer-wake` for this wait ID; the surviving daemon journal `logs/2026-11-21/20261121_152950_01/dev-loop/events.jsonl` (PID 13660) has none.
- Duplicate-daemon evidence: `docs/changelog/2026-11-21/20261121_161330-dll-build-and-daemon-inventory-investigation.md` already records same-checkout daemons 19168, 13660, and 33892.
- Scale: 41 open `Wait over budget` build-friction issues (20 `new`, 21 `in_review`) at review time.
- No build or test was requested or run. No compile, dev-loop, or module-worker invocation was made by this session.

# Regression review

- No regression was introduced; no source changed. This is a recurrence of the already-recorded over-budget wait noise, not a new regression record.

# Human acceptance

- A human must decide whether to amend the 60-second wait policy to a stage-aware/progress-aware budget, and approve any code change. The reviewer did not claim or perform that change.
- The related link failure of attempt `game-dll-20261121T155427526457Z-6240d5` remains owned by the DLL build investigation; this review only cross-referenced it to `issue-136d4553ff92`.

# Agent handoff context

- Owner of the wait policy: `build-runtime-subagents` under parent `issue-e044a931c54b`, task `wait-instrument`.
- Do not redo: reading the attempt timings/timeline, counting the open wait issues, tracing the wake, and checking the surviving journal for the wait event.
- Next step: obtain human approval for a stage-aware, progress-aware wait budget and a structured wait record containing active attempt ID, stage, `source_generation`, and `last_progress_utc`; then fix `devscripts/dev-loop.py` and `devscripts/build_friction.py`. Also make only the lock-owning daemon report waits and fold the per-timer wait issues into the parent.

## Captured build identity

```json
{
  "reviewed_attempt": {
    "build_attempt_id": "game-dll-20261121T155427526457Z-6240d5",
    "source_generation": 4,
    "revision": "fc35e6fc2eae1c97170f8af1fa4ce7c214e2195039a3f324f645f6f27eec08f5",
    "started_utc": "2026-11-21T15:54:27.526Z",
    "completed_utc": "2026-11-21T16:07:57.840Z",
    "total_seconds": 810.257,
    "success": false,
    "failed_phase": "link"
  },
  "dev_loop": {
    "build_authority": "dev-loop",
    "lifecycle": "game_build_failed",
    "latest_successful_build": "2026-11-21T02:26:50.306117Z",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "generation": 420,
    "dll": "build\\game\\generation-420\\game-g420.dll",
    "status": "ok"
  }
}
```

## Captured git state for this session's files

```text
 M docs/issues/README.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-4c12dde1d7ba.md
?? docs/changelog/2026-11-21/20261121_1625-wait-review-issue-4c12dde1d7ba.md
```

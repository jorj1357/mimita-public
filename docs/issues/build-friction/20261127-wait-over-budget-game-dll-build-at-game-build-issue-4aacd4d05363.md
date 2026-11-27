# issue-4aacd4d05363: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-not-hang; bookkeeping-and-wake-defects-owned-elsewhere)

### 2026-11-27T17:54:52.719Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (read-only; no source edit, no build, no runtime control).

OBSERVED — wait occurrence
- wait_id game-dll-build:2026-11-27T16:18:04.967Z, kind game-dll-build, stage game_build, elapsed 60.199s vs budget 60.0s.
- In-flight attempt game-dll-20261127T161805119364Z-b52361 (request build-request-94637118d744, source_generation 32) started 16:18:05.159Z and completed 16:19:52.236Z, exit_code 0, published generation 795, total 107.063s (.dev/build-timings/game-dll-20261127T161805119364Z-b52361.jsonl: build.started / build.completed).
- Its build note declared expected_seconds 180.0. The latest whole-game report is generation 798, compile 402.922s / total 432.753s (.dev/build-state current.json). Inference (high): the flat 60s WAIT_ISSUE_SECONDS (dev-loop.py:114-115) is below a normal whole-game build, so this fires at 60s on a healthy build; it is not a hang.

OBSERVED — bookkeeping defect on this occurrence
- logs/2026-11-27/20261127_155356/dev-loop/events.jsonl line 81 records dev-loop.wait.issue-failed at 16:19:36.384Z: "agent-wake.py run --once timed out after 30 seconds", reason "could not record over-budget wait".
- The issue WAS created and the wake WAS queued exactly once (wake-83e438c02e22, wake.queued 16:19:06.299Z), but no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake event exists for this wait, so the failure label is misleading.
- Root (high confidence): build_friction.wake_reviewer runs subprocess.run([... "run","--once"], timeout=30) with no TimeoutExpired handling; the drain exceeds 30s, is killed, and the retried/queued request waits for the next drain.
- evidence.reason is a mid-JSON tail "5921.888,\"build_attempt_id\":\"game-dll-20261127T161805119364Z-b52361\",..." (last 240 chars of a raw [GAME_FILE] row; dev-loop.py:3112 detail=line.rstrip()[-240:], :3142 assign, :3892 read). build_id=2026-11-27T08:12:40.178185Z and generation=794 are stale vs the in-flight attempt/gen 795. Both defects are already owned by issue-6e08d89b55e3 (occurrence appended there).

OBSERVED — wake delivery latency
- wake-83e438c02e22 queued 16:19:06.299Z; codex claimed/launched 16:48:57.331/402Z; codex log model_not_found "gpt-6-luna not supported with a ChatGPT account"; wake.retry -> opencode 16:55:21.731Z; opencode claimed/launched 17:06:14.184/445Z. Queue-to-final-launch ~2827.7s (47m08s), attempts=2. Same class already owned by issue-84c584957e65 (occurrence appended there; no duplicate).

ACCEPTANCE CHECK
- Met: wait_id, stage, elapsed, evidence present; exactly one wake.queued for this wait occurrence; one issue, no duplicate.
- Not met cleanly: reason is not human-readable; build/run identity is stale/incomplete; the dev-loop final event is issue-failed instead of over_budget + reviewer-wake even though both effects happened.

NEXT CHECKS (smallest useful; owner/human decision for shared authority)
1. Derive the wait budget from the build note expected_seconds (or recent whole-game duration) or raise the flat default above a normal DLL build; this wait class has 130+ identical open game-dll-build issues.
2. In build_friction.wake_reviewer, treat run --once TimeoutExpired as "queued/launch attempted" so the over_budget and reviewer-wake events still record. Track under issue-84c584957e65.
3. Reason/identity fix remains issue-6e08d89b55e3.

UNKNOWNS / CONTRARY
- The killed run --once may have launched an adapter child before the 30s timeout; no codex/opencode launch event exists before 16:48:57, so I infer no adapter turn started from that drain. I did not attempt a reproduction. No new root cause beyond the two owner issues.

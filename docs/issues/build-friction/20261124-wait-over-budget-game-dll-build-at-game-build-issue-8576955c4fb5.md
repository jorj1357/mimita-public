# issue-8576955c4fb5: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (true-positive-expected-build-duration; missing-dev-loop-wait-event-and-stale-identity)

### 2026-11-24T12:29:15.501Z — build-wait-reviewer

Reviewer: build-wait-reviewer

True positive: a real game-DLL build crossed the flat 60s budget while actively compiling; not a hung worker. The issue record and exactly-one reviewer wake exist, but the dev-loop half of the proof expectation is missing and the evidence identity is wrong.

Direct evidence (UTC):
- Wait: wait_id game-dll-build:2026-11-24T10:36:51.643Z, kind game-dll-build, stage game_build, elapsed 61.311s (budget 60), source_generation 53, request 2026-11-24T10:36:50.061Z-abe6d51b:mode3:game.
- Build attempt game-dll-20261124T103652183287Z-745ef3 (same request) SUCCEEDED. .dev/build-timings/game-dll-20261124T103652183287Z-745ef3.json: total_seconds 989.864, worker_total_seconds 969.791, compiled 208 / skipped 437, jobs 10, compile_max_ms 171287.738 (src/network/server-gamemode.cpp 171.288s), changed_source_preflight 40.549s, link 12.867s, started_utc 10:36:52.184Z, completed_utc 10:53:22.048Z. Reason for the 208-TU rebuild: header deps (ui-system.h, debug-visuals.h, gamepad-input.h). So the 61.311s sample was mid-compile, not a stall; the flat 60s budget is far below this legitimate rebuild duration.
- Issue created 2026-11-24T10:37:53.411Z; reviewer wake wake-7949763eebb4 queued exactly once (2026-11-24 10:37:55.012Z, .dev/agent-wake/events.jsonl).
- FIRST DIVERGENCE / missing proof: dev-loop journal logs/2026-11-24/20261124_084513/dev-loop/events.jsonl has exactly one event for this wait_id, line 274: dev-loop.wait.issue-failed at 10:38:25.296Z, error "agent-wake.py run --once ... timed out after 30 seconds". There is NO dev-loop.wait.over_budget and NO dev-loop.wait.reviewer-wake for this occurrence, even though the issue and wake were both created. So the proof expectation ("dev-loop wait event plus issue record plus reviewer wake record") is only 2/3 met: issue record + wake record present, dev-loop wait event absent and replaced by a misleading failure. Cause: build_friction.wake_reviewer synchronously runs `agent-wake.py run --once` (timeout=30) after `add`, and dev-loop.py check_wait_budget catches the TimeoutExpired as "could not record over-budget wait"; reported_wait_ids is already updated so the wait is never retried. Already tracked on issue-9a4503f6ed68 (appended this occurrence).
- Evidence identity gap: evidence.build_id 2026-11-24T07:48:35.684015Z is the launched EXE artifact, not the game-DLL attempt; generation 587 is stale (the attempt published generation 590); no build_attempt_id and no run_id. Owned by issue-136d4553ff92 / issue-9a4503f6ed68.
- reason is a truncated mid-JSON tail ("ncremental\",\"raw\":\"tree SSA incremental ... progress_write_ms..."), not a readable stage summary. Same class as issue-6e08d89b55e3 (appended this occurrence).
- Late dispatch: wake queued 10:37:55.012Z; codex claimed/launched 10:54:31.727/10:54:31.811Z (+996.8s = 16.6 min); wake.retry switched to opencode 11:16:29.381Z; opencode claimed/launched 11:32:24.818/11:32:24.878Z (+3269.9s = 54.5 min after queue). Exceeds the <=5 min target on issue-84c584957e65 (appended this occurrence).

Acceptance assessment: wait_id/stage/elapsed/issue/evidence/exactly-one-wake present; the required dev-loop wait event is absent for this occurrence and the build/run identity fields are wrong/stale. No source change made; no build or runtime proof claimed. Verdict: true-positive wait, proof incomplete due to known mechanism defects owned elsewhere.

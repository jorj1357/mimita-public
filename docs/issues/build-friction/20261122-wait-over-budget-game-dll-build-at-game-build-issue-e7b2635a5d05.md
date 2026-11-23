# issue-e7b2635a5d05: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-22T19:16:30.114Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent review by build-wait-reviewer (wake-1ffe8890fb10). Verdict: confirmed wait-policy false positive, not a stall and not a build failure. No duplicate issue created; no source edited; no build requested.

Exact occurrence
- wait_id game-dll-build:2026-11-22T18:23:48.797Z, elapsed 62.662s against budget 60.0s, run 20261122_174002.
- dev-loop.wait.over_budget at 2026-11-22T18:25:25.254Z (events.jsonl line 125) recorded stage "game_build" but the stored reason is the [GAME_STAGE] payload "manifest_validation / started / elapsed_ms 61092.586" for attempt game-dll-20261122T182350171415Z-5d884f (it had entered manifest_validation at 18:24:51.264Z).
- That attempt was healthy and forward-progressing: dev-loop.game-build.result at 18:34:41.284Z reports status ok, generation 511, total 651.033s. .dev/build-timings/game-dll-20261122T182350171415Z-5d884f.json reports total_seconds 656.083 with changed_source_preflight 284697.977 ms as the largest phase. The 60s trip therefore fired ~26s into a normal preflight, not a hang.

First divergence and owner
1. devscripts/dev-loop.py:3538 hardcodes stage "game_build" and never uses the parsed [GAME_STAGE] substage; the real stage appears only inside the raw reason. The parsed payload already exists at devscripts/dev-loop.py:2832-2868, but check_wait_budget reads only game_build_detail.
2. devscripts/dev-loop.py:109 applies one flat WAIT_ISSUE_SECONDS=60 to game-dll-build. This occurrence took 651s; other attempts in the same run took 191.9s, 221.2s, 354.8s and 186.6s. Because wait_id includes the per-attempt game_build_started_utc (dev-loop.py:3536) and build_friction.py:164-169 keys uniqueness on wait_id, every successful build over 60s mints another identical issue. This is one of ~46-50 open "Wait over budget: game-dll-build at game_build" issues in the brain and matches the stated risk in this issue.
3. Attribution is stale: the wait evidence says generation 510 (the previous success) while the in-flight attempt 5d884f published generation 511; build_id is the EXE artifact 2026-11-22T17:39:49.763492Z, and there is no build_attempt_id or run_id field. The in-flight attempt id exists only inside the reason string.

Wake check
- Exactly one wake.queued exists for this wait (wake-1ffe8890fb10 at 2026-11-22T18:24:59.280Z), so the issue's "wake once per wait occurrence" wording is satisfied at queue time.
- Delivery was not single: codex claimed/launched at 18:25:16.4; agent-wake then logged wake.retry "stale launched request recovered by scheduler" at 18:33:43.4 and opencode claimed/launched at 18:35:54.7 (request attempts=2). One reviewer activation was duplicated; that belongs to the wake scheduler, already tracked by issue-9a4503f6ed68 and issue-84c584957e65.

Disposition
- Root cause and fix direction are owned by issue-136d4553ff92 (this occurrence appended there): stage- and progress-aware budget (or measured p95 per build class), populate stage/reason from the parsed [GAME_STAGE] payload, record build_attempt_id and run_id, and coalesce repeats per attempt/signature instead of one issue per attempt.
- Acceptance fields were already complete (issue readiness ready, no missing fields). No clarification needed from the reporter. Do not close the issue as a build defect; treat it as a recorded wait-policy occurrence.

## Agent suggestion

### 2026-11-22T19:23:43.557Z — build-wait-reviewer

Count correction to review id 197: the brain currently holds 118 issues titled 'Wait over budget: game-dll-build at game_build' (96 in_review, 21 new, 1 assigned), not the ~46-50 estimate in the review. The verdict and the attempt evidence are unchanged. Appended as occurrence to owner issue-136d4553ff92.

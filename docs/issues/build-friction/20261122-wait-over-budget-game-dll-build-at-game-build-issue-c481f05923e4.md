# issue-c481f05923e4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-22T07:00:14.187Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed the over-budget game-dll-build wait occurrence and its evidence.

VERDICT: Confirmed genuine wait; detector and wake path behaved as designed; issue fields complete.

Evidence
- Build attempt game-dll-20261122T063411278546Z-d597e3 (generation 467) ran 2026-11-22T06:34:11.278Z -> 2026-11-22T06:35:13.027Z, total 61.738s, success=true (.dev/build-timings/game-dll-20261122T063411278546Z-d597e3.json). Budget is 60.0s, so the wait was genuine, not a false positive.
- Wait wait_id game-dll-build:2026-11-22T06:34:11.198Z was recorded once: issue created 06:35:11.249Z at elapsed 60.017s, exactly as the attempt crossed 60s.
- Reviewer wake queued exactly once: .dev/agent-wake/events.jsonl wake-8a1d247d0557 wake.queued at 06:35:12.861Z for issue-c481f05923e4. The later claimed/launched entries (codex, then opencode fallback) are the same request, not an extra wake.
- Phase split from the summary: changed_source_preflight 18.89s, link 11.41s, object_cache_scan 4.71s, compile 0.91s. The delay is real build work, not a stuck wait/poll.

Recurrence
- Adjacent attempts also exceeded 60s: g466/8dc1de 74.127s and g468/620bf4 74.608s, each recorded as its own issue (issue-ec91f5dc7366, issue-86b3b2ce1e73). They are distinct wait_ids, so they are not duplicates under the current one-issue-per-wait_id design, but this is consecutive build-friction and the issue risk note about a noisy wait policy is materializing. No duplicate issue was created for this same wait_id.

Fields
- reproduction, expected behavior, acceptance, paths, and proof expectation are present (readiness ready=true). No missing-field fill required.

Observations (no code change made; needs human approval if pursued)
- evidence.reason is not human-readable here. dev-loop _record_game_build_output stores the last 240 chars of the last worker output line, so a >240-char progress line becomes a JSON fragment (48653.078,build_attempt_id:...). Earlier occurrences kept the whole [GAME_STAGE] {...} line; same code path, only truncation differs. Suggested fix: persist a parsed stage/event label or clean reason and keep the raw tail separate.
- Top-level evidence.build_id is the EXE build id (2026-11-22T02:37:06.972018Z), not the game-DLL attempt; the attempt id appears only inside the truncated reason. Consider a dedicated game_build_attempt_id field.
- The current run dev-loop journal reported by --status (logs/2026-11-22/20261122_011031/dev-loop/events.jsonl) was absent on disk at review time, so the dev-loop.wait.over_budget runtime event could not be independently re-read. Proof rests on the build summary, issue record, and agent-wake journal above.

No source edited; no files claimed.

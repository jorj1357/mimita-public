# issue-1cbec48ac17a: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-healthy-long-build; budget-and-reason-diagnostics-followup)

### 2026-11-22T14:09:36.902Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent review by build-wait-reviewer.

Verdict: healthy long build, not a hang or stall. The 60s wait budget is below the normal cost of an incremental whole-game DLL build, and the over-budget issue reason field is malformed by a dev-loop detail-truncation bug. No source edit made; build authority untouched.

EVIDENCE (this occurrence, wait_id game-dll-build:2026-11-22T13:34:11.060Z)
- Attempt: game-dll-20261122T133411119264Z-567c5b (.dev/build-timings/game-dll-20261122T133411119264Z-567c5b.json).
- success=true, status=ok, total_seconds=80.443 (worker_total 66.602). Started 13:34:11.119Z, completed 13:35:31.664Z; ready.json published 13:35:31.945Z; DLL build/game/generation-493/game-g493.dll.
- Phase durations (ms): changed_source_preflight 41866.336; link 12666.526 (lld_linker 11667.443); object_cache_scan 3217.831; publish 1083.210; compile 1728.193 (identity provider).
- Single changed source src/effects/zombie-tower-effects.cpp compiled in 41.522s; 641 TUs skipped. The build was making normal progress and finished ~20s after the budget elapsed, not stalled.

ACCEPTANCE CHECK
- Issue carries wait_id, stage=game_build, reason, elapsed=60.295s, budget=60.0s, build_id, generation, source_generation, request_ids, and started_utc: PASS.
- Reviewer wake queued exactly once: PASS. .dev/agent-wake/events.jsonl has one wake.queued for wake-d23788d5888f (13:35:14.517Z); no second queued record for this wait_id.
- dev-loop.wait.over_budget runtime event: MISSING. The active run 20261122_011031 dev-loop/events.jsonl directory does not exist, so the wait/wake runtime events cannot be written. This is the known defect in issue-6e2d7b91e7d9 (and issue-0498cbef4ef3); it blocks the journal half of this issue's proof_expectation. Verified: logs/2026-11-22/20261122_011031 has no dev-loop/ subdir.
- Wake dispatch latency: wake.queued 13:35:14.517Z, codex claimed 13:35:19.558Z, wake.retry to opencode 13:39:09.321Z, opencode claimed/launched 13:42:33. This is the known late-dispatch defect issue-84c584957e65, not a wait-detector fault.

RECURRENCE (distinct wait_ids; each correctly got its own issue, no duplicate created for this one)
- 13:34:11.060Z -> issue-1cbec48ac17a, attempt 567c5b, success 80.443s.
- 13:41:27.835Z -> issue-b52a0621a29e, attempt 7896ef, failed compile 87.828s (failed_phase=compile, worker_error 'compile failed: 1 file(s) in 62.299s').
- 13:44:38.883Z -> issue-3ffb3f29b1f7, attempt ada12d, success 94.681s (link 10.551s, preflight 47.259s).
The pattern is systemic: every one-TU whole-game DLL rebuild here costs ~80-95s, so a fixed 60s WAIT_ISSUE_SECONDS (devscripts/dev-loop.py:109-110) fires on normal builds. Policy/owner is issue-9a4503f6ed68 (wake-proof-wait, in_review).

DIAGNOSTIC DEFECT (reported, not fixed; needs separate claim/approval)
- This issue's evidence.reason and issue-b52a0621a29e's reason are truncated JSON tails, e.g. '8859.562,"build_attempt_id":...' and '51561.369,"build_attempt_id":...'.
- Cause: _record_game_build_output sets self.game_build_detail = line.rstrip()[-240:] for every non-empty build output line (devscripts/dev-loop.py:2832-2867); when the last line is a long [GAME_RESULT] or progress JSON payload, the 240-char slice cuts into the JSON and drops its prefix. check_wait_budget then copies game_build_detail into wait['reason'] (devscripts/dev-loop.py:3539). The result is unreadable triage text for over-budget waits.
- Suggested fix (owner of dev-loop build detail): keep a human-readable stage/last-action string for reason, and store raw/last-output separately (or clamp only plain text lines, not JSON payload tails). This is a diagnostics-only fix; no change to build behavior.

CONCLUSION
The wait is genuine but expected for a one-TU DLL rebuild; there is no stall to chase in this occurrence. Follow-ups belong to existing issues: issue-9a4503f6ed68 (budget/policy), issue-6e2d7b91e7d9 (missing dev-loop journal events), issue-84c584957e65 (late wake dispatch), plus the reason-truncation diagnostic above.

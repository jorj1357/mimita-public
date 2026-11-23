# issue-8cbff41b5590: Wait over budget: exe-build-queue at build_pending

## Human report (verbatim, immutable)

> exe-build-queue wait at build_pending exceeded the wait budget


## Review (confirmed-expected-queue-contention; reason-truncated=issue-6e08d89b55e3; journal-event-missing; wake-queued-once; code-fix-needed=no)

### 2026-11-22T20:29:51.198Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent build-wait review (build-wait-reviewer, read-only; no source edit, no build, no second loop).

VERDICT: Confirmed legitimate expected queue contention; the wait record's diagnostic fields are partly degraded. No source fix is required for the wait itself.

1) WHAT HAPPENED (causal chain)
- A source change auto-queued an EXE build while the single whole-game DLL builder lane was occupied by game-DLL generation 513.
- Generation 513 build: `.dev/build-timings/game-dll-20261122T194850321333Z-5c98ea.json` started_utc 2026-11-22T19:48:50.322Z, completed_utc 19:49:51.570Z, total_seconds 61.235 (link 39.941s, object scan 1.854s, one compile 3.755s, 642 objects skipped).
- This issue's wait started ~19:48:50.279Z (recorded 19:49:50.279Z at elapsed 60.03s vs budget 60.0s), i.e. the EXE request waited for that ~61s game-DLL build. The EXE artifact then published as `.dev/builds/2026-11-22T19-53-22.116562Z` (source_generation 4).
- Conclusion: not a hang and not an idle false positive. The 60s budget roughly equals one normal whole-game DLL build, so an EXE request queued concurrently crosses it by milliseconds.

2) EVIDENCE GAPS / RECORDING DEFECTS (not the wait itself)
- `reason` is corrupt: stored as a truncated JSON tail `19994.767,"build_attempt_id":"game-dll-20261122T194850321333Z-5c98ea",...`. Root cause is `devscripts/dev-loop.py:2833` `detail = line.rstrip()[-240:]` (same pattern at :3714), and `check_wait_budget` copies `active_build_detail or last_message` verbatim (`dev-loop.py:3522`). Already tracked as tech-debt `issue-6e08d89b55e3`; do not duplicate.
- `started_utc` is empty and `request_ids` is empty for this occurrence (`dev-loop.py:3525,3529` read active-build fields that are None while merely pending). Acceptance asks for build/run IDs; only a stale `build_id` (previous EXE build 2026-11-22T17:39:49.763492Z, from `dev-loop.py:3526` `self.latest_build`) is present, not the blocking game-DLL attempt `game-dll-20261122T194850321333Z-5c98ea`.
- No dev-loop wait journal event exists: 19:49:50Z fell in a gap between runs (20261122_192457 ended ~19:35Z; 20261122_195603 started 19:56Z). `record_runtime_event` returns silently when `self.events_path` is unset (`dev-loop.py:2279-2281`), so `dev-loop.wait.over_budget` / `dev-loop.wait.reviewer-wake` were never written. `proof_expectation` is only partly met: issue record + wake request exist; dev-loop wait event does not.
- `wait_id` uses a `time.monotonic()` float (`exe-build-queue:108093.776`), unlike ISO-timestamped wait kinds, so it is not meaningful or stable across loop restarts.

3) WAKE
- Exactly one reviewer wake was queued for this wait: `wake-80c50cf54ae2`, created 2026-11-22T19:49:52.631Z, adapter opencode, status launched. It was claimed/launched only at 20:21:35.507Z (~31m43s later) — a separate wake-latency observation, not a duplicate wake.

4) RECOMMENDATION
- Treat the wait as expected contention: either accept >60s when serialized behind a full game-DLL build, or count the EXE queue against the game-DLL build window instead of a fixed 60s from EXE-queue start.
- Fix diagnostics under existing `issue-6e08d89b55e3` (reason truncation) and record `pending_build_started_utc`, request_ids, and the actual blocking build attempt id for exe-build-queue waits.
- Give automatic wait/reviewer-wake events a persistent control journal so `proof_expectation` is satisfiable outside a server/client run.
- No code was edited and no duplicate issue was filed.

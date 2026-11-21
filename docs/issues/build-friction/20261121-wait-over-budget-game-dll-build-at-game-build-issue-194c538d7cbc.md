# issue-194c538d7cbc: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-wait)

### 2026-11-21T19:56:32.580Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer in a bounded wake activation (wake-7bcf8adc7395). Read-only: no source edits, no builds, no leases.

Issue completeness: `issue readiness` ready=true, missing=[]. All required fields (wait_id, stage, reason, elapsed, build/run IDs, reproduction, expected behavior, acceptance, paths, risk, proof expectation, evidence) are present. No clarification needed.

Occurrence is VALID (not a policy false positive): the build was genuinely still running and progressed through the wait.
- Wait `game-dll-build:2026-11-21T19:22:36.929Z`, budget 60s, elapsed 60.669s, source_generation 3, request auto-3.
- Active attempt `game-dll-20261121T192237066244Z-ab2591` (timeline in `.dev/build-timings/game-dll-20261121T192237066244Z-ab2591.jsonl/.json`): build.started 19:22:37.066Z, build.completed 19:24:48.216Z, success=true, generation 427, revision `86949cd9…`, total 131.141s, worker 106.937s.
- Wait fired ~19:23:37.6Z, i.e. ~23.0s into `object_cache_scan` (started 19:23:14.625Z, ended 19:24:20.017Z). The build was advancing, not stalled.

First divergence / dominant measured cause = `object_cache_scan` (65.390s, ~50% of the 131.1s attempt), and most of that scan is unaccounted:
- object_cache_scan 65.390s; within it dependency_check_ms 8.732s + scan_parse_ms 1.407s + scan_stat_ms 0.269s = ~10.4s accounted. ~55s (84% of the scan) is not explained by depfile parse/stat or dependency checks, consistent with the per-skip progress/record overhead signature documented for generation 351 (docs/specs/dev-loop/build-time-observability.md, gen-351 trace).
- Other phases: changed_source_preflight 17.766s, link 12.156s, compile wall 10.563s (worker compile sum 28.313s across 13 TUs, parallel), publish 4.344s; pre-worker codegen+manifest+source_snapshot ~16.7s.
- Change report named 2 game sources (`src/actor/player-ai.cpp`, `src/network/multiplayer-packets.cpp`); 13 compiled, 629 skipped of 642. A 2-file edit still paid a ~65s full object-cache scan before compiling.

Wait-evidence quality defect persists (matches issue-4c12dde1d7ba finding #2):
- Evidence `build_id` = `2026-11-21T02:26:50.306117Z`, the last published artifact, not the active attempt; `generation` is null. The true attempt id and generation 427 survive only inside a truncated `reason` tail that starts mid-JSON (`dev-loop.py` keeps the last 240 chars of the stage line). Acceptance asks for exact build/run IDs, so the wait record still does not identify the in-progress attempt structurally.

Recurrence: this is one occurrence of a heavily repeated signature, not a new finding. Same "game-dll-build at game_build over budget" issues include issue-905f128c10a1 (gen 429, 19:45), issue-dc6792df3815, issue-4c12dde1d7ba, issue-63dccbd8f8b6, issue-2008e743c710, issue-84ec2bf27598, issue-c2cebb751fb4. Recorded as the current occurrence here; no duplicate issue created. Canonical parents: issue-e044a931c54b task `wait-instrument` (wait policy/evidence) and issue-136d4553ff92 (per-stage DLL build accounting, owner cpp-latency-forensics). Separate observation: the reviewer wake was created 19:23:38.567Z but launched 19:46:29.196Z (attempts=2, ~22.8 min) — wake-adapter latency, not this wait's build cause.

Recommendations (report only; no fix claimed, no code changed):
1. Keep the 60s budget for now (this wait is real), but report the currently-running/dominant stage and its own elapsed in the wait snapshot instead of the flat `game_build` total.
2. Put active attempt id, generation, stage, source_generation, and last_progress_utc into the structured wait evidence and stop truncating the stage line to its last 240 chars.
3. Investigate the ~55s of object_cache_scan not covered by dep/parse/stat: separate worker per-skip JSONL write, progress-file writes, atomic-replace retries, pipe backpressure, and daemon journal handling, so the next trace reconciles scan wall time.
4. Fold the repeated wait issues into parent issue-e044a931c54b task `wait-instrument`; attach the scan-cost evidence to issue-136d4553ff92.

Evidence paths: `.dev/build-timings/game-dll-20261121T192237066244Z-ab2591.jsonl` and `.json`; `.dev/agent-wake/requests/wake-7bcf8adc7395.json`; issue evidence record for wait_id `game-dll-build:2026-11-21T19:22:36.929Z`. No retained dev-loop `events.jsonl` in `logs/2026-11-21/20261121_144350/dev-loop/` contains the over_budget/reviewer-wake rows for this wait_id (same journal-retention gap as issue-4c12dde1d7ba finding #3).

Verdict: confirmed-build-wait. True over-budget wait; dominant real friction is the object-cache scan, and the wait record still omits the active attempt id. Evidence review only; no runtime/behavior proof claimed. No owner assigned here.

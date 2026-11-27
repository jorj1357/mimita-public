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

## Scheduled independent review

### 2026-11-27T07:55:31.576Z — rocket-league-controller

A bounded independent review was queued as wake wake-1195e8f00c81 after 837 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-build-wait)

### 2026-11-27T08:00:25.479Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review by issue-review-sweeper in a bounded wake activation (wake-1195e8f00c81). Read-only: no source edits, no builds, no leases, no state change beyond appending this review.

Readiness/proof: `issue readiness issue-194c538d7cbc` is still ready=true, missing=[]; the acceptance fields are present. No behavior proof is claimed for a wait issue; the applicable evidence is the wait record + dev-loop journal + reviewer wake.

Observed facts (new since the 2026-11-21 review):
- The same signature recurred on 2026-11-27 and is filed as a separate occurrence issue, not an occurrence here: issue-778fe2977118, wait_id `game-dll-build:2026-11-27T07:23:16.007Z`, run `20261127_072104`, elapsed 60.335s / budget 60.0s.
- The wait-evidence quality defect this issue's first review recorded is still present, now with the active attempt in hand:
  - evidence.build_id = `2026-11-27T04:28:12.568764Z` (the last published artifact, not the active attempt `game-dll-20261127T072316157011Z-06c929`).
  - evidence.generation = 783 (last published) while the active attempt finished as generation 784.
  - The active attempt id/revision survive only inside evidence.reason, a mid-JSON tail `6085.139,"build_attempt_id":"...","generation":784,...,"record_overhead_ms":7.447}`.
- Journal half of the proof expectation is met for the recurrence: `logs/2026-11-27/20261127_072104/dev-loop/events.jsonl` L52 `dev-loop.wait.over_budget` and L51 `dev-loop.wait.reviewer-wake` (wall_time 07:24:28.72Z); one `wake.queued` (wake-5377afd8a3ae, 07:24:17.449Z) and no duplicate issue.

Inference / uncertainty (mechanism, not runtime proof):
- The stale fields are consistent with the source path: dev-loop.py:3897-3898 build the game wait snapshot from `self.latest_build` and `self.game_build_generation`, while `self.game_build_generation` is assigned only on completion (dev-loop.py:3259), so during a build it still holds the previous published generation. The truncated tail comes from dev-loop.py:3112 (`detail = line.rstrip()[-240:]`). I read the code; I did not run it, so this is a code-level inference about cause, not verified runtime behavior.

Contrary evidence to this issue's original "dominant cause = object_cache_scan":
- 2026-11-21 attempt ab2591: object_cache_scan 65.390s of 131.141s total (prior review's timeline).
- 2026-11-27 attempt 06c929 (gen 784): total 68.594s, but compile 42.945s, link 8.505s, object_cache_scan 5.576s, publish 0.551s; 10 TUs compiled (compile sum 287.1s across 10 jobs), with peak host CPU 100% and minimum free memory 23.0 MB (postmortem).
- So the over-budget wait is real each time, but the dominant phase is occurrence-specific; today it was compile-under-memory-pressure, not the object-cache scan. Gen 785 (40.55s) and gen 786 (27.15s) did not trip the 60s budget, so this is not "every build waits".

No new false positive: the build that triggered both waits was advancing and succeeded.

Next discriminating check (do not close on this review):
1. Capture the active attempt id, active generation, current stage, and last_progress_utc into the structured wait evidence at wait time, sourced from the active attempt rather than `latest_build`/`game_build_generation`; keep the readable stage summary instead of the 240-char tail.
2. On the next over-budget wait, assert evidence.build_attempt_id equals the active attempt and evidence.generation equals that attempt's generation, and that evidence.reason is human-readable (not a `...,"progress_write_ms":...}` tail).
3. Log object_cache_scan vs compile wall for the next two occurrences; only then decide whether the flat 60s budget or the evidence fields should change.
Existing owners already cover the sub-findings: issue-e044a931c54b task `wait-instrument` (wait policy/evidence) and issue-136d4553ff92 (per-stage build accounting). The specific dispatch-latency recurrence is already recorded on issue-84c584957e65 (seq 21844, 2026-11-27T07:59:02Z) and the reason-tail recurrence on issue-6e08d89b55e3 by build-wait-reviewer; I did not duplicate them.

Verdict: confirmed-build-wait — real over-budget wait, recurring; the acceptance record exists but still does not structurally identify the in-progress attempt. Evidence review only; no runtime/behavior proof, no fix claimed.

# issue-905f128c10a1: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-wait)

### 2026-11-21T20:06:53.055Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer in a bounded wake activation (wake-87c5dedf1df2). Read-only: no source edits, no builds, no leases, no fix claimed.

Issue completeness: `issue readiness` ready=true, missing=[], score 8/8. wait_id, stage, elapsed, budget, reproduction, expected behavior, acceptance, paths, risk, proof expectation, and evidence are present. No clarification needed.

Occurrence is VALID (real wait, not a stalled build):
- Wait `game-dll-build:2026-11-21T19:45:26.326Z`, budget 60s, elapsed 60.63s, source_generation 1, request ids auto-1 / 2026-11-21T19:45:28.833Z-d8944de6.
- Active attempt `game-dll-20261121T194526468225Z-8036b9` (`.dev/build-timings/game-dll-20261121T194526468225Z-8036b9.json` + `.jsonl`): build.started 19:45:26.468Z, build.completed 19:47:47.322Z, success=true, generation 429, revision 53e012f1fae51d4cae3665debbcb9dcf21d733294f22f6506609928518569f84, total 140.845s, worker 115.88s.
- At the 60.63s mark (~19:46:26.9Z) the build was 24.5s into `object_cache_scan` (phase started 19:46:02.401Z, elapsed 35.93s). It was advancing, not hung.

First divergence / dominant measured cause = `object_cache_scan` prepass, not compile:
- Stage timeline: codegen 0.54s + 3.43s, manifest_snapshot 8.32s, manifest_validation 0.02s, source_snapshot 6.84s, worker validation/coherence/toolchain/identity ~0.22s, changed_source_preflight 13.07s (player-ai.cpp compile 13.02s), object_cache_scan 74.69s, link 11.70s, publish 0.71s.
- `object_cache_scan` = 74.693s (phase.started 19:46:02.401Z -> phase.completed 19:47:17.095Z). The first `scan.snapshot` is emitted at scan_seconds 61.29 (19:47:03.696Z) with scanned=1. Recorded parse_ms 13.078 + stat_ms 273.728 + dependency_check_ms 9.565 explain only ~0.30s, so ~61.0s of the prepass is untimed/unaccounted before the first translated unit is evaluated.
- Only `src/actor/player-ai.cpp` changed; 12 TUs compiled, 630 skipped of 642, 1513 unique dependencies. Compile wall was only ~28.3s (worker sum across 12 TUs) versus ~88s of snapshots+preflight+scan, so this is scan/dependency-index cost, not C++ compile cost.

Evidence-quality gap (same as issue-4c12dde1d7ba finding #2): the wait record's structured `build_id` is `2026-11-21T02:26:50.306117Z` (last published artifact, an EXE build) and `generation` is null; the true attempt id/generation 429 survive only in a truncated free-text `reason` tail. Acceptance asks for build/run IDs, so add active `build_attempt_id` + generation + source_generation + last-progress time as structured wait fields. `related_build` is being set to the true attempt in this review to close that gap for this occurrence.

Acceptance check: one reviewer wake was queued for exactly this wait_id (`.dev/agent-wake/requests/wake-87c5dedf1df2.json`, issue=issue-905f128c10a1, created 19:46:27.407Z). Dedupe by wait_id works; no duplicate issue created.

Wake-adapter observation (separate from the build cause): the wake was created 19:46:27.407Z but launched 19:57:08.787Z (attempts=2, ~10.7 min). Reviewer latency is adapter/scheduler, not this wait's build time.

Recurrence: one occurrence of a heavily repeated signature. Same "Wait over budget: game-dll-build at game_build" issues include issue-194c538d7cbc (gen 427, 19:22), issue-fef8cd0c4b71 (19:53), issue-dc6792df3815, issue-63dccbd8f8b6, issue-2008e743c710. Canonical parents: issue-e044a931c54b task wait-instrument (wait policy/evidence) and issue-136d4553ff92 (per-stage DLL build accounting). Recorded here as the gen-429 occurrence; no duplicate opened.

Recommendations (report only; no fix claimed, no code changed):
1. Attribute over-budget waits to the dominant substage and include it (with its own elapsed) in the structured wait snapshot instead of the flat `game_build` total.
2. Add active build_attempt_id, generation, source_generation, and last_progress_utc to the structured wait evidence; stop keeping only the last 240 chars of the stage line.
3. Add sub-timers to the `object_cache_scan` prepass (dependency-index load, depfile parse, token classify, stat prewarm) so the untimed ~61s is provable; the leading hypothesis for that prepass cost is per-token Path.resolve in the dependency-relative prepass (see issue-ad873eff97f3), confidence medium.
4. Keep the 60s budget for now (this wait is real at 140.8s) but add progress/stall detection and duplicate-wake suppression while parent issue-e044a931c54b / issue-136d4553ff92 remain open.

Evidence paths: `.dev/build-timings/game-dll-20261121T194526468225Z-8036b9.json` and `.jsonl`; `.dev/agent-wake/requests/wake-87c5dedf1df2.json`; issue evidence for wait_id `game-dll-build:2026-11-21T19:45:26.326Z`. No retained dev-loop `events.jsonl` exists under `logs/2026-11-21/20261121_144350/dev-loop/` for this wait_id (journal path reported by `--status` is absent), so wait durability currently rests on the issue and wake records.

Verdict: confirmed-build-wait. Genuine over-budget wait; dominant real friction is the ~61s untimed `object_cache_scan` prepass, and the wait record still omits the active attempt id structurally. Evidence review only; no runtime/behavior proof claimed, no owner assigned here.

## Context

### 2026-11-21T20:07:10.927Z — build-wait-reviewer

related_build: game-dll-20261121T194526468225Z-8036b9
evidence: {'budget_seconds': 60.0, 'build_id': '2026-11-21T02:26:50.306117Z', 'elapsed_seconds': 60.63, 'generation': None, 'kind': 'game-dll-build', 'reason': '35892.632,"build_attempt_id":"game-dll-20261121T194526468225Z-8036b9","source_generation":1,"generation":429,"revision":"53e012f1fae51d4cae3665debbcb9dcf21d733294f22f6506609928518569f84","progress_write_ms":2.177,"record_overhead_ms":2.825}', 'request_ids': ['auto-1', '2026-11-21T19:45:28.833Z-d8944de6'], 'source_generation': 1, 'stage': 'game_build', 'started_utc': '2026-11-21T19:45:26.326Z', 'wait_id': 'game-dll-build:2026-11-21T19:45:26.326Z', 'review_verdict': 'confirmed-build-wait', 'reviewed_active_build_attempt_id': 'game-dll-20261121T194526468225Z-8036b9', 'reviewed_active_generation': 429, 'reviewed_active_revision': '53e012f1fae51d4cae3665debbcb9dcf21d733294f22f6506609928518569f84', 'reviewed_build_total_seconds': 140.845, 'reviewed_build_worker_total_seconds': 115.88, 'reviewed_dominant_stage': 'object_cache_scan', 'reviewed_dominant_stage_seconds': 74.693, 'reviewed_scan_first_snapshot_seconds': 61.29, 'reviewed_scan_prepass_unaccounted_seconds': 60.99, 'reviewed_compiled': 12, 'reviewed_skipped': 630, 'reviewed_source_count': 642, 'reviewer_wake_id': 'wake-87c5dedf1df2', 'reviewer_wake_launch_latency_seconds': 641}

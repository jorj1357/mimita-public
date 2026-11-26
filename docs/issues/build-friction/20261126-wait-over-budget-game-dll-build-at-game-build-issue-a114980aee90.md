# issue-a114980aee90: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T06:22:46.275Z — build-wait-reviewer

reproduction: Observe dev-loop game-dll-build wait game-dll-build:2026-11-26T06:07:55.877Z at stage game_build; elapsed=60.623s; active attempt game-dll-20261126T060755948198Z-ddc45d (generation 687, source_generation 32).
related_build: game-dll-20261126T060755948198Z-ddc45d
related_run: 20261126_055157
proof_expectation: Dev-loop wait event plus the issue record and reviewer wake record; correlate the wait to the exact in-flight attempt id and target generation, and confirm the build was progressing rather than stalled.

## Review (reviewed-false-positive-wait-record-defects-persist)

### 2026-11-26T06:23:15.740Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer. Read-only review; no source edit, no file claimed, no build/test/runtime started by this agent.

VERDICT: false-positive wait (the build was actively compiling, not stalled) plus recurring wait-record integrity defects. The create-issue + reviewer-wake mechanism fired once for this wait occurrence, but the reviewer wake was launched twice (codex primary, then opencode after stale recovery).

OCCURRENCE (direct evidence)
- Wait: kind=game-dll-build, stage=game_build, wait_id=game-dll-build:2026-11-26T06:07:55.877Z, started 06:07:55.877Z, elapsed=60.623s, budget=60.0s, issue=issue-a114980aee90 created=true, request_ids=[2026-11-26T06:07:55.768Z-b0c0af84:mode3:game].
- Active attempt: game-dll-20261126T060755948198Z-ddc45d, generation 687, source_generation 32, revision f45ea8ab7356eabee0f12cc8ea20659297a253720fcd09d174fa06e67326e999, run 20261126_055157.
- Build result: success=true, total 88.921s, completed 06:09:24.862Z; phase wall compile 74438ms, object_scan 3203ms, link 3281ms, publish 782ms; 36 compiled / 609 skipped of 644, jobs=10. Slowest TU src/mapeditor/map-editor.cpp 41.415s; compile_sum_ms 678991.967.
- At detection (~06:08:56.5Z) translation units were completing, not stalled: physical-entity-replication 06:08:51.273Z, map-authored-geometry 06:08:53.895Z, constraint-debug 06:08:55.908Z, multiplayer-reconcile 06:08:56.713Z, constraint-system 06:08:57.091Z, main-systems 06:08:57.472Z, community-match-client 06:08:57.831Z.
- Cause of crossing: changed headers physical-entity.h (fanout 31), vehicle-system.h (15), vehicle-types.h (17) legitimately rebuilt 36 TUs; compile phase alone was 74.4s, so the flat 60s budget is below a normal header-fanout compile. This is a wait-policy false positive, same class as prior reviews.

RECORD DEFECTS REPRODUCED (same classes as issue-75e49f67b48c review)
1. Reason corruption (first divergence): recorded reason is a raw [GAME_FILE] JSON tail `59641.0,"build_attempt_id":"game-dll-...-ddc45d",...,"progress_write_ms":10.58,"record_overhead_ms":11.454`. Owner: devscripts/dev-loop.py:3112 (detail = line.rstrip()[-240:]) and :3142 (unconditional self.game_build_detail=detail) feeding check_wait_budget reason at :3892. Expected: readable stage/file summary, not a JSON tail.
2. Wrong build_id: recorded build_id=2026-11-26T06:00:42.933273Z (last EXE artifact), not the in-flight game-DLL attempt id. The correct attempt id appears only inside the corrupted reason text. Owner: dev-loop.py:3896 build_id=self.latest_build.
3. Stale generation: recorded generation=686 (previous published) while the in-flight attempt is generation 687; source_generation=32 is present. Owner: dev-loop.py:3897 generation=self.game_build_generation, which is only assigned on completion at :3259.
4. Wake dispatched twice: exactly one wake.queued (wake-fc2eaee53560, 06:08:58.473Z) satisfies "queued exactly once", but codex launched 06:09:01.753Z produced no review; wake.retry "stale launched request recovered by scheduler" 06:13:17.754Z then opencode claimed/launched 06:13:18.758/18.819Z. Queue-to-final-launch=260.3s (~4m20.3s), attempts=2. This is the known dispatch defect; occurrence attached to issue-84c584957e65, not duplicated here.
5. Duplicate cluster: the immediately preceding wait game-dll-build:2026-11-26T06:00:44.737Z (issue-70a739edc6ae, attempt game-dll-20261126T060044815851Z-9b7ffb, 419.093s) was created in the same run. Per-timestamp wait_id keeps spawning near-duplicate issues. owner: issue-7d3dcfd7433b.

RESOURCE OBSERVATION (direct; causality unknown)
- attempt ddc45d .resource.json: 67 samples @1s over 88.906s, mean_host_cpu_percent=96.442, peak=100.0, minimum_available_memory_bytes=34,197,504 (~32.6 MiB), final_sample_status=host_and_cached_processes, jobs=10. runtime_alignment=unavailable/mode_unverified; unavailable_fields=[matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics]. This records pressure; it does NOT prove host pressure caused the 60s crossing. gap_key build-resource-sampling:game-dll-20261126T060755948198Z-ddc45d appended to issue-2a5c5da53565 (with 9b7ffb and ee15db).

MISSING FIELDS
- Readiness already ready=true (missing=[], 8/8). Only related_build/related_run were empty; this review set related_build=game-dll-20261126T060755948198Z-ddc45d and related_run=20261126_055157.

RECOMMENDATION (report only; owner/human approval required)
- Wait policy/duplicate/identity owner issue-7d3dcfd7433b: stage-aware or progress-gated budget (flag only when the stage detail/timeline has not advanced) and dedup by (kind, stage, active attempt) instead of one issue per wait start timestamp.
- Fix dev-loop.py:3896-3898 to record the in-flight game-DLL attempt id + target generation (from the active build/progress, not latest_build/last-completed generation); stop leaking raw [GAME_FILE] lines into game_build_detail at :3112/:3142.
- Wake delivery -> issue-84c584957e65. Resource sampling gaps -> issue-2a5c5da53565.
- Build success is build evidence only; it is not behavior/runtime proof.

## Agent discussion and suggestions

### 2026-11-26T06:24:58.576Z — build-wait-reviewer

Message: `msg-27a28e998b61` (review)

Subject: Review wait issue-a114980aee90 evidence + small next check

Review request (bounded): wait game-dll-build:2026-11-26T06:07:55.877Z -> issue-a114980aee90 is a false positive. Attempt game-dll-20261126T060755948198Z-ddc45d (gen 687) succeeded in 88.921s, 36 compiled/609 skipped from header fanout physical-entity.h(31)/vehicle-system.h(15)/vehicle-types.h(17); TUs were completing at the 60.6s crossing. Record defects reproduced: build_id=last EXE artifact, generation=686 vs in-flight 687, reason=raw [GAME_FILE] JSON tail (dev-loop.py:3112/3142/3896-3898). The same run also created the near-duplicate wait issue-70a739edc6ae. Please review and agree the smallest next check for a stage/progress-gated budget + dedup by (kind, stage, active attempt) + in-flight attempt/generation identity in issue-7d3dcfd7433b. Resource gap_key build-resource-sampling:game-dll-20261126T060755948198Z-ddc45d (plus 9b7ffb, ee15db) was appended to issue-2a5c5da53565; matched pre-build baseline and host commit charge/limit are unavailable, so pressure causality is unproven. No source change by this reviewer; any shared dev-loop change needs human approval.

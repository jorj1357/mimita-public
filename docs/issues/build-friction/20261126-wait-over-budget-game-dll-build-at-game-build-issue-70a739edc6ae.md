# issue-70a739edc6ae: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T06:23:39.914Z — build-wait-reviewer

related_build: game-dll-20261126T060044815851Z-9b7ffb
related_run: 20261126_055157

## Review (reviewed-legit-over-budget-duplicate-cluster-and-record-defects)

### 2026-11-26T06:24:17.515Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed occurrence game-dll-build:2026-11-26T06:00:44.737Z (related_build=game-dll-20261126T060044815851Z-9b7ffb, related_run=20261126_055157, generation 686, source_generation 22, revision bc390e3c). Verdict: the wait exceeds budget and the target build really ran ~7 minutes, but this is another member of the already-reviewed duplicate-cluster false positive (issue-75e49f67b48c; parent issue-e044a931c54b), and record-integrity defects reproduce on this occurrence. Mechanism acceptance is met: exactly one issue and one reviewer wake for this wait_id.

EVIDENCE (direct observation)
- Wait evidence: budget_seconds=60.0, elapsed_seconds=61.112, stage=game_build, source_generation=22, build_id=2026-11-26T06:00:42.933273Z, generation=null, reason is the truncated [GAME_FILE] JSON tail of the src/combat/weapon-fire-damage.cpp translation_unit.completed record (elapsed_ms=58812.0).
- .dev/build-timings/game-dll-20261126T060044815851Z-9b7ffb.json: started 06:00:44.815Z, completed 06:07:43.913Z, total_seconds=419.093, success=true, compiled 157/644 (488 skipped), compile=393.203s, link=8.829s, publish=0.453s. Changed headers include src/combat/weapon-viewmodel.h, src/gamemode/gamemode.h, src/ragdoll/ragdoll-mode.h, src/render/post-fx.h; slowest compiles src/network/server-gamemode.cpp=72.714s and src/gamemode/map-config.cpp=57.338s. This was active header-fanout compilation, not a stall. NOTE: recorded elapsed_seconds=61.112 is the first-crossing sample; actual wall time was 419.093s, so the record understates the wait by ~358s.
- Journal logs/2026-11-26/20261126_055157/dev-loop/events.jsonl: exactly one dev-loop.wait.reviewer-wake (wall_time 06:02:07.526Z, wake queued, adapter codex) and one dev-loop.wait.over_budget (created=true) for this wait_id; no dev-loop.wait.issue-failed. Acceptance "wakes one reviewer exactly once for this wait occurrence" is satisfied.
- .dev/build-timings/game-dll-20261126T060044815851Z-9b7ffb.resource.json: 280 samples over 419.078s, mean_host_cpu_percent=97.601, peak=100.0, minimum_available_memory_bytes=22,413,312 (~21.4 MiB), aligned run 20261126_055157 server mode vehicle_freeplay. Inference only: host CPU/memory pressure plausibly lengthened compiles; no matched prebuild baseline, so not proven as cause. Routed to issue-2a5c5da53565.

FIELD A (duplicate cluster / policy false positive) - blocking duplicate work
One global WAIT_ISSUE_SECONDS=60 is applied to game_build even though header-fanout game-DLL builds legitimately run minutes. This exact title currently has 213 open issues (build-friction states: 24 new, 231 in_review, 6 assigned, 1 deferred at 2026-11-26T06:23Z). The same defect is documented on issue-75e49f67b48c and issue-450cfc80a0b9; parent is issue-e044a931c54b (owner build-runtime-subagents). Recommendation unchanged: stage-aware budget + progress-stall detection + dedup by (kind, stage, active build attempt) instead of one issue per wait start timestamp.

FIELD B (reason corruption, reproduces) - first divergence
devscripts/dev-loop.py:_record_game_build_output sets detail = line.rstrip()[-240:] and stores it as game_build_detail for every output line, including [GAME_FILE]/[GAME_SCAN]; the long translation_unit.completed JSON line is cut mid-value, so check_wait_budget (dev-loop.py around line 3892) records a raw JSON tail as reason. Expected: a readable stage/file summary. Same defect as Finding 2 on issue-75e49f67b48c.

FIELD C (build identity mis-recorded, new for this sample)
The wait record stores build_id=2026-11-26T06:00:42.933273Z (the preceding EXE build) and generation=null, so the game-DLL attempt id appears only inside the corrupted reason field. related_build was empty until this review. Expected: the wait record names the game-dll attempt id and its generation.

FIELD D (wake launch_result pollution, reproduces)
dev-loop.wait.reviewer-wake fields.wake.launch_result contains the full prompt text of an unrelated queued wake (mode3-latency-continuation wake-3e566741dc9a), not a bounded launch result. Same as Finding 3 on issue-75e49f67b48c (build_friction.py:wake_reviewer returns agent-wake.py run --once stdout).

FIELD E (blocking wake dispatch, reproduces)
The wait first crosses 60s at 2026-11-26T06:01:45.849Z but the over_budget event is stamped 06:02:07.527Z (~22s later), consistent with the synchronous agent-wake.py run --once inside check_wait_budget (Finding 4 on issue-75e49f67b48c). Expected: non-blocking dispatch.

MISSING FIELDS
Readiness was already ready=true (missing=[], score 8/8). This review set related_build=game-dll-20261126T060044815851Z-9b7ffb and related_run=20261126_055157. No occurrence was added because this wait_id did not recur; the recurrence is the cross-attempt duplicate cluster.

RECOMMENDATION (report only; no code edited, no file claimed)
1) issue-e044a931c54b owner: apply the policy fix and merge/close the per-timestamp wait cluster; stop spawning a new issue per attempt. 2) Fix FIELD B/C in _record_game_build_output / check_wait_budget. 3) Fix FIELD D/E in build_friction.py:wake_reviewer. 4) Resource observation routes to issue-2a5c5da53565.

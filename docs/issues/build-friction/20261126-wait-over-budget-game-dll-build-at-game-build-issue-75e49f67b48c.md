# issue-75e49f67b48c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T05:53:09.727Z — build-wait-reviewer

related_build: game-dll-20261126T040021929713Z-cc30b9
related_run: 20261126_035851

## Review (reviewed-wait-policy-false-positive-and-record-integrity-defects)

### 2026-11-26T05:53:42.128Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed occurrence game-dll-build:2026-11-26T04:00:21.825Z (related_build=game-dll-20261126T040021929713Z-cc30b9, related_run=20261126_035851, generation 683, source_generation 3, revision b62f6e3b0968). Verdict: the wait is a wait-policy false positive (same class as already-reviewed duplicates) AND the wait record carries concrete data-integrity defects. The automatic capture/wake mechanism itself fired exactly once.

EVIDENCE (direct observation)
- Issue evidence: budget_seconds=60.0, elapsed_seconds=60.182, stage=game_build, source_generation=3, request_ids=[2026-11-26T04:00:21.715Z-5669dc8f:mode3:game].
- .dev/build-timings/game-dll-20261126T040021929713Z-cc30b9.json: started 04:00:21.929Z, completed 04:02:46.165Z, total_seconds=144.234, success=true, compile=119.172s for 36 CUs (609 skipped), link=7.157s, publish=0.594s. changed paths: src/gamemode/arena-ball.cpp, src/physics/physical-entity.cpp, src/physics/physical-entity.h (build note dd7dafdcbed1, agent rocket-league-drive). Slowest compile src/network/server-gamemode.cpp=76.631s.
- Journal C:\mimita-v9\logs\2026-11-26\20261126_035851\dev-loop\events.jsonl: at the 60.182s mark the build was actively compiling, not stalled (src/network/multiplayer-physical-entities.cpp completed 04:01:18.948Z = 57.06s in; src/physics/constraints/constraint-debug.cpp 04:01:23.241Z = 61.2s in).
- Exactly-once wake: exactly one dev-loop.wait.reviewer-wake and one dev-loop.wait.over_budget (created=true) for this wait_id in the run journal; .dev/agent-wake/events.jsonl records wake.queued wake-936189e1fbd5 at 2026-11-26T04:01:27.386Z.

FINDING 1 (wait policy, duplicate cluster)
One global WAIT_ISSUE_SECONDS=60 (dev-loop.py:115) is applied to game_build, but a header-fanout game-DLL build legitimately runs ~2-4 minutes (this attempt 144.234s). This is a false positive, same defect already reviewed on issue-450cfc80a0b9 (2026-11-21); it is now ~100 open/in_review "Wait over budget: game-dll-build at game_build" issues. Parent issue-e044a931c54b (owner build-runtime-subagents); measuring issue-7d3dcfd7433b. Recommended (unchanged): stage-aware budgets + progress detection (flag only when the stage detail/timeline has not advanced for N seconds) + dedup by (kind, stage, active build attempt) instead of one issue per wait start timestamp.

FINDING 2 (deterministic reason corruption) - first divergence
The recorded reason is the last 240 chars of a raw [GAME_FILE] JSON line: \":56922.0,\"build_attempt_id\":\"game-dll-20261126T040021929713Z-cc30b9\",...,\"progress_write_ms\":9.099,\"record_overhead_ms\":9.887} (matches the src/network/multiplayer-physical-entities.cpp record, elapsed_ms=56922.0). devscripts/dev-loop.py:_record_game_build_output sets detail=line.rstrip()[-240:] (line 3112) and unconditionally stores self.game_build_detail=detail (line 3142) for every line including [GAME_FILE]/[GAME_SCAN]; check_wait_budget then uses game_build_detail or last_message as reason (line 3892). Expected: a readable stage/file summary, not a raw JSON tail. Acceptance "carries reason" is technically populated but semantically wrong.

FINDING 3 (wake launch_result pollution)
dev-loop.wait.reviewer-wake fields.wake.launch_result contains the full prompt of an unrelated queued wake (cpu-resource-followup, wake-e055260ade97), not a launch result. devscripts/build_friction.py:wake_reviewer returns launched.stdout.strip()[-2000:] (line 250) from "agent-wake.py run --once", whose stdout includes drained queue/prompt text. Expected: a bounded launch-result summary.

FINDING 4 (wake dispatch blocks dev-loop ~29.5s)
Wait detected ~04:01:22 (elapsed 60.182) but reviewer-wake + over-budget events were stamped only at 04:01:56.893/894Z; wake.queued was 04:01:27.386Z. build_friction.py:wake_reviewer synchronously runs "agent-wake.py run --once" with timeout=30 (lines 244-246) inside check_wait_budget, blocking the dev-loop main loop up to 30s (here ~29.5s). Same area as issue-84c584957e65 and issue-2a5c5da53565. Expected: non-blocking daemon dispatch.

RESOURCE OBSERVATION (direct; causality unknown)
.dev/build-timings/game-dll-...-cc30b9.resource.json: 104 samples @1s over 144.203s, mean_host_cpu_percent=97.221, peak=100.0 on a 12-logical-CPU host, minimum_available_memory_bytes=119,906,304 (~114.4 MiB of ~15.6 GiB). Aligned run 20261126_035851 server mode vehicle_freeplay (server pid 448, client pid 32772). Unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation (mode_unverified). Inference, not proven: host CPU/memory contention may have lengthened compiles (server-gamemode.cpp 76.631s). Routed to issue-2a5c5da53565; not treated as the wait cause without an aligned baseline.

MISSING FIELDS
Readiness was already ready=true (missing=[], 8/8). Only related_build/related_run were empty; this review set related_build=game-dll-20261126T040021929713Z-cc30b9 and related_run=20261126_035851.

RECOMMENDATION (report only; no code edit, no file claimed)
1) issue-e044a931c54b owner: apply Finding 1 policy fix and merge/close the duplicate wait cluster; stop spawning per-timestamp wait issues. 2) Fix Finding 2 in _record_game_build_output. 3) Fix Finding 3/4 in build_friction.py:wake_reviewer (bounded result, non-blocking dispatch). 4) Resource observation -> issue-2a5c5da53565.

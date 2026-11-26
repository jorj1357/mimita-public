# issue-8b46d995a7a0: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T06:16:53.204Z — build-wait-reviewer

related_build: game-dll-20261126T054715953058Z-ee15db
related_run: 20261126_052030

## Review (confirmed-recurrence-real-active-build-unreadable-reason-wake-fallback-delay)

### 2026-11-26T06:18:13.741Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-ead4126186ed; effective adapter opencode after codex exit)

Reviewed occurrence game-dll-build:2026-11-26T05:47:15.830Z (related_build=game-dll-20261126T054715953058Z-ee15db, related_run=20261126_052030, generation 685, source_generation 10, revision e6abb8fc). Verdict: the wait is a REAL wait on an actively compiling header-fanout rebuild, not a stall; it is a flat-60s-budget false positive, plus the same wait-record data-integrity defect and a new wake-delivery fallback delay. Capture/wake mechanism fired exactly once.

DIRECT OBSERVATIONS
- Issue evidence: elapsed_seconds=60.215 vs budget_seconds=60.0, stage=game_build, request_ids=[2026-11-26T05:47:15.643Z-6817a927:mode3:game].
- Journal logs/2026-11-26/20261126_052030/dev-loop/events.jsonl: build started 05:47:21.805; file completions 05:47:29 -> 05:48:33. At the 60.215s crossing (~05:48:16.045) src/main-systems.cpp completed at elapsed_ms=60046 (=60.046s); additional TUs completed AFTER the crossing: runtime-objects 61.3s, vehicle-system 61.4s, map-editor 66.8s, vehicle-audio 66.8s, server-players 70.3s, vehicle-networking 70.5s, vehicle-hud 72.6s, simulate-tick 73.75s, vehicle-commands 74.39s, vehicle-config 77.16s.
- Build result 05:48:40.280: status=ok, generation 685, compiled=22, skipped=623, compile_seconds=66.953, link_seconds=5.89, total_seconds=84.093, worker_total_seconds=77.687. generation.ready 05:48:42.440 duration=85.953s.
- Real work, not a no-op/snapshot stall: changed header src/vehicles/vehicle-types.h invalidated 19 dependent object files (cache_invalidations in the attempt summary). Build note 2d1fa7586bd6, agent rocket-league-settle ("dodge 360 ... vehicle.chassis event ... MIMITA_VEHICLE_FUZZ probe").
- Exactly-once at queue level: one dev-loop.wait.reviewer-wake (05:48:31.783) and one dev-loop.wait.over_budget created=true (05:48:31.784) for this wait_id; .dev/agent-wake/events.jsonl has exactly one wake.queued (wake-ead4126186ed, 05:48:18.415) for this issue. Acceptance "reviewer wake queued exactly once" is met.

FINDING 1 (wait policy, duplicate cluster - unchanged)
One flat WAIT_ISSUE_SECONDS=60 (dev-loop.py:114-115) is applied to game_build, but a single changed shared header (vehicle-types.h) recompiled 22 CUs and legitimately took 85.9s. This is the same class already analyzed on issue-75e49f67b48c, issue-be98253c272f, and ~200 same-title issues. Recommend (report only): stage-aware and progress-aware budget (flag only when the stage timeline has not advanced for N seconds) + dedup by (kind, stage, active build attempt) instead of one issue per wait start timestamp. Owners unchanged: e044a931c54b / 7d3dcfd7433b; consolidate this issue into that tracker.

FINDING 2 (deterministic reason corruption - confirmed again)
evidence.reason is the last 240 chars of the src/physics/runtime-objects-commands.cpp [GAME_FILE] line: ":56187.0,\"build_attempt_id\":\"game-dll-20261126T054715953058Z-ee15db\",...,\"progress_write_ms\":7.904,\"record_overhead_ms\":8.521}". Cause (unchanged): _record_game_build_output sets detail=line.rstrip()[-240:] (dev-loop.py:3112) and stores game_build_detail for every line (3142); check_wait_budget uses game_build_detail as reason (3892). Same defect as Finding 2 in issue-75e49f67b48c. The acceptance "carries reason" is met only literally, not semantically. Already raised to build-runtime-subagents (msg-3b8451e21806, thread-ce979d7b3940); not duplicated here.

FINDING 3 (wake delivery fell back after ~21 min)
The one queued wake did not complete on its primary adapter: wake.claimed codex 05:48:31.725 -> launched pid 34028 05:48:31.766; then wake.retry 06:02:07.453 ("previous adapter process exited; trying fallback", adapter=opencode); wake.claimed opencode 06:09:01.664 -> launched pid 32692 06:09:01.701 (this review). So exactly one wake was queued but the effective review started ~21 min later via fallback. Nearest existing owner surface: wake-adapter reliability (issue-36579a10979a class, different direction); reported here as evidence, no fix claimed.

FINDING 4 (synchronous wake dispatch skew - consistent with prior Finding 4)
Wait elapsed 60.215s implies budget crossing at 05:48:16.045, but the over_budget/reviewer-wake events are stamped 05:48:31.783/784, ~15.74s later. check_wait_budget is called synchronously on the main loop (dev-loop.py:6101) and build_friction.wake_reviewer does blocking subprocess.run("agent-wake.py run --once", timeout=30) (build_friction.py:244-246). Consistent with the previously reviewed ~29.5s block on issue-75e49f67b48c Finding 4; mechanism not re-proven here.

RESOURCE OBSERVATION (direct; causality unknown)
.dev/build-timings/game-dll-20261126T054715953058Z-ee15db.resource.json: 70 samples @1s over 85.906s, mean_host_cpu_percent=91.371, peak=100.0 on a 12-logical-CPU (8 physical) host, minimum_available_memory_bytes=16,654,336 (~15.9 MiB). Build still succeeded. unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics; runtime_alignment=unavailable (mode unverified). Routed to issue-2a5c5da53565 as gap_key build-resource-sampling:game-dll-20261126T054715953058Z-ee15db; NOT treated as the proven cause of the wait.

MISSING FIELDS
Readiness was ready=true (missing=[], 8/8). Only related_build/related_run were empty; this review set related_build=game-dll-20261126T054715953058Z-ee15db and related_run=20261126_052030.

RECOMMENDATION (report only; no code edit, no file claimed)
1) Owner of issue-e044a931c54b / issue-7d3dcfd7433b: apply the stage-aware/progress-aware budget and dedup, and consolidate this occurrence rather than per-timestamp issues. 2) Fix Finding 2 reason construction in _record_game_build_output. 3) Address Finding 3/4 wake dispatch (bounded result, non-blocking dispatch, adapter-exit evidence). 4) Resource observation -> issue-2a5c5da53565.

CONFIDENCE: high on observed timings, build success, exactly-once queue, reason corruption, and the codex->opencode fallback; medium on the delay mechanism in Finding 4; resource causality NOT established.

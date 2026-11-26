# issue-3385f308560f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T15:02:05.606Z — build-wait-reviewer

reproduction: Observe dev-loop game-dll-build wait game-dll-build:2026-11-26T06:17:59.474Z at stage game_build; elapsed=60.773s vs budget 60.0s; correlate to in-flight attempt game-dll-20261126T061759578653Z-67ec90 (generation 688, source_generation 2).
related_build: game-dll-20261126T061759578653Z-67ec90
related_run: 20261126_061412
proof_expectation: Dev-loop wait event plus the issue record and reviewer wake record; confirm one over_budget event and one reviewer wake, and that the waited-on attempt was progressing (not stalled) across the crossing.

## Review (reviewed-false-positive-progressing-build-record-defects)

### 2026-11-26T15:02:39.180Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer

Read-only review; no source edited, no file claimed, no build/test/runtime started by this agent.

VERDICT: false-positive wait (the waited-on game DLL build was actively progressing, not stalled). Same class as umbrella issue-7d3dcfd7433b / issue-e044a931c54b. Mechanism acceptance is met for this occurrence: exactly one over_budget event (created=true) and exactly one reviewer-wake event for this wait_id.

OCCURRENCE (direct evidence)
- Wait: kind=game-dll-build, stage=game_build, wait_id=game-dll-build:2026-11-26T06:17:59.474Z, started 06:17:59.474Z, elapsed=60.773s, budget=60.0s, source_generation=2, request_ids=[2026-11-26T06:17:59.223Z-261bb8d6:mode3:game].
- Dev-loop journal logs/2026-11-26/20261126_061338/dev-loop/events.jsonl: one dev-loop.wait.over_budget (created=true, wall_time 06:19:05.640Z) and one dev-loop.wait.reviewer-wake (wake-14d62006319a, adapter codex, queued=true, wall_time 06:19:05.640Z).
- Waited-on attempt (correlated from timeline): game-dll-20261126T061759578653Z-67ec90, generation 688, revision eee512f0058f4458ee4d80b6e1448740fea5b093c5d2ee4ffd764faea2cf33c2, changed paths src/combat/explosion-fx.cpp + src/combat/weapon-viewmodel.cpp. Result success=true, total_seconds=107.875, completed 06:19:47.463Z; 3 compiled / 642 skipped of 644 source_count, jobs=10.

PROGRESS AT THE CROSSING (not a stall)
- 60s crossing ~06:18:59.47Z. At that point the attempt was inside changed_source_preflight (06:18:28.240Z→06:19:14.806Z) with both heavy TUs still compiling: src/combat/weapon-viewmodel.cpp 46.407s (started 06:18:28.264Z) and src/combat/explosion-fx.cpp 41.993s (started 06:18:28.259Z).
- After preflight: object_cache_scan 06:19:14.8Z, compile module-identity-provider 3.263s, link.lld_linker 06:19:18.292Z→06:19:45.544Z (27.203s), publish, build completed 06:19:47.355Z. Work advanced continuously across the crossing.
- Cause: the flat 60s WAIT_ISSUE_SECONDS is below the natural cost of a 2-TU rebuild + full 644-object link on this host (changed_source_preflight 46.563s + compile 3.39s + link 27.266s). This is a wait-policy false positive, not a stuck worker. Note the 60.773s figure is the first-crossing sample; actual build wall time was 107.875s.

RECORD INTEGRITY ON THIS OCCURRENCE
1. build_id=2026-11-26T06:10:41.962780Z is the preceding EXE artifact build (exe-build completed ~06:10:42Z), NOT the in-flight game-DLL attempt game-dll-20261126T061759578653Z-67ec90. Reproduces self.latest_build identity defect (dev-loop.py:~3896).
2. generation=null while the in-flight attempt is generation 688. Reproduces stale/absent generation (dev-loop.py:~3897).
3. reason is READABLE this time: '[GAME BUILD] revision=eee512... sources=644 jobs=10 priority=high cache=build\\game\\objects\\b10e3a31b3ae8637 seeded=0'. The raw [GAME_FILE] JSON-tail corruption seen on issue-a114980aee90 / issue-70a739edc6ae did NOT reproduce here (positive data point).
4. dev-loop.wait.reviewer-wake fields.wake.launch_result again contains the full reviewer prompt text instead of a bounded launch result (same as issue-a114980aee90 field D).
5. Dispatch latency: crossing ~06:18:59.47Z, wake log 06:19:02.266Z, over_budget/reviewer-wake journal stamped 06:19:05.640Z (~6.2s after crossing; journal write follows the wake, consistent with synchronous dispatch inside check_wait_budget).

RESOURCE OBSERVATION (direct; causality unknown)
- .dev/build-timings/game-dll-20261126T061759578653Z-67ec90.resource.json: 75 samples @1s over 107.86s, mean_host_cpu_percent=98.63, peak_host_cpu_percent=100.0, minimum_available_memory_bytes=378,179,584 (~360 MiB), status=captured, final_sample_status=host_and_cached_processes, runtime_alignment run_id=20261126_061412 (server bloodtest pid 10544 gen 687; client pid 28860 ~199.8 fps). unavailable_fields include matched_prebuild_resource_baseline and host_commit_charge_and_limit, so pressure is recorded but NOT proven as the cause of the crossing. gap_key: build-resource-sampling:game-dll-20261126T061759578653Z-67ec90.

MISSING FIELDS
- Readiness was already ready=true (missing=[], 8/8). related_build/related_run were empty; this review set related_build=game-dll-20261126T061759578653Z-67ec90 and related_run=20261126_061412. No occurrence added to this issue (this wait_id did not recur); the recurrence is the cross-attempt duplicate cluster on the umbrella.

RECOMMENDATION (report only; owner/human approval required)
- Policy + build_id/generation record defects -> tracker issue-7d3dcfd7433b (umbrella issue-e044a931c54b): stage/progress-gated budget, dedup by (kind, stage, active attempt), and record the in-flight attempt id/target generation instead of latest_build.
- Resource sampling gap -> issue-2a5c5da53565.
- Wake launch_result pollution / blocking dispatch -> issue-84c584957e65.
- Build success is build evidence only; it is not behavior/runtime proof.

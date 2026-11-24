# issue-21e4b1fbc77e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-not-a-stall-telemetry-defects)

### 2026-11-24T21:26:24.471Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer for over-budget wait game-dll-build:2026-11-24T20:35:35.515Z (issue issue-21e4b1fbc77e); read-only for source, build, runtime, and issue state. Wake wake-477d42954674.

OBSERVED FACTS
1. Wait: kind game-dll-build, stage game_build, started_utc 2026-11-24T20:35:35.515Z, elapsed 60.406s vs budget 60.0s; issue created 2026-11-24T20:36:36.019Z. Exactly one issue carries this wait_id.
2. The waited-on build is real work, not a stall. Request 2026-11-24T20:35:35.235Z-ce3687b4:mode3:game maps to attempt game-dll-20261124T203535650414Z-718e6f: outcome success, generation 618, total_seconds 228.997, published 2026-11-24T20:39:24.432Z. Phase durations: changed_source_preflight 149.629s (src/vehicles/vehicle-system.cpp 149.469s), link 61.452s, compile 2.715s, publish 0.552s. The resource sampler still showed phase changed_source_preflight with the TU active at elapsed 112.9s, so at the 60s trigger the build was healthy and compiling.
3. Evidence attribution defect: the issue records build_id=2026-11-24T19:27:02.742428Z and generation=617, which are the last completed build, not the waited-on attempt (generation 618, attempt game-dll-20261124T203535650414Z-718e6f). related_build and related_run are both empty. The attempt is identifiable only through request_ids[0] plus started_utc. dev-loop.py:3158-3170 sets game_build_started_utc at build start but game_build_generation is only set from the result at dev-loop.py:3259, and latest_build is the last completed build, so mid-build wait evidence reports the prior generation. Reconfirms the stale-identity occurrence already on issue-9a4503f6ed68 (2026-11-24T12:55Z).
4. Journal half missing: no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake row for this wait_id or issue id exists in any logs/2026-11-24/*/dev-loop/events.jsonl. The only repo hit is .dev/agent-wake/requests/wake-477d42954674.json. No retained dev-loop run covers the 20:36:36Z trigger: run 20261124_202802's dev-loop journal ends 20:29:30Z, the next root run begins 20:55:41Z, and run 20261124_203142_solver-proof-3 has no dev-loop journal. This matches the events_path=None early-return variant at dev-loop.py:2333-2335 (record_runtime_event returns when events_path is None), already tracked on issue-6e2d7b91e7d9. Inference: the exact dropped write was not captured; the absence is directly observed.
5. Reviewer wake: wake-477d42954674 queued exactly once at 2026-11-24T20:36:37.848Z. codex claimed/launched 20:36:52.484/20:36:52.610Z (pid 34424); wake.retry "stale launched request recovered by scheduler" at 20:38:37.184Z; opencode claimed/launched 20:38:42.741/20:38:42.828Z (pid 34476); request attempts=2. Queue-to-final-launch 124.98s (~2m5s), within the proposed <=5 min target, so late dispatch is NOT reproduced; however two wake.launched events exist for one wait_id, so the exactly-one-launch target is still violated. Sources: .dev/agent-wake/events.jsonl and .dev/agent-wake/requests/wake-477d42954674.json.
6. Reason quality: the recorded reason is a readable "[GAME BUILD] revision=... sources=644 jobs=10 priority=high ..." line, not a raw JSON tail; the reason-tail defect on issue-6e08d89b55e3 is NOT reproduced here.
7. Resource pressure (direct observation, not a proven cause): host 12 logical CPUs; 176 samples at ~1s over 228.99s recorded mean host CPU 99.97%, peak 100%; host memory used max 99%; minimum available memory 4.64 MB; physical used 16.49/16.80 GB at elapsed 112.9s. A live client pid 35812 (run 20261124_195553) recorded 14.69 FPS, p95 222.6 ms, and 17 of 17 frames over the 4.17 ms budget at 2026-11-24T20:36:51Z during the wait. Sampler top list also showed a mimita.exe instance at 162.3% of one logical CPU (13.5% whole host) and non-build user processes cpptools 69.0%, pythonw 44.5%, opencode 41.3%, chatgpt 38.6% (per-logical-CPU peaks). Source: .dev/build-timings/game-dll-20261124T203535650414Z-718e6f.resource.jsonl. Unknown: whether contention causally stretched the 149.5s TU compile; matched_prebuild_resource_baseline is unavailable.
8. Acceptance check: the clause "carries wait_id, stage, reason, elapsed, build/run IDs, and evidence" is NOT met as written (stale build_id/generation, no related_build/related_run, no dev-loop journal). The clause "a reviewer wake is queued exactly once for this wait occurrence" IS met (one wake.queued, one request file).

CONCLUSION
Not a stalled build. The 60s budget is shorter than a normal changed-TU game-DLL build on this 12-CPU host under ~100% host CPU load. This review performed no edit, build, runtime launch, issue close, or proof claim. Issue remains in_review.

OWNER / NEXT STEP (not claimed by this review)
- issue-9a4503f6ed68 owner wake-proof-wait: include the in-flight attempt id/generation/target in wait evidence and populate related_build.
- issue-6e2d7b91e7d9: write wait events when the daemon has no active run (events_path None).
- issue-84c584957e65: preserve exactly one launch per wait_id.
- issue-ed3ac775283d owner root-resource-loop: decouple long game-DLL builds from live-pair CPU saturation so 240 FPS / 60 Hz goals hold during builds.
UNKNOWNS: whether deleted/rotated run dirs once held the wait event; whether host saturation causally lengthened the 149.5s TU compile.

## Context

### 2026-11-24T21:26:24.731Z — build-wait-reviewer

related_build: game-dll-20261124T203535650414Z-718e6f
evidence: {'build_attempt_id': 'game-dll-20261124T203535650414Z-718e6f', 'request_id': '2026-11-24T20:35:35.235Z-ce3687b4:mode3:game', 'generation': 618, 'build_outcome': 'success', 'total_seconds': 228.997, 'published_utc': '2026-11-24T20:39:24.432Z', 'slowest_compile': {'path': 'src/vehicles/vehicle-system.cpp', 'seconds': 149.469}, 'link_seconds': 61.452, 'wait_recorded_generation': 617, 'wait_recorded_build_id': '2026-11-24T19:27:02.742428Z', 'dev_loop_journal_events_found': False, 'wake': 'wake-477d42954674', 'wake_queued_utc': '2026-11-24T20:36:37.848Z', 'wake_final_launch_utc': '2026-11-24T20:38:42.828Z', 'queue_to_final_launch_seconds': 124.98, 'launches_for_wait_id': 2, 'resource_journal': '.dev/build-timings/game-dll-20261124T203535650414Z-718e6f.resource.jsonl', 'host_logical_cpus': 12, 'host_mean_cpu_percent': 99.97, 'host_peak_cpu_percent': 100.0, 'minimum_available_memory_bytes': 4861952}

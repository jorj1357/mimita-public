# issue-ee3e906c8273: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-24T22:45:53.391Z — build-wait-reviewer

related_run: 20261124_205956
evidence: {'budget_seconds': 60.0, 'build_id': '2026-11-24T19:27:02.742428Z', 'elapsed_seconds': 61.245, 'generation': 623, 'kind': 'game-dll-build', 'reason': '[GAME BUILD] revision=244e42841d167a4582d4f0ecb53f2cff017a6ef01b2c5ff5e9e51d13e5d67e5f sources=644 jobs=10 priority=high cache=build\\game\\objects\\b10e3a31b3ae8637 seeded=0', 'request_ids': ['2026-11-24T21:43:54.624Z-f8b0d514:mode3:game'], 'source_generation': 25, 'stage': 'game_build', 'started_utc': '2026-11-24T21:43:56.034Z', 'wait_id': 'game-dll-build:2026-11-24T21:43:56.034Z', 'build_attempt_id': 'game-dll-20261124T214357271677Z-7f59d1', 'run_id': '20261124_205956', 'build_outcome': 'failed', 'failed_phase': 'changed_source_preflight', 'failed_source': 'src/main-systems.cpp', 'build_total_seconds': 310.99, 'changed_source_preflight_seconds': 261.697, 'wake': 'wake-59dc989b28d0', 'wake_queued_utc': '2026-11-24T21:45:00.693Z', 'wake_final_launch_utc': '2026-11-24T21:59:10.601Z', 'wake_launches': 2, 'wake_queue_to_final_launch_seconds': 849.908, 'host_mean_cpu_percent': 99.984, 'minimum_available_memory_bytes': 40304640}

## Review (reproduced)

### 2026-11-24T22:45:54.651Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-59dc989b28d0)

Wait record: complete and reproducible.
- wait_id game-dll-build:2026-11-24T21:43:56.034Z, stage game_build, elapsed 61.245s vs 60.0s budget, issue build_id 2026-11-24T19:27:02.742428Z, generation 623, source_generation 25, request 2026-11-24T21:43:54.624Z-f8b0d514:mode3:game, revision 244e42841d167a4582d4f0ecb53f2cff017a6ef01b2c5ff5e9e51d13e5d67e5f.
- dev-loop.wait.over_budget created 2026-11-24T21:45:08.566Z in logs/2026-11-24/20261124_205956/dev-loop/events.jsonl (line 288); run_id 20261124_205956.
- The waited-on attempt was real work, not a stall: game-dll-20261124T214357271677Z-7f59d1 started 21:43:57.271Z, total 310.990s, failed at changed_source_preflight in src/main-systems.cpp (261.697s) on 'weaponsPtr/cameraPtr/playerPtr/npcSystemPtr/worldPtr is not a member of mimita::game' at lines 505-509 (.dev/build-timings/game-dll-20261124T214357271677Z-7f59d1.json).

Acceptance check:
- Issue carries wait_id, stage, reason, elapsed, build/request IDs, and evidence: PASS. The only missing IDs were the run_id 20261124_205956 and the active build_attempt_id; both are added to evidence below.
- A reviewer wake is queued exactly once for this wait occurrence: PASS. Exactly one wake.queued row exists for wake-59dc989b28d0 / issue-ee3e906c8273 at 21:45:00.693Z (.dev/agent-wake/events.jsonl line 2899).
- Caveat (dispatch defect already tracked in issue-84c584957e65; no duplicate issue filed): the wake was launched twice and late. codex claimed/launched 21:45:05.480Z/21:45:07.333Z (pid 26096); wake.retry to opencode 21:51:21.275Z ('stale launched request recovered by scheduler'); opencode claimed/launched 21:59:10.107Z/21:59:10.601Z (pid 18976). Queue-to-final-launch 849.908s (~14m10s), above the <=5 min target, with two wake.launched rows for one wait_id.
- Wake-record misattribution confirmed: this wait's dev-loop.wait.reviewer-wake event at 21:45:08.562Z is stamped wait_id game-dll-build:2026-11-24T21:43:56.034Z / issue-ee3e906c8273 but its launch_result embeds wake-dbeab2f5c771 (log 2026-11-24T21-01-34.352Z-build-wait-reviewer.log, pid 33676, issue-6692376723be). The next wait's reviewer-wake event at 21:59:11.373Z (wait game-dll-build:2026-11-24T21:58:06.724Z / issue-a2de003a1900) embeds wake-59dc989b28d0. Cause: build_friction.py:242-252 returns the tail of 'agent-wake.py run --once', which drains other pending wakes, so the recorded wake is not necessarily the one just queued.

Resource evidence (direct, build-phase sidecar):
- .dev/build-timings/game-dll-20261124T214357271677Z-7f59d1.resource.json: 124 samples @1s over 310.98s, 12 logical CPUs, mean host CPU 99.984% / peak 100.0%, minimum available memory 40,304,640 B (~38.4 MiB), no sampler errors. runtime_alignment mode_unverified (server_actual_mode bloodtest, runtime run 20261124_212828, client mode stale_or_unreported).
- Inference: severe host CPU/memory pressure may have lengthened the 261.7s preflight; it does not explain the compile error. Cross-linked to issue-2a5c5da53565 (resource pressure) and issue-136d4553ff92 (stage latency).

No source edits, no build requested. Journal/artifact paths above are pinned. Confidence: high on wait record, build outcome, wake queue/launch counts, and misattribution; medium on resource causality.

# issue-551c405dc80c: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Context

### 2026-11-27T01:09:24.300Z — build-wait-reviewer

related_build: 2026-11-27T00:44:04.497616Z
related_run: 20261127_001718
evidence: {'wait_id': 'exe-build:17:2026-11-27T00:42:18.238Z', 'kind': 'exe-build', 'stage': 'compiling', 'reason': 'compiling 0/45 [CXX] src\\host\\game-state-bridge-legacy.cpp', 'elapsed_seconds': 60.86, 'budget_seconds': 60.0, 'started_utc': '2026-11-27T00:42:18.238Z', 'request_ids': ['2026-11-27T00:42:17.246Z-d93af8bb:mode3:cold'], 'build_id': '2026-11-26T22:52:33.481041Z', 'generation': 17, 'source_generation': 17, 'review': {'reviewer': 'build-wait-reviewer', 'verdict': 'confirmed-expected-cold-rebuild', 'not_a_stall': True, 'run_id': '20261127_001718', 'published_build_id': '2026-11-27T00:44:04.497616Z', 'build_attempt_id': 'exe-build-20261126T194218-c028c7d8', 'request': '2026-11-27T00:42:17.246Z-d93af8bb:mode3:cold (targets cold+game)', 'total_duration_seconds': 107.625, 'worker_timing': {'build_script_seconds': 87.593, 'source_identity_publish_seconds': 13.344, 'post_build_processing_seconds': 15.36, 'artifact_validation_seconds': 0.203, 'artifact_publish_seconds': 1.344, 'setup_seconds': 0.782, 'unattributed_seconds': 1.89}, 'exe_sources': 46, 'resource_summary': {'mean_host_cpu_percent': 98.404, 'peak_host_cpu_percent': 100.0, 'minimum_available_memory_bytes': 2323144704, 'sample_count': 75, 'sample_interval_seconds': 1.0, 'process_discovery_wall_ms_total': 52736.808, 'sampler_thread_cpu_ms': 24765.625, 'summary_path': '.dev\\build-timings\\exe-build-20261126T194218-c028c7d8.resource.json'}, 'outcome': 'exe published 2026-11-27T00:44:12.060Z; live pids [29544] unchanged; no failure/retry/timeout', 'flat_budget_owner': 'devscripts/dev-loop.py WAIT_ISSUE_SECONDS (line ~112) and check_wait_budget (line ~3839)', 'recurrence': 'none for this wait_id; 00:44:12 and 00:51:42 game-dll-build waits are separate wait_ids/issues', 'umbrella_issue': 'issue-e044a931c54b (build-runtime-subagents)', 'unknown': 'whether intentional cold rebuilds should be exempt from or use a cold-specific 60s budget; fix not claimed, needs human approval'}}

## Review (confirmed-expected-cold-rebuild)

### 2026-11-27T01:10:45.434Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait exe-build:17:2026-11-27T00:42:18.238Z for run 20261127_001718. Issue readiness was already complete (missing=[], score 8/8); related build/run and merged evidence attached.

Finding: NOT a stall or wedge. This was an intentional cold EXE rebuild from Mode 3 batch request 2026-11-27T00:42:17.246Z-d93af8bb (targets cold+game). Evidence from run 20261127_001718 dev-loop/events.jsonl:
- dev-loop.build.completed (00:44:12.060Z): build 2026-11-27T00:44:04.497616Z, attempt exe-build-20261126T194218-c028c7d8, duration 107.625s.
- Worker breakdown: build_script 87.593s, source_identity_publish 13.344s, post_build 15.36s, artifact_validation 0.203s, artifact_publish 1.344s, setup 0.782s, unattributed 1.89s.
- exe-cutover exe_sources=46; wait reason was 'compiling 0/45 [CXX] src\\host\\game-state-bridge-legacy.cpp' (45 TUs recompiled from cold).
- Resource summary .dev/build-timings/exe-build-20261126T194218-c028c7d8.resource.json: mean_host_cpu_percent 98.404, peak 100.0, min_available_memory 2,323,144,704 bytes, 75 samples @1s.
- EXE published successfully at 00:44:12.060Z; live game pids [29544] stayed on the prior executable (published_live_process_unchanged). No failure, retry, or timeout on this exe build.

Conclusion (high confidence): the 60.86s compiling wait at 0/45 is a legitimate cold rebuild under ~98% host CPU. The flat WAIT_ISSUE_SECONDS=60 budget (devscripts/dev-loop.py:112; check_wait_budget devscripts/dev-loop.py:3839) treats any cold rebuild >60s as friction, so this issue is expected behavior of the current policy, not a regression.

Separate unverified observation: resource-sampler overhead is large inside this 107.6s build - process_discovery_wall_ms_total 52,736.808ms and sampler_thread_cpu_ms 24,765.625ms. This is sampler cost, not build stall; flagged only, not claimed as the cause.

Recurrence: none for this wait_id (unique). The 00:44:12.064Z and 00:51:42.586Z 'game-dll-build' over-budget waits are separate wait_ids and separate issues; not merged here.

Suggested next check (fix NOT claimed; needs human approval): decide whether cold (mode3:cold) EXE rebuilds should use a cold-specific wait budget or be excluded from the generic 60s friction budget so intentional full rebuilds stop generating one high-priority issue each. Umbrella: issue-e044a931c54b (owner build-runtime-subagents). If chosen, it is a small dev-loop.py policy change and should be claimed separately.

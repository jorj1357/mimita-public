# issue-55479477f3a7: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-26T01:08:43.620Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: reviewed — the waited-on build was healthy; no build stall. The over-budget event is the known flat-60s wait-policy/telemetry class, plus new stage-accounting and resource-sampling evidence.

Direct observations (journal logs/2026-11-26/20261126_004140/dev-loop/events.jsonl):
- wait_id game-dll-build:2026-11-26T00:44:42.847Z; started_utc 00:44:42.847Z; elapsed_seconds 61.2 vs budget 60.0; trigger request 2026-11-26T00:44:42.722Z-087b4e9e:mode3:game; issue created 00:45:48.380Z (L79-80).
- dev-loop.game-build.started 00:44:59.851Z (L70) => 17.004s of the wait elapsed before the worker stage started (manifest_snapshot started 00:44:45.631Z, source_snapshot/codegen followed). If the budget clock began at build.started, this key would not have fired.
- Attempt game-dll-20261126T004443042199Z-2455a3, source_generation 5, generation 661, revision 9461894...: result status=ok at 00:48:32.358Z (L121); total_seconds 228.859, compile 200.219s, link 5.297s, compiled 48 / skipped 597; generation ready 00:48:34.062Z (L122). Changed paths: src/physics/physical-entity.{h,cpp}, src/vehicles/vehicle-system.cpp, src/vehicles/vehicle-types.h (build note da67d8a10032, agent rocket-league-landing).
- Slowest TU src/network/server-gamemode.cpp compiled 102.053s, finishing 00:47:56Z — the wait fired while 10 parallel compiles were still active. So this is not a stalled build.

First divergence (wait record vs reality):
1) Stage accounting: 17.0s pre-worker (snapshot/validation/codegen) counts against the flat 60s budget. Class tracked by issue-136d4553ff92.
2) Identity: wait evidence build_id=2026-11-26T00:34:14.001080Z (previously published EXE build id) and generation=null, while the in-flight attempt was game-dll-20261126T004443042199Z-2455a3 -> generation 661. Active attempt id/generation are absent from dedicated fields. Class tracked by issue-9a4503f6ed68.
3) Reason is a raw JSON tail ('':53719.0,"build_attempt_id":...'') rather than a stage summary (it is the tail of the src/input/input-poll.cpp TU row, started_elapsed_ms=53719). Class tracked by issue-6e08d89b55e3.
4) Reviewer wake: wake-b5a8c3467e0e queued exactly once at 00:45:50.531Z (.dev/agent-wake/events.jsonl L3383; request .dev/agent-wake/requests/wake-b5a8c3467e0e.json). codex claimed/launched 00:45:53.474/53.526Z (L3387-3388) then exited; wake.retry to opencode 00:57:03.231Z (L3395); opencode claimed/launched 00:58:52.021/52.034Z (L3400-3401; pid 22680). Two launches for one wait_id, queue-to-final-launch 181.503s (~3m02s). Class tracked by issue-84c584957e65.

Resource evidence (brain_check gap_key build-resource-sampling:game-dll-20261126T004443042199Z-2455a3; .dev/build-timings/game-dll-20261126T004443042199Z-2455a3.resource.json): status=captured, 166 samples @1s over 230.532s, mean host CPU 98.119%, peak 100%, minimum available memory 55,664,640 B (~53.1 MiB), sampler setup 313ms. runtime_alignment=mode_unverified (server vehicle_freeplay; client mode stale_or_unreported, PID 30112). Unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation. Near-full CPU is consistent with a long healthy compile but does NOT prove pressure caused duration or the wait; missing samples are unknown, not zero.

Contrary/uncertainty: the reported elapsed_seconds 61.2 does not exactly match the wall gap to the over_budget journal row (00:45:53.584Z is 70.737s after started_utc); the record appears to be stamped at an earlier budget-poll tick (issue created 00:45:48.380Z). Exact poll-tick timing is not captured. No source edit, build, fix, or runtime proof is claimed by this review.

Owners/next step: no new defect owned here. The smallest useful checks are already scoped on the cross-linked issues: start the game-DLL wait clock at game-build.started (or exclude pre-worker stages) on issue-136d4553ff92; carry build_attempt_id and in-flight generation in wait evidence on issue-9a4503f6ed68; emit a readable stage summary instead of the 240-char tail on issue-6e08d89b55e3; require exactly one wake.launched per wait_id (or record fallback as a distinct attempt) on issue-84c584957e65; record a matched pre-build resource baseline on issue-2a5c5da53565. This issue can stay as the per-occurrence record.

## Context

### 2026-11-26T01:09:05.002Z — build-wait-reviewer

related_build: game-dll-20261126T004443042199Z-2455a3
related_run: 20261126_004140
evidence: {'wait_id': 'game-dll-build:2026-11-26T00:44:42.847Z', 'kind': 'game-dll-build', 'stage': 'game_build', 'reason': '":53719.0,"build_attempt_id":"game-dll-20261126T004443042199Z-2455a3","source_generation":5,"generation":661,"revision":"94618941355e24e3f8d440241d836febfc4134e77ce39f56b57c3a7491c2761b","progress_write_ms":6.389,"record_overhead_ms":6.975}', 'elapsed_seconds': 61.2, 'budget_seconds': 60.0, 'started_utc': '2026-11-26T00:44:42.847Z', 'build_id': '2026-11-26T00:34:14.001080Z', 'generation': None, 'source_generation': 5, 'request_ids': ['2026-11-26T00:44:42.722Z-087b4e9e:mode3:game'], 'actual_build_attempt_id': 'game-dll-20261126T004443042199Z-2455a3', 'actual_generation': 661, 'build_outcome': 'ok', 'total_seconds': 228.859, 'compile_seconds': 200.219, 'link_seconds': 5.297, 'compiled': 48, 'skipped': 597, 'run_id': '20261126_004140', 'preworker_gap_seconds': 17.004, 'wake': 'wake-b5a8c3467e0e', 'wake_queued_utc': '2026-11-26T00:45:50.531Z', 'wake_launch_count': 2, 'queue_to_final_launch_seconds': 181.503, 'gap_key': 'build-resource-sampling:game-dll-20261126T004443042199Z-2455a3', 'resource_summary': '.dev\\build-timings\\game-dll-20261126T004443042199Z-2455a3.resource.json', 'journal': 'logs/2026-11-26/20261126_004140/dev-loop/events.jsonl'}

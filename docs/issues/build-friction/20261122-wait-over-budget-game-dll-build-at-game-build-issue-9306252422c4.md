# issue-9306252422c4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T01:54:43.081Z — build-wait-reviewer

related_build: game-dll-20261122T012948089638Z-6abdff
related_run: 20261122_011031
evidence: {'budget_seconds': 60.0, 'build_id': '2026-11-21T02:26:50.306117Z', 'elapsed_seconds': 60.433, 'generation': 438, 'kind': 'game-dll-build', 'reason': '[GAME_STAGE] {"stage":"source_snapshot","state":"started","time_utc":"2026-11-22T01:30:19.674Z","elapsed_ms":31578.0,"build_attempt_id":"game-dll-20261122T012948089638Z-6abdff"}', 'request_ids': ['auto-2'], 'source_generation': 2, 'stage': 'game_build', 'started_utc': '2026-11-22T01:29:47.508Z', 'wait_id': 'game-dll-build:2026-11-22T01:29:47.508Z', 'build_attempt_id': 'game-dll-20261122T012948089638Z-6abdff', 'build_started_utc': '2026-11-22T01:29:48.089Z', 'build_completed_utc': '2026-11-22T01:38:38.191Z', 'build_success': True, 'build_total_seconds': 529.953, 'worker_total_seconds': 348.875, 'build_result': 'ok; generation 439 published 2026-11-22T01:38:38.530Z', 'compiled': 13, 'skipped': 629, 'changed': ['src/npc/npc.cpp'], 'phase_durations_ms': {'changed_source_preflight': 127484, 'coherence_validation': 3078, 'compile': 103485, 'link': 92687, 'link.gnu_linker': 91829, 'object_cache_scan': 19282, 'publish': 12157, 'toolchain_setup': 31, 'vendor_compile': 93}, 'slowest_compiles': [{'path': 'src/npc/npc.cpp', 'seconds': 126.955}, {'path': 'src/network/reliable-gameplay-events.cpp', 'seconds': 102.616}, {'path': 'src/npc/npc-behavior-module.cpp', 'seconds': 50.917}, {'path': 'src/physics/movement/actor-contact-policy-module.cpp', 'seconds': 48.394}, {'path': 'src/render/actor-rendering-module.cpp', 'seconds': 48.341}], 'wait_window': {'wait_started_utc': '2026-11-22T01:29:47.508Z', 'over_budget_recorded_utc': '2026-11-22T01:31:06.814Z', 'elapsed_seconds': 60.433, 'budget_seconds': 60.0, 'phase_during_wait': 'source_snapshot', 'source_snapshot_started_utc': '2026-11-22T01:30:19.674Z', 'source_snapshot_completed_utc': '2026-11-22T01:31:43.068Z', 'source_snapshot_duration_seconds': 82.25}, 'journal_path': 'logs/2026-11-22/20261122_011031/dev-loop/events.jsonl', 'timing_path': '.dev/build-timings/game-dll-20261122T012948089638Z-6abdff.json', 'review': {'reviewer': 'build-wait-reviewer', 'wake': 'wake-9b53285bb55a', 'verdict': 'confirmed_legitimate_wait', 'wake_queued_once': True, 'duplicate_issue_occurrence': False}}

## Review (pass)

### 2026-11-22T01:59:48.992Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Read-only review of wait issue-9306252422c4. Finding: the over-budget wait was real; detection, record, and reviewer wake are correct. No code defect in the wait path.

Timeline: at wait start 2026-11-22T01:29:47.508Z the root dev-loop was already building game-dll-20261122T012948089638Z-6abdff (attempt started 01:29:48.089Z, changed=src/npc/npc.cpp) from generation 438. The 60.433s budget tripped at 01:31:06.814Z while the build was in source_snapshot (started 01:30:19.674Z, completed 01:31:43.068Z, 82.25s). The build then completed successfully at 01:38:38.191Z as generation 439 (published 01:38:38.530Z).

Measured cause (from .dev/build-timings/game-dll-20261122T012948089638Z-6abdff.json): total 529.953s, worker 348.875s for 13 compiled + 629 skipped files. Largest phases: changed_source_preflight 127.484s, compile 103.485s wall (230.922s CPU-sum; npc.cpp 126.955s, reliable-gameplay-events.cpp 102.616s), link 92.687s (GNU linker 91.829s), object_cache_scan 19.282s, publish 12.157s. The 60s wait budget is ~1/9 of this build's wall time, so most non-no-op game-DLL builds trip it.

Acceptance check: wait_id, stage, reason, elapsed, budget, request IDs, and evidence are present; exactly one reviewer wake (wake-9b53285bb55a) was queued for wait_id game-dll-build:2026-11-22T01:29:47.508Z and no duplicate occurrence was created. Gap repaired in this review: evidence previously lacked run_id and the in-flight build attempt id (both only in the free-text reason) and related_run/related_build were empty. Added related_run=20261122_011031, related_build=game-dll-20261122T012948089638Z-6abdff, and structured build/run/timing evidence.

Duplicate-churn risk confirmed: four build-friction issues were filed within ~13 min for successive game-DLL waits (issue-00d61cbc12a2 at 01:25:23, this issue at 01:29:47, issue-4891874dd02b exe-build-queue, issue-4409099b6172 at 01:38:41). Each distinct wait_id intentionally creates a new issue (build_friction.py record_wait_incident), so this is by design rather than a bug; but with the 60s budget below normal build latency it yields one issue per build.

Recommendation (review-only; not claimed, no code edited): keep the wait-handling path as-is. Route the recurring friction to existing build-latency owners issue-136d4553ff92 (build-coordinator) and issue-ed3ac775283d (cpu-build-efficiency), or add a bounded cooldown/coalescing rule in build_friction.py so repeated game-DLL waits append an occurrence to the current open build-friction issue instead of one issue per build. This issue can be closed as reviewed when that policy is decided.

Evidence: logs/2026-11-22/20261122_011031/dev-loop/events.jsonl lines 91-121 and .dev/build-timings/game-dll-20261122T012948089638Z-6abdff.json. Confidence: high that the wait and build outcome are real and the issue record is accurate; the duplicate-policy item is a recommendation, not a defect.

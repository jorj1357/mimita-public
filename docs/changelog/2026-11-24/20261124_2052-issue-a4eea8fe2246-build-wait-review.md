# issue-a4eea8fe2246 Build-Wait Review

# Task

- Task ID: review build/test wait issue-a4eea8fe2246 (wake-ad0bab437950)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.828s, wait_id game-dll-build:2026-11-24T20:00:45.963Z
- Summary: Verified the wait against attempt `game-dll-20261124T200045986936Z-3f16b3`, its build-timings and resource summaries, the dev-loop/wake journals, and the wake queue. Appended one review (verdict `confirmed-observability`) to issue-a4eea8fe2246 and linked occurrences to issues 6e2d7b91e7d9 (missing dev-loop wait event), 84c584957e65 (late/duplicate dispatch), 6e08d89b55e3 (reason tail), a0fa88737830 (build_id/generation identity), and 2a5c5da53565 (resource sampling). No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-24T20:52:00Z

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted source and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records written through `agent-coord.py`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- Wait was real, not a stall. Attempt `game-dll-20261124T200045986936Z-3f16b3` started 20:00:45.987Z (Mode 3 request `2026-11-24T20:00:45.704Z-bb02a2dd:mode3:game`; note: vehicle-contact-solver changed `src/physics/physical-entity.cpp` + `config/vehicles/octane_rl.json`) and ended 20:03:00.380Z, total 134.394s. Compile was still running at the 60.828s wait.
- The build FAILED, not merely slow: `failed_phase: compile`, `worker_error: compile failed: 1 file(s) in 125.197s`, and `src/entities/player-render.cpp:634:18 error: 'layer' was not declared in this scope`. 34 compiled / 595 skipped / jobs 10. Slowest TUs: server-npcs.cpp 57.64s, server-packets.cpp 48.41s, actor-triangle-solver.cpp 44.54s, procedural-world.cpp 42.68s, map-authored-geometry.cpp 41.18s. Later Mode 3 attempts progressed and the daemon reports generation 616 published, so the compile error appears transient; no duplicate issue created for it.
- Auto-intake and queue contract held: exactly one `wake.queued` (wake-ad0bab437950, 20:01:47.676Z) for this wait.
- PROOF GAP (new occurrence of issue-6e2d7b91e7d9): the required `dev-loop.wait.over_budget` and `dev-loop.wait.reviewer-wake` events are absent. No `logs/**/dev-loop/events.jsonl` contains this wait_id/issue id or any runtime event in 20:00-20:03Z; the only retained hit is `.dev/agent-wake/requests/wake-ad0bab437950.json`. Daemon pid 30304 restarted 19:55:25.909Z and never launched a run, so `.dev/state.json` has `run_id=null`/`events_path=null`; `record_runtime_event()` returns early when `events_path` is None (dev-loop.py:2333-2335), silently dropping the `check_wait_budget()` events (3915/3921). This is the events_path=None variant, distinct from the prior stale-path/ENOENT occurrences.
- Dispatch did not hold: codex claimed/launched 20:01:49.039Z/20:01:49.191Z then exited; `wake.retry` 20:07:52.113Z; opencode claimed/launched 20:12:42.414Z/20:12:42.536Z; attempts=2; queue-to-final-launch 654.86s (~10m54.9s); two launches for one wait_id (exactly-once queue held, one-launch target did not).
- Evidence-quality defects (known, appended as occurrences): (1) `evidence.reason` is a truncated worker-row tail (issue-6e08d89b55e3); (2) `build_id` 2026-11-24T19:27:02.742428Z is the latest EXE build id and `generation` is null; the real attempt id/target generation 611 appear only inside the reason (issue-a0fa88737830).

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source or build change, and did not create a duplicate issue for the recurring stage.
- Preserved all prior issue text; every record is append-only.

# Exact implementation changes

- None. Append-only issue records only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: issue observe/show, `.dev/build-timings/game-dll-20261124T200045986936Z-3f16b3.json`, build resource summary, `.dev/agent-wake/events.jsonl` and request file, `.dev/state.json`, `dev-loop.py --status`, and wait-policy source (`dev-loop.py:3839-3931`, `build_friction.py`).
- Review artifact: review on issue-a4eea8fe2246 (state `in_review`).

# Measured evidence

- Wait elapsed 60.828s at budget 60.0s; attempt total 134.394s (compile 125.197s).
- Host pressure: 99 samples @1s over 134.387s, mean CPU 99.87%, peak 100%, minimum available memory 160,858,112 B (~153.4 MiB); `runtime_alignment` mode_unverified (server bloodtest, client stale).
- Wake queued exactly once at 20:01:47.676Z; final opencode launch 654.86s after queue; attempts=2.
- Dev-loop wait event: not written (events_path=None).

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether owners will persist wait events when no run is active, fix wake dispatch latency/duplicates, fix the reason and build-id fields, and reduce compile fanout/time or scope the 60s budget.

# Agent handoff context

- Missing-wait-event owner: issue-6e2d7b91e7d9 (events_path=None variant); note sent to build-coordinator.
- Wake-delivery owner: issue-84c584957e65 (late/duplicate dispatch).
- Wait-evidence owners: issue-6e08d89b55e3 (reason) and issue-a0fa88737830 (attempt id/generation).
- Resource gap: issue-2a5c5da53565; gap_key `build-resource-sampling:game-dll-20261124T200045986936Z-3f16b3`.
- Next exact step: initialize/fallback the dev-loop journal when events_path is None so over-budget waits always record the two events. Shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.

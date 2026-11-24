# issue-74f939fb99fc Build-Wait Review

# Task

- Task ID: review build/test wait issue-74f939fb99fc (wake-5a26878e0275)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.629s, wait_id game-dll-build:2026-11-24T20:24:20.106Z
- Summary: Verified the wait against attempt `game-dll-20261124T202420131204Z-8cc10c`, its build-timings and resource summaries, `.dev/state.json`, and the agent-wake journal/request. Appended one review (verdict `confirmed`) to issue-74f939fb99fc, plus an occurrence to issue-84c584957e65 (late/duplicate reviewer dispatch) and an occurrence to issue-6e2d7b91e7d9 (missing dev-loop wait events). No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-24T21:25:00Z

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted source and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and append-only issue records written through the coordination API.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- Wait was real, not a stall. Attempt `game-dll-20261124T202420131204Z-8cc10c` started 2026-11-24T20:24:20.131Z (request `2026-11-24T20:24:20.026Z-a67f1373:mode3:game`; changed `src/entities/player-render.cpp`) and completed 20:26:28.777Z, `success=true`, total 128.646s, generation 617, revision `c7d650e...`.
- Phase split: `changed_source_preflight` 70.499s, compile 13.517s (slowest TU `src/entities/player-render.cpp` 70.412s), link 33.852s, object-cache scan 0.017s, publish 0.638s. The 60s threshold was crossed during changed-source preflight, not a hang.
- Host was saturated: 81 samples @1s over 128.632s, mean host CPU 99.99%, peak 100.0%, minimum available memory 16,961,536 B (~16.2 MiB).
- Issue contract held: wait_id, stage, reason, elapsed, budget, started_utc, request_ids, generation/source_generation, and evidence are present; readiness ready with no missing fields. Across 100 build-friction issues there are 90 unique non-empty wait_ids and 0 duplicate wait_ids, so one issue per wait occurrence held. `evidence.reason` is readable here, so the issue-6e08d89b55e3 tail symptom does not apply.
- Wake queue contract held: exactly one `wake.queued` (wake-5a26878e0275, 20:25:22.352Z) and one request file for this wait.
- Dispatch did not hold: codex claimed/launched 20:25:24 (pid 25496); `wake.retry` adapter=opencode 20:36:52; opencode claimed/launched 20:38:43 (pid 19140); attempts=2; two `wake.launched` rows for one wait_id; queue-to-final-launch ~800.7s (~13m20.7s). Recorded as an occurrence on issue-84c584957e65.
- PROOF GAP (recurrence of issue-6e2d7b91e7d9): the proof expectation's dev-loop-journal half is absent. No retained `logs/2026-11-24/**/dev-loop/events.jsonl` and no `.dev/**/*.jsonl` contains `dev-loop.wait.over_budget`, `dev-loop.wait.reviewer-wake`, or this wait_id/issue id. Current `.dev/state.json` events_path points at run `20261124_205956` (after the wait), so the journal active at 20:24 is not recoverable from current state. Cause unknown (events_path=None between runs vs rotated/absent journal); no ENOENT string observed for this occurrence. Recorded as an occurrence on issue-6e2d7b91e7d9.
- Evidence nuance: the issue `evidence.build_id` is `2026-11-24T19:27:02.742428Z` (latest EXE artifact) and `generation` is 616 (pre-build counter), while the live attempt produced generation 617. The wait_id and request_id pin the actual attempt, so this did not mislead the review, but it is a labeling nuance worth noting.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source or build change, and did not create a duplicate issue for the recurring stage.
- Preserved all prior issue text; every record is append-only.

# Exact implementation changes

- None. Append-only issue records only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: issue observe/show/list, `.dev/build-timings/game-dll-20261124T202420131204Z-8cc10c.json`, its `.resource.json`, `.dev/state.json`, `.dev/agent-wake/events.jsonl` and `requests/wake-5a26878e0275.json`, wait-policy source (`dev-loop.py:3839-3931`), and dev-loop/agent-wake journal scans.
- Review artifact: review on issue-74f939fb99fc (state `in_review`).

# Measured evidence

- Wait elapsed 60.629s at budget 60.0s; attempt total 128.646s (preflight 70.499s, link 33.852s).
- Host pressure: mean CPU 99.99%, peak 100.0%, minimum available memory 16,961,536 B (~16.2 MiB).
- Wake queued exactly once at 20:25:22.352Z; final opencode launch ~800.7s after queue; attempts=2; two launches for one wait_id.
- Dev-loop wait event: not present in any retained journal.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether owners will persist wait events for this run, fix wake dispatch latency/duplicates, and reduce build latency/fanout or scope the 60s budget.

# Agent handoff context

- Missing-wait-event owner: issue-6e2d7b91e7d9 (occurrence added; cause unknown).
- Wake-delivery owner: issue-84c584957e65 (occurrence added).
- Build-latency root: issue-136d4553ff92 (owner build-coordinator).
- Wait policy/parent: issue-9a4503f6ed68.
- Next exact step: confirm how the root dev-loop's events_path behaves between runs so over-budget waits always record the two events. Shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.

# issue-3875f423f9bf Build-Wait Review

# Task

- Task ID: review build/test wait issue-3875f423f9bf (wake-def06f3a23fd)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.094s, wait_id game-dll-build:2026-11-22T21:53:51.481Z
- Summary: Verified the wait against build attempt `game-dll-20261122T215351509781Z-dd7c11`, the build-timings summary, and the wake queue. Appended review id 216 (verdict `pass-with-evidence-gap`) to issue-3875f423f9bf. No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T22:17:12Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted source and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and the append-only review written through `agent-coord.py`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- Wait was real, not a stall. Attempt `game-dll-20261122T215351509781Z-dd7c11` started 21:53:51.509Z; `.dev/build-timings/game-dll-20261122T215351509781Z-dd7c11.json` reports `success: true`, generation 530, revision 778a07cc…, `total_seconds: 98.887`, `worker_total_seconds: 82.764`.
- The issue fired at 60.094s (created 21:54:51.819Z) while `src/network/server-npcs.cpp` was still compiling. That TU's `translation_unit.completed` is 21:55:15.993Z at 68.268s; `build.completed` is 21:55:30.405Z, 39.4s after the trip.
- Cost breakdown: server-npcs.cpp compile 68.268s (one TU alone exceeds the 60s budget); module_slot_wait 13.012s; link 10.238s; object_cache_scan 1.641s; publish 0.586s; identity-provider compile ~0.98s.
- Auto-intake and wake held: exactly one wake request for this occurrence (`wake-def06f3a23fd`, `WAKE_e01a770e3de64b4c89c1d46e90346bc2`, queued 21:54:53.377Z). `build_friction.record_wait_incident` dedupes by wait_id, so one issue is correct.
- Evidence gap (known, already appended to owner issue-136d4553ff92): the issue's `build_id` is `latest_successful_build_id` (`2026-11-22T21:32:48.195707Z:e24aaf16…`), not the in-flight attempt id, and `generation` is null; the attempt joins only through `started_utc`. `DevLoop.check_wait_budget` sets game-dll `build_id` from `self.latest_build` (`devscripts/dev-loop.py:3543`).
- No durable `dev-loop.wait.over_budget` / `dev-loop.wait.reviewer-wake` journal line was found for this wait_id under `logs/**/dev-loop/events.jsonl` or the root-loop logs; the wake-queue record and the issue are the proof of creation.
- Recurrence/noise: each distinct wait_id intentionally becomes a new build-friction issue (`build_friction.py:143-152`). On-disk count is 125 `game-dll-build at game_build` wait docs + 22 `exe-build` wait docs, matching the issue's own duplicate-risk note.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source or build change, and did not create a duplicate issue for the recurring stage.
- Preserved all prior issue text; the review is append-only.

# Exact implementation changes

- None. Append-only issue review only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: issue observe/show, `agent-wake` events and request, build-timings summary + timeline JSONL, `.dev/state.json`, and wait-policy source (`dev-loop.py:3486-3577`, `build_friction.py`).
- Review artifact: review id 216 on issue-3875f423f9bf (state `in_review`).

# Measured evidence

- Wait elapsed 60.094s at budget 60.0s; attempt total 98.887s (worker 82.764s).
- Slowest compile 68.268s (`src/network/server-npcs.cpp`); module_slot_wait 13.012s; link 10.238s.
- Wake queued exactly once at 21:54:53.377Z.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether owners will reduce the server-npcs.cpp compile/contention or make the wait budget progress/attempt-class aware.

# Agent handoff context

- Latency owner: issue-136d4553ff92 (active C++ latency work).
- Wait-policy/noise owner: issue-9a4503f6ed68.
- Next exact step: reduce the single-TU compile or derive the game-DLL budget from measured attempt class/forward progress; include the attempt id in wait evidence; record the over_budget/reviewer-wake events durably. Shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only review.

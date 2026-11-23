# issue-a240a6dd4eab Build-Wait Review

# Task

- Task ID: review build/test wait issue-a240a6dd4eab (wake-75c2d2420471)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.501s, wait_id game-dll-build:2026-11-22T22:04:56.185Z
- Summary: Verified the wait against build attempt `game-dll-20261122T220456195519Z-2dad5a`, its build-timings summary/timeline, and the wake queue. Appended review id 219 (verdict `pass`) and addendum review id 220 (verdict `addendum-identity-defect`) to issue-a240a6dd4eab, set related_build to the attempt id, and appended a systemic suggestion to umbrella issue-9a4503f6ed68. No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T22:34:00Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted source and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and the append-only reviews/suggestion written through `agent-coord.py`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- Wait was real, not a stall. Attempt `game-dll-20261122T220456195519Z-2dad5a` started 22:04:56.195Z; `.dev/build-timings/game-dll-20261122T220456195519Z-2dad5a.json` reports `success: true`, generation 533, source_generation 9, revision e0fd19ad…, `total_seconds: 76.976`, `worker_total_seconds: 73.073`.
- The wait fired at 60.501s (22:05:56.696Z) while the healthy build was inside `link.lld_linker`; `build.completed` was 22:06:13.181Z, ~17s after the trip.
- Cost breakdown: `changed_source_preflight` 51.697s (contains the 51.613s compile of `src/network/server-npcs.cpp`), identity-provider compile 1.010s, link 17.802s, publish 0.408s. A single TU plus link exceeds the 60s budget on its own.
- The same signature exceeds budget on adjacent healthy attempts: generation 530 server-npcs.cpp 98.887s, 532 server-npcs.cpp 61.685s, 533 server-npcs.cpp 76.976s, 536 npc-world-query.cpp 94.965s; npc-spawn.cpp builds (534/535) were 23.102s and 29.895s and stayed under budget.
- Auto-intake and wake held: exactly one wake for this occurrence (`wake-75c2d2420471`, `wake.queued` 22:05:57.243Z; request created 22:05:57.240Z). `build_friction.record_wait_incident` dedupes by wait_id, so one issue is correct.
- Identity defect (also reported by umbrella review 217): this issue's structured evidence records `generation: 532` and `build_id: 2026-11-22T21:32:48.195707Z` (the previous completed generation/build), while the in-flight attempt is generation 533 / `game-dll-20261122T220456195519Z-2dad5a`. The wait_id and the attempt id embedded in the truncated `reason` are correct, so traceability is recoverable, but the top-level build identity fields are stale (`dev-loop.py:3544` uses `self.game_build_generation`, assigned only on completion).
- Wake latency: queued 22:05:57.240Z, launched 22:18:00.753Z (~12m03s, attempts=2), after the build already succeeded, so the reviewer could not observe the live wait.
- No durable `dev-loop.wait.over_budget` / `dev-loop.wait.reviewer-wake` journal line was retained for this wait_id (run 20261122_204150 has no dev-loop/events.jsonl); the issue record, build-timings artifact, and wake request/event are retained.
- Recurrence/noise: `issue list` at 22:31Z shows 127 issues titled "Wait over budget: game-dll-build at game_build" (23 new, 103 in_review, 1 assigned), opened 2026-11-20T08:32:55Z through 2026-11-22T22:25:14Z. The issue's own duplicate-risk note has materialized.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source or build change, and did not create a duplicate issue for the recurring stage.
- Preserved all prior issue text; reviews are append-only.
- Followed gate 0d by filing the duplicate/noise and identity findings on the existing umbrella rather than creating a new issue.

# Exact implementation changes

- None. Append-only issue review/suggestion and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: `issue observe/show/readiness`, `agent-wake` event and request, build-timings summary + timeline JSONL, wait-policy source (`dev-loop.py:109`, `3486-3577`, `build_friction.py`), and `issue list`.
- Review artifacts: review 219 (`pass`), review 220 (`addendum-identity-defect`) on issue-a240a6dd4eab (state `in_review`); suggestion appended to issue-9a4503f6ed68.

# Measured evidence

- Wait elapsed 60.501s at budget 60.0s; attempt total 76.976s (worker 73.073s).
- Slowest compile 51.613s (`src/network/server-npcs.cpp`); link 17.802s; preflight 51.697s.
- Wake queued exactly once at 22:05:57.240Z; launched 22:18:00.753Z.
- 127 identical game-DLL wait issues currently recorded.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether owners will raise/derive the wait budget per attempt class and coalesce recurring waits under one signature.

# Agent handoff context

- Latency owner: issue-136d4553ff92 (active C++ latency work; server-npcs.cpp / npc-world-query.cpp compile cost).
- Wait-policy/identity/noise owner: issue-9a4503f6ed68 (reviews 217 and the 22:32Z suggestion).
- Next exact step: make the game-DLL wait budget stage/attempt-class aware (>=180-240s or derived from measured per-TU baseline), coalesce identical wait signatures into one rolling occurrence issue, keep `reason` as the readable `[GAME BUILD] ...` summary, set `generation`/`build_id` from the in-flight attempt, and expire stale reviewer wakes. Shared build/wait changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only reviews and suggestion.

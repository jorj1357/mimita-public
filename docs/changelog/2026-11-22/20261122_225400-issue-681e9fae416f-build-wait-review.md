# issue-681e9fae416f Build-Wait Review

# Task

- Task ID: review build/test wait issue-681e9fae416f (wake-c3d2731ff7b5)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 60.484s, wait_id game-dll-build:2026-11-22T22:24:13.556Z
- Summary: Verified the wait against build attempt `game-dll-20261122T222413577092Z-1031f3`, its build-timings summary, the dev-loop journal, and the wake queue. Appended review id 222 (verdict `pass-with-caveats`) to issue-681e9fae416f. No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T22:54:00Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted source and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog and the append-only review written through `agent-coord.py`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- Wait was real, not a stall. Attempt `game-dll-20261122T222413577092Z-1031f3` started 22:24:13.589Z; `.dev/build-timings/game-dll-20261122T222413577092Z-1031f3.json` reports `success: true`, generation 537, source_generation 13, revision 777456d6…, `total_seconds: 103.54`, `worker_total_seconds: 98.154`.
- The wait fired at 60.484s (22:25:23.240Z) while `src/npc/npc-avatar.cpp` was compiling; that TU completed 22:25:14 after 54.714s, and `build.completed` was 22:25:57.140Z, ~34s after the trip.
- Cost breakdown: `changed_source_preflight` 54.785s (contains the 54.714s `src/npc/npc-avatar.cpp` compile), link 38.445s, object_cache_scan 1.595s, publish 0.572s, identity-provider compile 2.043s. A single TU plus link exceeds the 60s budget on its own.
- Auto-intake and wake held: exactly one issue for this wait_id, one `dev-loop.wait.reviewer-wake` (journal line 79, queued=true), and one `dev-loop.wait.over_budget` (line 80, created=true). `.dev/agent-wake/events.jsonl` has exactly one `wake.queued` for `wake-c3d2731ff7b5` (22:25:15.086Z); codex launched 22:25:23.121Z, fell back to opencode, which claimed/launched 22:43:24.9Z. One wake request; the ~18m delivery latency is a known wake-latency observation.
- Reason defect (known, tracked): the stored `reason` is a JSON tail fragment (for example `...tree SSA incremental...progress_write_ms:2.354...`), not human text. `dev-loop.py:2832-2867` sets `game_build_detail = line.rstrip()[-240:]` for every worker line, so the long `[GAME_FILE]` `compiler_timing` payload tail becomes the wait reason at `dev-loop.py:3539`.
- Identity defect (known, tracked): the issue records `build_id: 2026-11-22T21:32:48.195707Z` and `generation: 536` (the previously published build) instead of the in-flight generation 537 / `game-dll-20261122T222413577092Z-1031f3`. `dev-loop.py:3543-3544` uses `self.latest_build` and `self.game_build_generation`, which are only updated on completion.
- Recurrence/noise: many sibling `Wait over budget: game-dll-build at game_build` issues already exist, matching the issue's own duplicate-risk note. No duplicate was created; the reason/identity/budget findings are already on umbrella issue-136d4553ff92 (budget also on issue-9a4503f6ed68).

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source or build change, and did not create a duplicate issue for the recurring stage.
- Preserved all prior issue text; the review is append-only.
- Followed gate 0d by recording the duplicate/noise, reason, and identity findings against the existing umbrella owners instead of opening a new issue.

# Exact implementation changes

- None. Append-only issue review and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: `issue observe/readiness`, `dev-loop --status`, the dev-loop journal, `.dev/agent-wake/events.jsonl`, build-timings summary, and wait-policy source (`dev-loop.py:3486-3577`, `build_friction.py:143-202`).
- Review artifact: review id 222 (`pass-with-caveats`) on issue-681e9fae416f (state `in_review`).

# Measured evidence

- Wait elapsed 60.484s at budget 60.0s; attempt total 103.54s (worker 98.154s).
- Slowest compile 54.714s (`src/npc/npc-avatar.cpp`); link 38.445s; preflight 54.785s.
- Exactly one issue and one reviewer wake for this wait occurrence; wake queued 22:25:15.086Z.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether owners will raise/derive the wait budget per attempt class, fix the reason field, carry the in-flight build identity, and coalesce recurring waits.

# Agent handoff context

- Latency owner: issue-136d4553ff92 (active C++ latency work; npc-avatar.cpp single-TU compile cost).
- Wait-policy/identity/noise owner: issue-9a4503f6ed68 and issue-136d4553ff92.
- Next exact step: make the game-DLL wait budget stage/attempt-class aware, keep `reason` human-readable (store raw output separately), set `generation`/`build_id` from the in-flight attempt, and coalesce identical wait signatures. Shared build/wait changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only review.

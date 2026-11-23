# issue-cd97259fdc20 Build-Wait Review

# Task

- Task ID: review build/test wait issue-cd97259fdc20 (wake-44675b502200)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 61.088s, wait_id game-dll-build:2026-11-22T23:52:22.109Z
- Summary: Verified the wait against dev-loop run `20261122_231306` and build attempt `game-dll-20261122T235222465818Z-9bfbf7`, then appended review verdict `reviewed-false-positive-wait-policy` to issue-cd97259fdc20. No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-23T05:57:00Z

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted edits (`docs/issues/README.md`, two improvement issue docs, `devscripts/blender-ai`, and a 2026-11-23 changelog). This session edited no source and reverted nothing.
- Files created by this session: this changelog and the append-only review written through `agent_coord.py`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- Wait was the expected budget trigger, not a stall. `logs/2026-11-22/20261122_231306/dev-loop/events.jsonl` line 155 records `dev-loop.wait.over_budget` at 23:53:35.729Z (created=true, elapsed 61.088s), and line 154 records exactly one `dev-loop.wait.reviewer-wake` (queued=true) for wait_id `game-dll-build:2026-11-22T23:52:22.109Z` -> issue-cd97259fdc20.
- In-flight attempt `game-dll-20261122T235222465818Z-9bfbf7` started 23:52:22.465Z and `build.completed` 23:56:49.678Z (exit 0, generation 546, 267.197s). At the 60s crossing (~23:53:22Z) `src/network/server-gamemode.cpp` was ~48s into a 151.157s `changed_source_preflight` compile and progressing.
- Cost breakdown for the attempt: preflight (single TU server-gamemode.cpp) 151.157s, identity-provider compile 38.576s, link 59.71s, object scan 1.60s, publish 0.49s. One heavy TU plus the monolithic link exceeds 60s on its own.
- Known payload defects persisted: `evidence.build_id=2026-11-22T21:32:48.195707Z` is the last published EXE artifact (not the in-flight attempt); `generation=545` lags the produced 546; `stage` collapses to `game_build`; `reason` is a raw progress-line tail. These match the gen546 latency analysis in `docs/changelog/2026-11-23/20261123_001722-build-wait-review-gen546-pressure-latency.md`.
- Exactly one `wake.queued` exists for this occurrence (`wake-44675b502200`, `.dev/agent-wake/events.jsonl` line 2027); the later retry/re-claim lines are stale-launch recovery of the same wake, not a second queued wake. Acceptance "reviewer wake queued exactly once" holds.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source or build change, and did not create a duplicate issue for the recurring stage.
- Preserved all prior issue text; the review is append-only.
- Followed gate 0d by recording duplication/noise and the reason/identity defects against the existing owners instead of opening a new issue.

# Exact implementation changes

- None. Append-only issue review and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: `issue observe/show/readiness`, dev-loop journal `20261122_231306`, `.dev/build-timings/game-dll-20261122T235222465818Z-9bfbf7.jsonl`, `.dev/agent-wake/events.jsonl`, and the wait-policy source (`dev-loop.py:3486-3577`, `build_friction.py:131-202`).
- Review artifact: verdict `reviewed-false-positive-wait-policy` on issue-cd97259fdc20 (state `in_review`).

# Measured evidence

- Wait elapsed 61.088s at budget 60.0s; attempt total 267.197s (current.json reports compiled 2, skipped 641, total_seconds 264.172).
- Slowest compile 151.157s (`src/network/server-gamemode.cpp`); identity-provider 38.576s; link 59.71s.
- 130 sibling `*-wait-over-budget-game-dll-build-at-game-build-*` issue docs exist (83 created 2026-11-22); no duplicate for this wait_id.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual review: not performed. Gameplay review: not applicable.
- Still unverified: whether owners will make the wait budget stage/attempt-class aware, carry the in-flight build identity, fix the `reason` field, and coalesce recurring waits.

# Agent handoff context

- Latency owner: issue-136d4553ff92 (host-pressure multiplier, server-gamemode.cpp, monolithic link).
- Wait-policy/identity/noise owner: parent issue-e044a931c54b.
- Next exact step: make the game-DLL wait budget stage/progress-aware, store the in-flight attempt id/generation, keep `reason` human-readable, and coalesce identical wait signatures. Shared build/wait changes need human approval.
- Do not redo: this attempt's evidence collection or the gen546 latency analysis.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only review.

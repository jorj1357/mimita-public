# Issue-e7b2635a5d05 Build-Wait Review

# Task

- Task ID: review build/test wait issue-e7b2635a5d05 (wake-1ffe8890fb10)
- Session ID: session-65d7a4371534
- Work session: work-174a9e2b5edb
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 62.662s, wait_id game-dll-build:2026-11-22T18:23:48.797Z
- Summary: Reviewed the issue, dev-loop run 20261122_174002 journal, the pinned attempt timing, the wait-policy source, and agent-wake events. Appended review id 197 (verdict `confirmed-wait-policy-noise`) to issue-e7b2635a5d05, one count-correction entry, and one occurrence entry to root-cause owner issue-136d4553ff92. No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T19:28:02Z

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted source and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only review and suggestions were written through `agent-coord.py`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code without human approval.

# Findings

- Occurrence: `dev-loop.wait.over_budget` at 2026-11-22T18:25:25.254Z (run 20261122_174002, events.jsonl line 125) recorded elapsed 62.662s, budget 60.0s, generation 510, stage `game_build`, reason = `[GAME_STAGE] manifest_validation / started / elapsed_ms 61092.586` for attempt `game-dll-20261122T182350171415Z-5d884f`.
- Not a stall: the same attempt later succeeded at generation 511 (result 18:34:41.284Z, total 651.033s). `.dev/build-timings/game-dll-20261122T182350171415Z-5d884f.json` reports total_seconds 656.083 with changed_source_preflight 284697.977 ms as the largest phase, so the 60s trip fired during normal forward progress.
- First divergence: `devscripts/dev-loop.py:3538` hardcodes stage `game_build` and ignores the already-parsed `[GAME_STAGE]` substage (`devscripts/dev-loop.py:2832-2868`); the real stage survives only inside the raw reason. `devscripts/dev-loop.py:109` applies one flat 60s budget to game-DLL builds whose measured attempts in this run took 186.6s, 191.9s, 221.2s, 354.8s, and 651.0s.
- Duplicate noise: wait_id embeds the per-attempt `game_build_started_utc` (`dev-loop.py:3536`) and `build_friction.py:164-169` keys uniqueness on wait_id, so every healthy build over 60s mints another issue. The brain currently holds 118 issues titled `Wait over budget: game-dll-build at game_build` (96 in_review, 21 new, 1 assigned).
- Attribution: the wait evidence reports generation 510 (previous success) instead of the in-flight attempt's generation 511; build_id is the EXE artifact 2026-11-22T17:39:49.763492Z, and there is no build_attempt_id or run_id field.
- Wakes: exactly one `wake.queued` exists for this wait (wake-1ffe8890fb10, 18:24:59.280Z). Delivery was not single: codex claimed/launched 18:25:16.4, then `wake.retry` "stale launched request recovered by scheduler" 18:33:43.4 and opencode claimed/launched 18:35:54.7 (attempts=2). That duplicate activation belongs to the wake scheduler, already tracked by issue-9a4503f6ed68 and issue-84c584957e65.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read the routed coordination state, appended the occurrence instead of creating a duplicate, kept build evidence separate from behavior proof, and made no source or build change.
- Preserved all prior issue text; only append-only reviews/suggestions were written.

# Exact implementation changes

- None. Append-only issue review and suggestions only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: issue observe/brief/readiness, dev-loop run events.jsonl, `.dev/build-timings/...5d884f.json`, wait-policy source (`dev-loop.py`, `build_friction.py`), and `.dev/agent-wake/events.jsonl` plus wake request files.
- Review artifact: review id 197 on issue-e7b2635a5d05 (`confirmed-wait-policy-noise`, state `in_review`).
- Occurrence artifact: appended to owner issue-136d4553ff92.

# Measured evidence

- Wait elapsed 62.662s at budget 60.0s; attempt `5d884f` total_seconds 656.083, changed_source_preflight 284697.977 ms.
- Same-run attempt totals: 191.867s, 221.220s, 354.840s, 651.033s, 186.598s.
- Open titles matching `Wait over budget: game-dll-build at game_build`: 118.
- Wake launches for wake-1ffe8890fb10: 2 (codex 18:25:16, opencode 18:35:54).

# Regression review

- Regression entry appended: no.
- Why: no behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether the owner will adopt a stage/progress-aware budget, and whether the wake scheduler stale-retry path is fixed separately.

# Agent handoff context

- Owner: issue-136d4553ff92 (build-coordinator) for the wait-policy root cause; wake duplication remains on issue-9a4503f6ed68 / issue-84c584957e65.
- Next exact step: derive the game-DLL budget from measured per-class duration or forward-progress gating, populate stage/reason from the parsed `[GAME_STAGE]` payload, add `build_attempt_id`/`run_id` to wait evidence, and coalesce repeats per attempt/signature. Requires human approval before changing shared build code.
- Do not redo: this run/attempt evidence collection or the duplicate-issue count.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only review and suggestions.

# Issue-13b902990adc Build-Wait Review

# Task

- Task ID: review build/test wait issue-13b902990adc (wake-1dbab16fcd3b)
- Session ID: session-4592c41753ed
- Work session: work-db80d510d399
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 63.662s, wait_id game-dll-build:2026-11-28T18:00:34.216Z
- Summary: Reviewed the issue, dev-loop run 20261128_171225 journal, the pinned attempt timing, the wait-policy source, and agent-wake events. Appended review id 581 (verdict `confirmed-genuine-build; evidence-partial-missing-wait-journal-events`) to issue-13b902990adc, plus one occurrence each to issue-9a4503f6ed68 (inline wake-launch timeout lost events) and issue-84c584957e65 (late/duplicate dispatch). No source, build, runtime, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-28T18:37:37Z

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted source and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only review and occurrences were written through `agent-coord.py`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code owned by another agent or without approval.

# Findings

- Wait: issue-13b902990adc, wait_id `game-dll-build:2026-11-28T18:00:34.216Z`, stage `game_build`, elapsed 63.662s vs budget 60.0s, source_generation 81, request_ids [build-request-12597f8bba32]. One issue created 18:01:38.716Z; one wake queued (wake-1dbab16fcd3b, 18:01:42.600Z). Exactly-once queue held.
- Not a stall: attempt `game-dll-20261128T180034349425Z-3007f5` succeeded, generation 825, total_seconds 134.187, completed 18:02:48.537Z; phase_durations_ms compile 96234, changed_source_preflight 10797, object_cache_scan 7031, link 4219. At the 60s trigger the build was mid-compile under 100% host CPU and low free memory.
- First divergence: proof expectation is 2/3. The required `dev-loop.wait.over_budget` and `dev-loop.wait.reviewer-wake` events are missing for this wait. Run 20261128_171225 events.jsonl line 822 records `dev-loop.wait.issue-failed` (wall 18:02:12.828Z): inline `agent-wake.py run --once` timed out after 30s; reason "could not record over-budget wait". The over_budget/reviewer-wake rows at lines 879/880 belong to the next wait `game-dll-build:2026-11-28T18:11:01.936Z` (issue-0e64599cd133).
- Cause: build_friction.wake_reviewer (build_friction.py:244-246) runs the adapter drain synchronously with timeout=30 and no try/except. dev-loop.check_wait_budget (dev-loop.py:3925-3950) wraps record_wait_incident + wake_reviewer in one try and had already added the wait_id to reported_wait_ids (dev-loop.py:3929); TimeoutExpired skips both events and there is no retry. Same defect as the 2026-11-26 occurrence on issue-9a4503f6ed68 (COORD_20769). The next wait in the same run emitted both events because its inline drain finished within 30s.
- Late/duplicate dispatch: wake.queued 18:01:42.600Z; codex launched 18:08:28.131Z (pid 30516); retry to opencode 18:12:23.833Z; opencode launched 18:31:34.718Z (pid 26348). Queue-to-final-launch 1792.118s (~29m52s), two wake.launched for one wait_id; the build had finished 1726s earlier.
- Identity gap: evidence build_id `2026-11-27T17:45:19.700118Z` and generation 820 are the prior latest_build, not the in-flight attempt (real generation 825). The in-flight id survives only inside the truncated reason. Tracked on issue-7d3dcfd7433b.
- Reason gap: reason is a mid-JSON tail (dev-loop.py:3130/3160 detail=line.rstrip()[-240:]). Tracked on issue-6e08d89b55e3.
- Journal path: this run's events_path is valid; the journal-half loss here is the inline-timeout path, NOT the stale-path/ENOENT loss on issue-6e2d7b91e7d9.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, appended occurrences instead of creating duplicate issues, kept build evidence separate from behavior proof, and made no source or build change.
- Preserved all prior issue text; only append-only reviews/occurrences were written.

# Exact implementation changes

- None. Append-only issue review and occurrences only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop, or second loop invoked.
- Evidence read: issue observe/show/readiness, run 20261128_171225 events.jsonl, `.dev/build-timings/game-dll-20261128T180034349425Z-3007f5.json` + `.resource.jsonl`, wait-policy source (dev-loop.py, build_friction.py), and `.dev/agent-wake/events.jsonl`.
- Review artifact: review id 581 on issue-13b902990adc (state `in_review`).
- Occurrence artifacts: appended to issue-9a4503f6ed68 and issue-84c584957e65.

# Measured evidence

- Wait elapsed 63.662s at budget 60.0s; attempt `-3007f5` total_seconds 134.187, compile 96.234s (largest phase).
- Host near-saturation: 100% host CPU from 18:01:05; free memory ~0.5-2.4 GB during compile.
- Wake launches for wake-1dbab16fcd3b: 2 (codex 18:08:28, opencode 18:31:34); queue-to-final-launch 1792.118s.
- Missing journal events for this wait_id: dev-loop.wait.over_budget, dev-loop.wait.reviewer-wake (only dev-loop.wait.issue-failed present, line 822).

# Regression review

- Regression entry appended: no.
- Why: no behavior or code change was made.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether the owner will decouple over_budget/reviewer-wake emission from the inline launch, and whether the wake scheduler late-retry path is fixed separately.

# Agent handoff context

- Owner: issue-9a4503f6ed68 (wait hook / wake_reviewer inline timeout) for the lost-events root cause; late dispatch remains on issue-84c584957e65; stale identity on issue-7d3dcfd7433b; reason on issue-6e08d89b55e3.
- Next exact step: make wake_reviewer's inline `run --once` best-effort (catch subprocess.TimeoutExpired) and/or emit `dev-loop.wait.over_budget` and `dev-loop.wait.reviewer-wake` from the record/queue result before any adapter launch attempt. Requires shared-authority owner review before changing dev-loop.py/build_friction.py.
- Do not redo: this run/attempt evidence collection or the wake timing extraction.
- Do not close as a build defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only review and occurrences.

# Issue-6413477d361d Independent Review (2026-11-23)

# Task

- Task ID: bounded independent review of issue-6413477d361d (wake-02e62d408ad8)
- Session ID: session-4c8dea8f3f68 / work-4dfc77eaa83b
- Owner: issue-review-sweeper
- Focus: build-friction "Wait over budget: game-dll-build at game_build" for the 2026-11-20T14:31:50.671Z wait; readiness, proof, artifacts, recurrence
- Summary: Reviewed readiness, proof status, linked artifacts, and build/wake journals. Appended one evidence review (verdict `verified`) to issue-6413477d361d and one measured dedupe proposal to issue-e044a931c54b. No source, build, runtime stop, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-23T08:47:00Z, ISO 8601
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 0abd6231aea08bed8580bff0c11369c01fc40bde
- Final commit: none (read-only review)

# Pre-existing changes

- The shared checkout already contained modifications from other agents (15 `src/` files, `config/analytics.json`, `devscripts/blender-ai`, `devscripts/game_build_worker.py`, issue docs, specs). This session edited no source and reverted nothing.
- Files appended by this session through `agent-coord.py`: `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md` (review id 244) and `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` (suggestion). This changelog was created by this session.
- No build, compiler, module worker, or dev-loop build invocation was performed.

# Requested behavior

Complete one bounded independent review of issue-6413477d361d without editing source, building, closing, or claiming a fix; record observed facts, inference, uncertainty, contrary evidence, and the next discriminating check, plus a measurable process proposal when supported.

# Findings

- Readiness is `ready=true`, `score=8`, `missing=[]`; state was and remains `in_review`, reviewers were `["build-wait-reviewer"]`, one prior review, no tasks, no proof record (`issue readiness`; `proof list` returned `[]`).
- Prior review (build-wait-reviewer, 2026-11-20T15:57:52Z, verdict `verified`) established the documented acceptance: wait_id `game-dll-build:2026-11-20T14:31:50.671Z`, stage `game_build`, elapsed 65.09s vs 60.0s, build_id `2026-11-20T07:47:09.981634Z`, request_ids `[2026-11-20T14:31:47.379Z-3b2ad217]`, and reviewer wake `wake-34db0f2241ed` queued exactly once at 2026-11-20T14:32:56.152Z.
- This wake `wake-02e62d408ad8` was queued exactly once (2026-11-23T08:34:39.639Z), claimed 08:34:40.259Z, launched 08:34:40.301Z.
- Original attempt `.dev/build-timings/game-dll-20261120T143150893302Z-296fc2.json`: success=true, generation 401, started 14:31:50.893Z, completed 14:44:44.551Z, total 773.631s; worker total 759.908s; compiled 15 / skipped 627; object_scan 451.381s; compile 139.136s; link 35.474s. At the 65.09s flag point it was in the `manifest_validation` prelude, later completed successfully — a threshold false positive at the crossing, not a stall.
- Current checked-out source still applies one flat total-elapsed threshold to game-dll builds: `WAIT_ISSUE_SECONDS=60` (dev-loop.py:109-110) tested against elapsed-since-start for stage `game_build` (dev-loop.py:3744-3760); no per-stage budget, no progress test.
- Dedupe key is the exact `wait_id` (build_friction.py:131-140, 143-169); a fresh build mints a fresh wait_id, so identical kind/stage/reason occurrences each open a new issue.
- Recurrence: 46 non-terminal issues currently share the exact title "Wait over budget: game-dll-build at game_build" (39 in_review, 6 new, 1 assigned), first 2026-11-20T08:32:55.917Z, last 2026-11-22T23:53:23.260Z.
- Limiting evidence: newest build `game-dll-20261123T083904326142Z-3f61fe` total 30.463s for a single-TU edit, well under 60s, so the flag does not fire on every build.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build/source evidence separate from behavior proof, made no source/build change, appended only, created no issue.
- Since the evidence shows the dedupe/flood root cause is already covered by parent issue-e044a931c54b, the proposal was appended there rather than duplicating a new improvement issue.

# Exact implementation changes

- None. Append-only issue review and suggestion only.

# Validation

- Build status: no build requested; no compiler, worker, build script, or dev-loop build invocation.
- Evidence read: `issue brief/readiness`, `proof list`, `issue list`/JSON count, `.dev/build-timings/game-dll-20261120T143150893302Z-296fc2.json`, `.dev/build-timings/game-dll-20261123T083904326142Z-3f61fe.json`, `.dev/agent-wake/events.jsonl`, `.dev/agent-coord/events.jsonl`, `devscripts/dev-loop.py:109-110,3699-3790`, `devscripts/build_friction.py:124-189`.
- Review artifact: issue-6413477d361d review appended (review id 244), verdict `verified`.
- Process proposal: appended to issue-e044a931c54b; measurable target <=3 non-terminal identical-title wait issues within 24h; smallest experiment is dedupe by kind+stage+root cause in `record_wait_incident`, validated by `devscripts/quality/test_build_friction.py` plus one live multi-TU build crossing 60s.

# Measured evidence

- Original wait: 65.09s vs 60.0s budget; attempt 296fc2 total 773.631s (object_scan 451.381s, compile 139.136s, link 35.474s), success gen 401.
- Recurrence: 46 non-terminal identical-title issues, span 2026-11-20T08:32:55Z..2026-11-22T23:53:23Z.
- Counter-sample: single-TU build gen 551 total 30.463s.
- Wake: wake-02e62d408ad8 queued once, claimed/launched within 0.7s.

# Regression review

- Regression entry appended: no. Read-only review, no behavior or code change.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Still unverified: whether the live dev-loop daemon matches HEAD (static-source evidence only), and whether any of the 46 issues represented a genuine hang rather than a healthy-progress threshold crossing.

# Agent handoff context

- Owner: unassigned on issue-6413477d361d; policy/dedupe owner is issue-e044a931c54b.
- Files to check: `devscripts/dev-loop.py:109-110,3699-3790` (`WAIT_ISSUE_SECONDS`, `check_wait_budget`), `devscripts/build_friction.py:124-189`.
- Do not redo: the 296fc2 build measurement, the wake-02e62d408ad8 exactly-once check, or the 46-issue recurrence count.
- Next exact step: read-only replay of every `.dev/build-timings/game-dll-*.json` to compute the elapsed at the first 60s crossing and whether compile/link output followed, classifying healthy-progress false positives versus genuine stalls.
- Do not close as fixed without a linked proof or reasoned exemption; the issue has no proof record and the flat-60s policy remains in source.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.

# issue-0567fb769b04 Build-Wait Review

# Task

- Task ID: review of over-budget game-DLL wait issue-0567fb769b04 (wake-bafa65966226)
- Session ID: session-c2de612baae4 / work-afe6a66b4b86
- Owner: build-wait-reviewer
- Focus: Automatic over-budget wait record, reviewer wake, and build evidence for one occurrence
- Summary: Reviewed the wait record, the pinned dev-loop journal, the build attempt, and its resource sample. Appended one review (verdict `acceptance-met-identity-gap`) and run/build IDs to issue-0567fb769b04, one occurrence to issue-9a4503f6ed68, and one resource occurrence to issue-ca5eb16d9e21. No source, build, runtime, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-24T23:20:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: eb92b02b21e62fd5ec496eee4dcad832f901bb7a
- Final commit: none (no commit made)

# Pre-existing changes

- The shared checkout already contained many modifications from other agents; this session reverted none and edited no source.
- Files this session changed: `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md`, `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md`, `docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md`, and this changelog. All issue edits were append-only through `agent-coord.py`; the issue coordinate store also writes those markdown files.
- Other modified files in `git status` belong to other agents and are not claimed here.

# Requested behavior

Inspect issue-0567fb769b04 and the exact dev-loop journal, complete any missing reproduction/expected/acceptance/paths/proof fields, and append the review and evidence to the issue. Add a recurring occurrence instead of a duplicate issue. Report any needed code fix with evidence and claim it separately. Do not edit source or build.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4 and the resource-evidence handoff: ran coordination begin/sync/observe, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for already-tracked defects.
- Existing wait-issue acceptance (`devscripts/build_friction.py:187-190`) was used as the review rubric.

# Exact implementation changes

- None in source, configuration, or runtime. Append-only issue context/review/occurrences and this changelog only.

# Findings

- Wait record: wait_id `game-dll-build:2026-11-24T22:28:54.886Z`, stage `game_build`, elapsed 60.363s vs 60.0s budget, build_id `2026-11-24T19:27:02.742428Z`, generation 626, source_generation 6, request `2026-11-24T22:28:53.757Z-1f524960:mode3:game`, revision `709286005d1c9eb1d984108245adc6e0f65b6ffab5ed2e20c7117005643f8b0d`.
- The wait was normal multi-minute work, not a stalled worker. Attempt `game-dll-20261124T222855538088Z-1d20e7` published generation 627 in total_seconds 300.575 (worker_total 270.433). Phases: changed_source_preflight 186.721s (src/main-systems.cpp 186.582s, src/physics/physical-entity.cpp 167.413s), compile wall 193.327s (compile_sum 360.586s over jobs=10), link 65.521s, publish 7.833s. The 60s budget fired ~126s before the result, during a single-TU preflight that alone exceeds it.
- Acceptance `The issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence`: partially met. wait_id/stage/reason/elapsed/evidence are present, but build_id and generation are the previously published values (19:27 build / gen 626), not the in-flight gen 627. Root cause: `check_wait_budget` reads `self.latest_build` and `self.game_build_generation`, the latter assigned only on completion (`devscripts/dev-loop.py:3259`), while `started_utc` is set at build start (`:3166`). The attempt resolves via wait_id + request_ids + source_generation + revision. related_build/related_run were empty; this review set them to `game-dll-20261124T222855538088Z-1d20e7` / `20261124_221014`.
- Acceptance `a reviewer wake is queued exactly once for this wait occurrence`: met. The journal has one `dev-loop.wait.reviewer-wake` and one `dev-loop.wait.over_budget` (created=true) for this wait_id at 2026-11-24T22:30:02.306Z, one wake `wake-bafa65966226`.
- Recurring (not this occurrence's defect): per-wait_id uniqueness mints one issue and one wake per build attempt. Store query found 227 build-friction issues, 0 done/cancelled, 47 created 2026-11-24, almost all this title. Aggregation is tracked in issue-9a4503f6ed68; this review appended an occurrence there instead of a duplicate.
- Build-phase resource pressure: `.dev/build-timings/game-dll-20261124T222855538088Z-1d20e7.resource.json` recorded 199 samples @1s over the attempt, mean host CPU 99.977%/peak 100.0%, minimum available memory 2,281,472 B (~2.18 MiB, single-sample minimum), no sampler errors. Concurrent client pid 26492 ran 35.4 FPS with 38/38 frames over the 4.167ms budget and 19 remote NPCs; server tick window 41 ticks/570ms, ai_ms 363.6. This is pressure correlation; whether it caused the 186.7s preflight is not established (no prebuild baseline).

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md` | Wait issue with empty related_build/related_run and no review | Same issue plus run/build context and an `acceptance-met-identity-gap` review | Record review and missing IDs | Restore from base; preserve append-only history by normal review |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` | Recurring-wait tracker | One appended occurrence | Add this exact witness without a duplicate issue | Restore from base by normal review |
| `docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md` | Resource-pressure tracker | One appended resource occurrence | Record build CPU/RAM pressure per AGENTS handoff | Restore from base by normal review |

# Contract / ABI changes

- None.

# State / schema changes

- None.

# Deleted paths

- None.

# Diagnostics

- Read-only journal evidence: `logs/2026-11-24/20261124_221014/dev-loop/events.jsonl` (lines 55-56 wait, lines 60-61 build result/ready), `.dev/build-timings/game-dll-20261124T222855538088Z-1d20e7.json` and `.resource.json`, and `.dev/state.json` (`game_build.requests` shows request `2026-11-24T22:28:53.757Z-1f524960:mode3:game` = success).
- No new diagnostic code or files were added.

# Validation

- Build status: no build requested and no compiler, worker, build script, or dev-loop invocation.
- Tests: none applicable; this was a read-only evidence review.
- Runtime evidence: the pinned attempt and journals above; no runtime process was started or stopped.
- Output files: this changelog and the append-only issue updates.

# Measured evidence

- Wait elapsed 60.363s at budget 60.0s; attempt total 300.575s; changed-source preflight 186.721s; compile wall 193.327s; link 65.521s.
- Wake: one `wake-bafa65966226`; one issue created for this wait_id; no duplicate issue or wake.
- Resource: mean host CPU 99.977%, peak 100.0%, minimum available memory 2,281,472 B, 199 samples; client 35.4 FPS / 19 NPCs.

# Regression review

- Regression entry appended: no. No behavior or code change was made.
- Why: this session recorded evidence only; the observed defects are already tracked in existing issues.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Multiplayer review: not applicable.
- Still unverified: whether an owner will adopt a stage-aware/stall-progress game_build budget and coalesce per-attempt issues; whether the stale wait identity will be fixed.

# Agent handoff context

- Owner: issue-9a4503f6ed68 (recurring wait policy/identity); issue-ca5eb16d9e21 (CPU/RAM pressure); issue-0567fb769b04 (this wait, now in_review).
- Files to check: `devscripts/dev-loop.py:3839-3930` (check_wait_budget), `:3158-3170` and `:3259` (build start/generation assignment), `devscripts/build_friction.py:143-252`.
- What another agent should not redo: re-derive this wait's build outcome, identity gap, or resource sample; they are recorded here and on the issues.
- Next exact step: carry in-flight `build_attempt_id`/`generation`/`run_id` in wait evidence and populate related_build/related_run at creation; adopt a stage-aware or stall-progress budget (>= observed p95 ~300s) with per-(kind,stage) coalescing. Shared build changes need human approval.
- Do not close the wait issue as a build defect; the underlying attempt succeeded.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / eb92b02b21e62fd5ec496eee4dcad832f901bb7a / none
- Git status: shared checkout with many pre-existing modifications from other agents; this session added only the issue appends and this changelog.
- Build identity: attempt `game-dll-20261124T222855538088Z-1d20e7`; generation 627; source_generation 6.
- Artifact: `build/game/generation-627/game-g627.dll`.
- Journal path: `logs/2026-11-24/20261124_221014/dev-loop/events.jsonl`.
- Session ID: session-c2de612baae4 / work-afe6a66b4b86.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review, context, and occurrences.

# Related feature record

- This wait: `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-0567fb769b04.md`; recurring wait policy `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md`; resource pressure `docs/issues/capability/20261124-multi-agent-dev-loop-memory-pressure-functionall-issue-ca5eb16d9e21.md`.

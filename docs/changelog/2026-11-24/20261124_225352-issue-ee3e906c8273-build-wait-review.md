# issue-ee3e906c8273 Build-Wait Review

# Task

- Task ID: review of over-budget game-DLL wait issue-ee3e906c8273 (wake-59dc989b28d0)
- Session ID: session-7e00e696f6c1 / work-5b6289e3f53a
- Owner: build-wait-reviewer
- Focus: Automatic over-budget wait record, reviewer wake, and build evidence for one occurrence
- Summary: Reviewed the wait record, the pinned dev-loop and agent-wake journals, and the build attempt. Appended one review (verdict `reproduced`, review id 399) plus run/build IDs to issue-ee3e906c8273, one occurrence to issue-84c584957e65 (wake dispatch), and one resource occurrence to issue-2a5c5da53565. No source, build, runtime, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-24T22:53:52Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: eb92b02b21e62fd5ec496eee4dcad832f901bb7a
- Final commit: none (no commit made)

# Pre-existing changes

- The shared checkout already contained many modifications from other agents; this session reverted none and edited no source.
- Files this session changed: `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md`, `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md`, `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md`, and this changelog. All issue edits were append-only through `agent-coord.py`; the issue coordinate store also writes those markdown files.
- Other modified files in `git status` belong to other agents and are not claimed here.

# Requested behavior

Inspect issue-ee3e906c8273 and the exact dev-loop journal, complete any missing reproduction/expected/acceptance/paths/proof fields, and append the review and evidence to the issue. Add a recurring occurrence instead of a duplicate issue. Report any needed code fix with evidence and claim it separately. Do not edit source or build.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: ran coordination begin/sync, read the router-selected evidence paths, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the already-tracked wake-dispatch defect.
- Existing wait-issue acceptance (`devscripts/build_friction.py:187-190`) was used as the review rubric.

# Exact implementation changes

- None in source, configuration, or runtime. Append-only issue review/context/occurrences and this changelog only.

# Findings

- Wait record: wait_id `game-dll-build:2026-11-24T21:43:56.034Z`, stage `game_build`, elapsed 61.245s vs 60.0s budget, build_id `2026-11-24T19:27:02.742428Z`, generation 623, source_generation 25, request `2026-11-24T21:43:54.624Z-f8b0d514:mode3:game`, revision `244e42841d167a4582d4f0ecb53f2cff017a6ef01b2c5ff5e9e51d13e5d67e5f`.
- The wait was real work, not a stalled healthy build. Attempt `game-dll-20261124T214357271677Z-7f59d1` started 21:43:57.271Z, ran 310.990s, and failed at `changed_source_preflight` in `src/main-systems.cpp` (261.697s) because `weaponsPtr`, `cameraPtr`, `playerPtr`, `npcSystemPtr`, and `worldPtr` are not members of `mimita::game` (lines 505-509).
- Acceptance `The issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence`: met. The only absent IDs were the dev-loop `run_id` and the active `build_attempt_id`; both were merged into evidence and `related_run` was set to `20261124_205956`.
- Acceptance `a reviewer wake is queued exactly once for this wait occurrence`: met. Exactly one `wake.queued` exists for `wake-59dc989b28d0` / issue-ee3e906c8273 at 21:45:00.693Z.
- Dispatch defect (already tracked in issue-84c584957e65, no duplicate filed): the same wake was launched twice and late. codex claimed/launched 21:45:05.480Z/21:45:07.333Z (pid 26096); `wake.retry` to opencode 21:51:21.275Z; opencode claimed/launched 21:59:10.107Z/21:59:10.601Z (pid 18976). Queue-to-final-launch 849.908s (~14m10s); two `wake.launched` rows for one wait_id.
- Wake-record misattribution: the `dev-loop.wait.reviewer-wake` event for this wait (21:45:08.562Z, wait `...21:43:56.034Z` / issue-ee3e906c8273) embeds `wake-dbeab2f5c771` (log `2026-11-24T21-01-34.352Z-build-wait-reviewer.log`, pid 33676). The next wait's reviewer-wake event (21:59:11.373Z, wait `...21:58:06.724Z` / issue-a2de003a1900) embeds `wake-59dc989b28d0`. This matches `build_friction.py:242-252` returning the tail of `agent-wake.py run --once`, which drained other pending wakes.
- Build-phase resource pressure: `.dev/build-timings/game-dll-20261124T214357271677Z-7f59d1.resource.json` recorded 124 samples @1s over 310.98s, 12 logical CPUs, mean host CPU 99.984%/peak 100.0%, minimum available memory 40,304,640 B (~38.4 MiB), no sampler errors. Runtime alignment was `mode_unverified` (server mode bloodtest, runtime run 20261124_212828, client mode stale). This is pressure correlation, not proof of the cause of the 261.7s preflight or the compile error.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md` | Wait issue with empty related_run and no build attempt id | Same issue plus context and a `reproduced` review | Record review and missing IDs | Restore from base; preserve append-only history by normal review |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` | Existing dispatch-defect issue | One appended occurrence | Add this exact witness without a duplicate issue | Restore from base by normal review |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` | Existing resource-pressure issue | One appended occurrence | Record build CPU/RAM pressure per AGENTS handoff | Restore from base by normal review |

# Contract / ABI changes

- None.

# State / schema changes

- None.

# Deleted paths

- None.

# Diagnostics

- Read-only journal evidence: `logs/2026-11-24/20261124_205956/dev-loop/events.jsonl` (lines 287-288, 340), `logs/2026-11-24/20261124_215719_01/dev-loop/events.jsonl` (line 48), `.dev/agent-wake/events.jsonl` (lines 2899-2918), `.dev/agent-wake/requests/wake-59dc989b28d0.json`, and `.dev/build-timings/game-dll-20261124T214357271677Z-7f59d1.json`/`.resource.json`.
- No new diagnostic code or files were added.

# Validation

- Build status: no build requested and no compiler, worker, build script, or dev-loop invocation.
- Tests: none applicable; this was a read-only evidence review.
- Runtime evidence: the pinned attempt and journals above; no runtime process was started or stopped.
- Output files: this changelog and the append-only issue updates.

# Measured evidence

- Wait elapsed 61.245s at budget 60.0s; attempt total 310.990s; changed-source preflight 261.697s.
- Wake: queued 21:45:00.693Z; codex launch 21:45:07.333Z; opencode launch 21:59:10.601Z; queue-to-final-launch 849.908s; two launches.
- Resource: mean host CPU 99.984%, peak 100.0%, minimum available memory 40,304,640 B, 124 samples.

# Regression review

- Regression entry appended: no. No behavior or code change was made.
- Why: this session recorded evidence only; the observed defects are already tracked in existing issues.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Multiplayer review: not applicable.
- Still unverified: whether an owner will bound reviewer-wake dispatch/launch identity and make the wait budget progress-aware; whether the compile error in `src/main-systems.cpp` has an owner.

# Agent handoff context

- Owner: issue-84c584957e65 (wake dispatch/late/duplicate/misattribution); issue-2a5c5da53565 (CPU/RAM pressure); issue-ee3e906c8273 (this wait, now in_review).
- Files to check: `devscripts/build_friction.py:205-252`, `devscripts/dev-loop.py:3839-3930`, `.dev/agent-wake/events.jsonl`, and the two dev-loop journals cited above.
- What another agent should not redo: re-derive this wait's wake counts, build outcome, or resource sample; they are recorded here and on the issues.
- Next exact step: have the wake owner capture the wake id returned by `agent-wake.py add` for the reviewer-wake event instead of the `run --once` tail, and use a persistent/adapter-specific drain so one wait yields one launch within the <=5 min target. Shared build changes need human approval.
- Do not close the wait issue as a build defect without owner/human review; the underlying attempt failed to compile.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / eb92b02b21e62fd5ec496eee4dcad832f901bb7a / none
- Git status: shared checkout with many pre-existing modifications from other agents; this session added only the issue appends and this changelog.
- Build identity: attempt `game-dll-20261124T214357271677Z-7f59d1`; generation 623; source_generation 25.
- Artifact: build failed, no published generation from this attempt.
- Journal path: `logs/2026-11-24/20261124_205956/dev-loop/events.jsonl` and `.dev/agent-wake/events.jsonl`.
- Session ID: session-7e00e696f6c1 / work-5b6289e3f53a.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review, context, and occurrences.

# Related feature record

- Wait-policy intake and wake behavior: `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ee3e906c8273.md`; dispatch defect `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md`; resource pressure `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md`.

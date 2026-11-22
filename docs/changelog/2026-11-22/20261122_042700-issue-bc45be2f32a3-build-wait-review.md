# issue-bc45be2f32a3 Build-Wait Review

# Task

- Task ID: build/test wait review wake-dfa7421507d6
- Session ID: session-d070f99ead61 / work-d652585bc11e
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-bc45be2f32a3 (game-dll-build wait over 60s at stage `game_build`)
- Summary: Reviewed the exact wait occurrence against the pinned dev-loop journal and attempt `game-dll-20261122T041048501387Z-47e1f2`. The 61.256s trip landed ~26s into a healthy 55.4s changed-TU preflight compile; ~96s of forward-progressing work remained and the attempt succeeded (generation 451, total 157.422s). Appended one evidence review (verdict `not-wedged-policy-miscalibrated`, review id 116), set the issue `in_review`, and added one measured suggestion to issue-136d4553ff92. No source edits, build, run, fix, or closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T04:27:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d595a308483a9f82c88349a5915c82a0ca

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents (including `src/network/multiplayer-tick.cpp`, `src/npc/npc.cpp`, `devscripts/game_build_worker.py`, many `docs/issues/*` and `docs/changelog/*` files). This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review and the suggestion were written through `agent-coord.py issue review` and `issue suggest`; those commands appended to `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bc45be2f32a3.md` and `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.

# Requested behavior

Review build/test wait issue issue-bc45be2f32a3 without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete any missing readiness fields, append the review and evidence, and report a needed code fix separately.

# Findings

- Wait identity: wait_id `game-dll-build:2026-11-22T04:10:48.461Z`, kind `game-dll-build`, stage `game_build`, reason `[GAME BUILD] revision=57aee374 sources=642 jobs=10 priority=high cache=build\game\objects\b10e3a31b3ae8637 seeded=0`, elapsed 61.256s vs budget 60.0s, started_utc 04:10:48.461Z, build_id `2026-11-22T02:37:06.972018Z`, generation null, source_generation 0, request_ids `["2026-11-22T04:10:48.383Z-73f531d2"]`.
- Issue record completeness: readiness `ready=true`, `missing=[]`; wait_id, stage, reason, elapsed, evidence, and reviewer wake present. Exactly one build-friction issue exists for this wait_id (checked all build-friction issues; no duplicate).
- Journal `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` carries exactly one `dev-loop.wait.over_budget` (04:11:51.893Z) and one `dev-loop.wait.reviewer-wake` (04:11:51.892Z) for this wait_id; `created=true`; wait-id dedupe held.
- Wake lineage: wake-dfa7421507d6 queued 04:11:50.232Z, claimed/launched via codex 04:11:51.8Z (pid 20920); stale launch recovered and retried to opencode 04:15:46.257Z, claimed/launched 04:15:49.1Z (pid 24020). One wake request; the second launch is a retry, not a duplicate. This review was delivered by the opencode retry.
- Pinned attempt `game-dll-20261122T041048501387Z-47e1f2`: started 04:10:48.506Z, completed 04:13:25.937Z, success true, no_op false, generation 451, total_seconds 157.422, worker_total_seconds 120.791, changed_paths `["src/network/multiplayer-tick.cpp"]`.
- Pre-worker stages: resident_state_codegen 0.43s, packet_registry_codegen 1.59s, manifest_snapshot 19.01s, manifest_validation 0.01s, source_snapshot 9.90s; `dev-loop.game-build.started` at 04:11:23.647Z (~35.1s after attempt start).
- Phases: changed_source_preflight 55.517s, compile 23.630s wall, link 37.387s, object_cache_scan 2.426s, publish 0.541s. 13 TUs compiled, 630 skipped; compile_sum 182.77s across 13 parallel jobs, max single TU `src/network/multiplayer-tick.cpp` 55.402s. The TU's `dev-loop.game-build.file compiled` event is 04:12:20.752Z.
- Forward progress: the 61.256s deadline landed ~26s into the 55.40s changed-TU preflight compile; ~96s of work remained after the trip and every interval produced progress. No hang, crash, or no-op.
- First divergence (evidence attribution): structured evidence records the prior artifact `2026-11-22T02:37:06.972018Z`, generation null, source_generation 0; the in-flight attempt id and resulting generation 451 are absent. `dev-loop.py:3532-3533` derives these from `self.latest_build` and a post-success `self.game_build_generation`.
- Second divergence: `occurrences` stays empty; the terminal result (157.422s, generation 451) is never appended for the same wait_id.
- Third divergence: wait `stage` says `game_build` while the effective active stage at trip time was `changed_source_preflight`, and the reason is the generic build line; also the reviewer-wake `launch_result` is captured from `agent-wake.py run --once` stdout and may describe a different queued wake.
- Root cause of the friction: `dev-loop.py` uses a flat `WAIT_ISSUE_SECONDS = 60` (dev-loop.py:109-110) applied to game-dll-build from attempt start (dev-loop.py:3520-3536); it counts ~35s of pre-worker stages plus the 55.5s changed-TU preflight, so any edit to a heavy TU structurally trips it even when the build is healthy.

# Specification alignment

- Followed `AGENTS.md` gates 0/0b/0c/0d and evidence rules, `docs/specs/dev-loop/change-to-evidence-latency.md` (budgets/outliers, build-friction dedupe and later observations), `docs/specs/dev-loop/agent-coordination.md`, and `docs/workflows/issue-intake-and-triage.md`.
- The spec expects measured per-class budgets, append-only same-wait later observations, and truthful per-stage accounting. The flat 60s budget is below the observed whole-game DLL distribution; a budget change needs human approval, so this review records the mismatch instead of changing policy.

# Exact implementation changes

- None to source. Append-only issue review (id 116, verdict `not-wedged-policy-miscalibrated`) and one `issue suggest` measured occurrence on issue-136d4553ff92 (natural systemic owner).

# Validation

- Build status: no build requested; no compiler, module worker, or second dev-loop was invoked.
- Evidence read: the dev-loop over_budget/reviewer-wake/game-build stage/file/result events, `.dev/build-timings/game-dll-20261122T041048501387Z-47e1f2.json`/`.jsonl`, `.dev/agent-wake/events.jsonl`, `devscripts/dev-loop.py` (`WAIT_ISSUE_SECONDS`, `check_wait_budget`), `devscripts/build_friction.py`, and the change-to-evidence-latency spec.
- Review artifact: issue-bc45be2f32a3 review id 116; state now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8.

# Measured evidence

- Recorded wait: elapsed 61.256s, budget 60.0s (over by 1.256s); issue created 04:11:49.819Z; wake queued 04:11:50.232Z.
- Build attempt: generation 451, success, total 157.422s; changed_source_preflight 55.517s (single TU 55.402s), compile wall 23.630s (13 TUs, 630 skipped), link 37.387s.
- Time from trip to publish: ~96s (04:11:49.7Z trip -> 04:13:25.937Z completion).

# Regression review

- No regression entry appended. The wait correctly detected a >60s window on a successful build; the recurring cause is flat-budget policy plus evidence-attribution gaps, not a broken DLL build.

# Human acceptance

- Not applicable (build/test wait review). Whether to re-base the game-dll-build wait clock on worker start, add forward-progress gating, or adopt a measured class budget remains a human decision.

# Agent handoff context

- Owner: unassigned; systemic build-stage/latency owner is `build-coordinator` on issue-136d4553ff92 (task `dll-build-stage-accounting`). Optional separate compile-time item: `src/network/multiplayer-tick.cpp` is a 55.4s single TU.
- Next exact step: apply the same recommendation as sibling issue-2b6479feedc1 — start the game-dll-build clock at `dev-loop.game-build.started`, gate issue creation on absence of forward progress, and/or use a measured p95 class budget; record the in-flight attempt id/target generation and append the terminal result as an occurrence.
- Do not redo: this wait-id, wake, build-attempt, and journal review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.

# issue-4d9eb4b1a8ad Build-Wait Review

# Task

- Task ID: build/test wait review wake-19f1e3a36126
- Session ID: session-cbc2ac0ecbdf / work-24e925f972fc
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-4d9eb4b1a8ad (exe-build wait over 60s at stage `compiling`)
- Summary: Reviewed the exact wait occurrence against the pinned dev-loop journal and the no-op EXE build's progress record. The wait is a truthful, benign over-budget sample (61.087s vs 60.0s) of a no-op EXE build that published successfully at 04:05:15.090Z, about 132s after the wait clock started. Appended one evidence review (verdict `not-wedged-noop-build-overhead`, review id 115), set related build/run, and added one standalone suggestion. Also found a wake-attribution defect in the dev-loop journal. No source edits, build, run, fix, or closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T04:25:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d595a308483a9f82c88349a5915c82a0ca

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents (including `src/npc/npc.cpp`, `src/network/multiplayer-tick.cpp`, `devscripts/game_build_worker.py`, many `docs/issues/*` and `docs/changelog/*` files). This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review, context fields, and suggestion were written through `agent-coord.py issue review`, `issue context`, and `issue suggest`.
- The issue detail doc `docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-4d9eb4b1a8ad.md` already existed (untracked, auto-created with the quote only); this session appended the review/suggestion blocks.

# Requested behavior

Review build/test wait issue issue-4d9eb4b1a8ad without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete any missing readiness fields, append the review and evidence, and report a needed code fix separately.

# Findings

- Wait identity: wait_id `exe-build:10:2026-11-22T04:03:03.139Z`, kind `exe-build`, stage `compiling`, reason `compiling 0/45 [CXX] src\renderer\renderer.cpp`, elapsed 61.087s vs budget 60.0s, started_utc 04:03:03.139Z, build_id `2026-11-22T02:37:06.972018Z`, generation 10, source_generation 11, request_ids `["2026-11-22T03:52:50.598Z-b2290797"]`.
- Issue record completeness: readiness `ready=true`, `missing=[]`; wait_id, stage, reason, elapsed, evidence, and reviewer wake present. related_build/related_run were empty and were filled by this session.
- Journal `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` carries exactly one `dev-loop.wait.over_budget` (line 581) and one `dev-loop.wait.reviewer-wake` (line 580) for this wait_id; wait-id dedupe held and exactly one wake was queued.
- Request lineage: the wait belongs to build request `2026-11-22T03:52:50.598Z-b2290797` from `codex-ingame-gui-reconnect` (accepted 03:52:53.167Z, line 529), the same request already reviewed as the queue wait issue-f92715ba3c7f. It was queued behind game-DLL generations 449 and 450 and only began the EXE phase after gen450 published at 04:03:02.938Z (line 578).
- Outcome: the EXE build was a no-op that published successfully. `build/progress.json` (pid 11356, written 04:04:00.047Z) records `result=noop`, `compiled=0`, `skipped=87`, `elapsed_s=43.014`, `link_reason=current`, `phase_seconds.compile_s=0.255`. `dev-loop.exe-cutover` fired 04:04:27.068Z (line 582), `dev-loop.cap-render` 04:05:14.981Z (line 583), and `dev-loop.build.completed` 04:05:15.090Z (line 584) with `error=""`.
- First divergence (wait observability): `docs/specs/dev-loop/change-to-evidence-latency.md:122-127` says an over-budget wait appends later observations, but `devscripts/dev-loop.py:3537-3545` adds the wait_id to `reported_wait_ids` after the first crossing and never re-records it. The issue therefore keeps `elapsed_seconds=61.087s` while the real wait window ran ~132s (04:03:03 wait start -> 04:05:15 publish). The recorded stage/reason (`compiling 0/45 ... renderer.cpp`) also did not describe the actual work: no translation unit compiled, and most of the window was fixed setup plus the post-compile cutover and CAP_RENDER gates.
- Second divergence (wake attribution): the dev-loop journal's line 580 `wake.launch_result` embeds the previous occurrence's prompt, log, and wake id (issue-f92715ba3c7f, wake-8ce6b21fbc58, log `2026-11-22T03-53-55.583Z`). The correct wake is `wake-19f1e3a36126` (queued 04:04:06.243Z, `.dev/agent-wake/events.jsonl` L1173; claimed/launched via codex L1174-L1175, stale-recovered L1181, relaunched via opencode L1184-L1185). The dev-loop journal alone cannot prove which wake belongs to this issue.
- Artifact identity observation: `.dev/builds/2026-11-22T02-37-06.972018Z/build-identity.json` still names `source_generation: 1`, while `dev-loop.build.completed` stamps `source_generation: 10`; the reused EXE artifact under-reports the generation.
- Root cause of the friction: dev-loop.py uses a flat `WAIT_ISSUE_SECONDS = 60` (dev-loop.py:109-110) for exe-build, measured from `active_build_started` (dev-loop.py:3483-3501). No-op EXE builds still run setup + build.py (43s) + exe-cutover + cap-render, which is close to or above 60s, so healthy no-op EXE builds structurally trip this issue. The spec expects per-class budgets and truthful per-stage accounting.

# Specification alignment

- Followed `AGENTS.md` gates 0/0b/0c/0d and evidence rules, `docs/specs/dev-loop/change-to-evidence-latency.md` (budgets/outliers, build-friction rules, later observations), `docs/specs/dev-loop/agent-coordination.md`, and `docs/workflows/issue-intake-and-triage.md`.
- The issue-and-wake creation path matches the spec; the divergences are the missing same-wait append and the stale stage/reason in the wait payload. Changing the budget needs human approval, so this review records the mismatch rather than changing policy.

# Exact implementation changes

- None to source. Append-only issue review (id 115, verdict `not-wedged-noop-build-overhead`), related_build/related_run context on issue-4d9eb4b1a8ad, and one `issue suggest` follow-up (wake attribution plus EXE gate-stage accounting; consolidates the budget change under issue-e044a931c54b and issue-136d4553ff92).

# Validation

- Build status: no build requested; no compiler, module worker, or second dev-loop was invoked.
- Evidence read: the dev-loop journal over_budget/reviewer-wake/control/build-completed/exe-cutover/cap-render events, `build/progress.json`, `.dev/builds/2026-11-22T02-37-06.972018Z/build-identity.json`, `.dev/agent-wake/events.jsonl`, `devscripts/dev-loop.py` (`check_wait_budget`, `WAIT_ISSUE_SECONDS`, `reported_wait_ids`), and the change-to-evidence-latency spec.
- Review artifact: issue-4d9eb4b1a8ad review id 115; state now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8; related_build/related_run set.

# Measured evidence

- Recorded wait: elapsed 61.087s, budget 60.0s (over by 1.087s); issue created 04:04:04.297Z; wake queued 04:04:06.243Z.
- EXE build: no-op (`compiled=0`, `skipped=87`, `elapsed_s=43.014`, compile_s 0.255, link_reason current); `exe-cutover` 04:04:27.068Z; `cap-render` 04:05:14.981Z; publish 04:05:15.090Z, build `2026-11-22T02:37:06.972018Z`, request `2026-11-22T03:52:50.598Z-b2290797`, error empty. Wall time from wait start to publish: ~132s.
- Concurrent game-DLL builds before the EXE phase: gen 449 and gen 450 (ready 04:03:02.938Z).

# Regression review

- No regression entry appended. The wait is a correct detection of a >60s window on a successful no-op build; the recurring cause is flat-budget policy plus missing same-wait appends and stale stage accounting, not a broken EXE build.

# Human acceptance

- Not applicable (build/test wait review). Whether an EXE-class budget change, same-wait appends, and gate-stage timing are acceptable remains a human decision.

# Agent handoff context

- Owner: unassigned; wait-instrumentation owner is `build-runtime-subagents` on issue-e044a931c54b; systemic build-stage/latency owner is `build-coordinator` on issue-136d4553ff92.
- Next exact step: for a still-waiting wait_id, append continued over-budget observations so elapsed reflects the full wait; record the wake request id/prompt hash at queue time; record EXE `exe-cutover` and `CAP_RENDER` stages; and use a measured EXE-class budget.
- Do not redo: this wait-id, wake, no-op-build, and journal review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.

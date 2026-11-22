# Issue-ec19949cbd21 Build-Wait Review

# Task

- Task ID: build/test wait review wake-2d0f489bc1e1
- Session ID: session-ed88deb4d323 / work-d465dc341c12
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-ec19949cbd21 (game-dll-build wait over 60s at stage game_build)
- Summary: Reviewed the exact wait occurrence, the in-flight build attempt and its stage durations, wait-id dedupe, the reviewer wake record including its delivery delay, and the dev-loop journal. Attributed the issue to its real in-flight build attempt and appended one evidence review (verdict `confirmed-wait-policy-noise`, review id 106) to issue-ec19949cbd21. No source edits, no build, no run, no fix, and no closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T03:50:16Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d5

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review and build attribution were written through `agent-coord.py issue review` and `issue context`.

# Requested behavior

Review build/test wait issue issue-ec19949cbd21 without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete any missing readiness fields, append review and evidence, and report a code fix separately if one is needed.

# Findings

- Wait identity: wait_id `game-dll-build:2026-11-22T03:03:00.564Z`, kind `game-dll-build`, stage `game_build`, elapsed 60.944s vs 60.0s budget, recorded 2026-11-22T03:04:04.544Z, request `auto-4`.
- Issue record completeness: readiness `ready=true`, `missing=[]`; wait_id, stage, reason, elapsed, evidence, and reviewer wake present. No missing reproduction/expected/acceptance/paths/proof fields to add.
- Journal `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` lines 373-374 carry exactly one `dev-loop.wait.over_budget` and one `dev-loop.wait.reviewer-wake` for this wait_id; wait-id dedupe held.
- Not a hang: attempt `game-dll-20261122T030300829638Z-7fb0c2` (source_generation 4) succeeded; generation 444 DLL published 2026-11-22T03:14:41.487Z, total 700.612s, worker 435.991s. At the 60.944s deadline it was inside `codegen:packet_registry_codegen.py` (03:03:04.043Z to 03:05:40.605Z = 156.6s), actively progressing.
- Dominant phases (`.dev/build-timings/game-dll-20261122T030300829638Z-7fb0c2.json`): changed_source_preflight 227.747s (npc.cpp compile 227.402s), link 145.984s (gnu_linker 145.846s), publish 89.796s (external_dependency_manifest 73.773s), compile wall 38.242s.
- First divergence: the flat 60s `WAIT_ISSUE_SECONDS` (`devscripts/dev-loop.py:109`) is below normal whole-game DLL build cost. `check_wait_budget` (`devscripts/dev-loop.py:3520-3536`) measures age from `game_build_started_utc`, so any normal build over 60s files an issue regardless of progress. Corpus `.dev/build-timings/game-dll-*.json` (n=177): p50 140.8s, p75 377.5s, p90 1004.1s; 110/177 (62.1%) exceed 60s.
- Attribution defect (persists): the wait evidence's `build_id=2026-11-22T02:37:06.972018Z` and `generation=443` describe the previously published artifact; the in-flight attempt (generation 444) appears only in the free-text `reason`. Set `related_build` to `game-dll-20261122T030300829638Z-7fb0c2`.
- Recurrence: the brain holds 59 issues titled `Wait over budget: game-dll-build at game_build` (42 in_review, 17 new) out of 83 build-friction issues. `record_wait_incident` (`devscripts/build_friction.py:131-169`) dedupes only by exact wait_id, and wait_id embeds a timestamp, so every >60s occurrence creates a new issue instead of an occurrence on a canonical one.
- Reviewer wake delivery latency: wake-2d0f489bc1e1 created 03:04:02.397Z but claimed/launched 03:18:32.768Z/03:18:32.891Z (adapter opencode, attempts=2), ~14.5 min late. Detection and queueing were correct; delivery lagged.

# Specification alignment

- Followed the router's issue-intake-and-triage, agent-proof-and-verification, change-to-evidence-latency, and agent-coordination guidance.
- Treated wait detection, build identity, wake delivery, and behavior as separate claims; no behavior completion was claimed.

# Exact implementation changes

- None to source. Append-only issue review (id 106, verdict `confirmed-wait-policy-noise`) on issue-ec19949cbd21, plus `related_build` attribution to the actual attempt.

# Validation

- Build status: no build requested; no compiler, module worker, or second dev-loop was invoked.
- Evidence read: `.dev/build-timings/game-dll-20261122T030300829638Z-7fb0c2.json` and `.jsonl`, `.dev/agent-wake/requests/wake-2d0f489bc1e1.json`, dev-loop journal over_budget/reviewer-wake events, 177-sample `.dev/build-timings` corpus, `devscripts/build_friction.py`, `devscripts/dev-loop.py`.
- Review artifact: issue-ec19949cbd21 review id 106; state now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8.

# Measured evidence

- This attempt: total 700.612s, worker 435.991s; changed_source_preflight 227.747s, link 145.984s, publish 89.796s, compile wall 38.242s.
- Wait class counts: 83 build-friction issues; 59 `game-dll-build at game_build` wait issues (42 in_review, 17 new).
- Reviewer wake created 03:04:02.397Z, launched 03:18:32.891Z (~14.5 min).

# Regression review

- No regression entry appended. The wait is a correct detection of a >60s build; the recurring cause is budget calibration plus duplicate-issue policy, not a newly broken build.

# Human acceptance

- Not applicable (build/test wait review). Whether a calibrated game_build budget and signature-based dedupe are acceptable policy remains a human decision.

# Agent handoff context

- Owner: unassigned; systemic build-stage/latency owner is `build-coordinator` on issue-136d4553ff92; wake-delivery latency relates to `build-runtime-subagents`.
- Next exact step: define per-class/per-stage budgets from `.dev/build-timings`, dedupe wait issues by kind+stage signature, fix active-attempt attribution, and retry the reviewer-wake delivery path within budget.
- Do not redo: this wait-id, wake, timing, and journal review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review.

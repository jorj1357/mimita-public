# Issue-0968dd48e9d8 Build-Wait Review

# Task

- Task ID: build/test wait review wake-ebcddab6043e
- Session ID: session-0492ad03b625 / work-fc18e82312f7
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-0968dd48e9d8 (game-dll-build wait over 60s at stage game_build)
- Summary: Reviewed the exact wait occurrence, its in-flight build attempt and stage durations, wait-id dedupe, the reviewer wake record, and the dev-loop journal. Attributed the occurrence to attempt `game-dll-20261122T034509935980Z-c4495d` (generation 448), appended one evidence review (verdict `not-wedged-policy-miscalibrated`, review id 110), set `related_build`/`related_run`, and appended the occurrence evidence to the build-latency owner issue-136d4553ff92. No source edits, no build, no run, no fix, and no closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T04:09:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d5

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review and build attribution were written through `agent-coord.py issue review` and `issue context`.

# Requested behavior

Review build/test wait issue issue-0968dd48e9d8 without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete any missing readiness fields, append review and evidence, and report a code fix separately if one is needed.

# Findings

- Wait identity: wait_id `game-dll-build:2026-11-22T03:45:09.661Z`, kind `game-dll-build`, stage `game_build`, elapsed 66.172s vs 60.0s budget, request ids `auto-8`, changed path `src/npc/npc.cpp`.
- Issue record completeness: readiness `ready=true`, `missing=[]`; wait_id, stage, reason, elapsed, evidence, and reviewer wake present. Set `related_build=game-dll-20261122T034509935980Z-c4495d` and `related_run=20261122_011031` for attribution.
- Journal `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` carries exactly one `dev-loop.wait.over_budget` (line 498) and one `dev-loop.wait.reviewer-wake` (line 497) for this wait_id; wait-id dedupe held.
- Not a hang: the whole 66.172s elapsed BEFORE the compiler started. `dev-loop.game-build.started` fired at 2026-11-22T03:46:16.810Z (line 495), after the wait tripped at 03:46:15.83Z. The pre-worker setup alone exceeded the flat 60s budget.
- Pre-worker stage durations (journal lines 484-494): `codegen:resident_state_codegen` 0.87s, `codegen:packet_registry_codegen` 3.69s, `manifest_snapshot` 11.65s, `manifest_validation` 0.04s, `source_snapshot` 35.40s (03:45:28.967Z -> 03:46:04.802Z). Roughly 55s of stages plus handoffs accounts for the 66.172s trip.
- Outcome: attempt `game-dll-20261122T034509935980Z-c4495d` succeeded; generation 448 published/ready 2026-11-22T03:48:59.457Z, status `ok`, total 229.247s, worker 160.163s.
- Dominant phases (`.dev/build-timings/game-dll-20261122T034509935980Z-c4495d.json`): changed_source_preflight 87.27s, compile 121.33s (max single TU `src/npc/npc.cpp` 87.13s), link 27.89s, object_cache_scan 3.39s, publish 5.45s. All ahead of the wait or after it; forward progress throughout.
- First divergence: the flat 60s `WAIT_ISSUE_SECONDS` (`devscripts/dev-loop.py:109-110`) is applied to game-dll-build from `game_build_started_utc` (`devscripts/dev-loop.py:3520-3536`), which counts pre-worker codegen plus manifest/source snapshot. Any healthy 642-TU build whose setup exceeds 60s files an issue and wakes a reviewer.
- Attribution defect (persists): the recorded evidence has `generation=447`, but attempt c4495d produced generation 448. `self.game_build_generation` retains the previous published generation at trip time; `source_generation=8` is correct for the request. Same lag noted on sibling issue-a59a0058df76.
- Recurrence: the brain holds 62 non-closed issues titled `Wait over budget: game-dll-build at game_build` (47 in_review, 15 new) out of 88 build-friction issues. `record_wait_incident` dedupes only by exact wait_id, and the wait_id embeds a timestamp, so every >60s occurrence creates a new issue instead of an occurrence on a canonical one.

# Specification alignment

- Followed the router's issue-intake-and-triage, agent-proof-and-verification, change-to-evidence-latency, and agent-coordination guidance.
- The observed behavior matches `docs/specs/dev-loop/change-to-evidence-latency.md:122-127`, which mandates one issue and one wake per over-budget wait occurrence. `docs/specs/dev-loop/agent-build-queue.md:314` still lists the default wait budget as an open review question.

# Exact implementation changes

- None to source. Append-only issue review (id 110, verdict `not-wedged-policy-miscalibrated`), `related_build`/`related_run` on issue-0968dd48e9d8, and one occurrence-evidence suggestion on owner issue-136d4553ff92.

# Validation

- Build status: no build requested; no compiler, module worker, or second dev-loop was invoked.
- Evidence read: `.dev/build-timings/game-dll-20261122T034509935980Z-c4495d.json`, dev-loop journal over_budget/reviewer-wake/game-build-stage/game-generation-ready events, 181-sample `.dev/build-timings` corpus, `devscripts/build_friction.py`, `devscripts/dev-loop.py`.
- Review artifact: issue-0968dd48e9d8 review id 110; state now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8.

# Measured evidence

- This attempt: total 229.247s, worker 160.163s; changed_source_preflight 87.27s, compile 121.33s, link 27.89s, object_cache_scan 3.39s, publish 5.45s. Pre-worker setup 66.17s, entirely before worker start.
- Wait class counts: 88 build-friction issues; 62 non-closed `game-dll-build at game_build` wait issues (47 in_review, 15 new).
- Build-timing corpus (n=181): p50 141.0s, p75 377.3s, p90 1004.1s, p95 1914.5s; 113/181 (62.4%) exceed 60s.

# Regression review

- No regression entry appended. The wait is a correct detection of a >60s build; the recurring cause is budget calibration plus duplicate-issue policy, not a newly broken build.

# Human acceptance

- Not applicable (build/test wait review). Whether a worker-start-anchored clock, forward-progress gating, a measured-p95 class budget, and signature-based dedupe are acceptable policy remains a human decision.

# Agent handoff context

- Owner: unassigned; systemic DLL build-stage/latency owner is `build-coordinator` on issue-136d4553ff92.
- Next exact step: define class/stage budgets from `.dev/build-timings`, start the game-dll-build wait clock at worker start or gate on absence of forward progress, dedupe wait issues by kind+stage signature, and fix active-attempt attribution of `game_build_generation`.
- Do not redo: this wait-id, wake, timing, and journal review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review.

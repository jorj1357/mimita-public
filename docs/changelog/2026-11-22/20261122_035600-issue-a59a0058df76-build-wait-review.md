# Issue-a59a0058df76 Build-Wait Review

# Task

- Task ID: build/test wait review wake-dee34142513d
- Session ID: session-74e01155613e / work-04246000b1ad
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-a59a0058df76 (game-dll-build wait over 60s at stage game_build)
- Summary: Reviewed the exact wait occurrence, its in-flight build attempt and stage durations, wait-id dedupe, the reviewer wake record, and the dev-loop journal. Attributed the occurrence to attempt `game-dll-20261122T032247328342Z-79f775` (generation 446), appended one evidence review (verdict `not-wedged-policy-miscalibrated`, review id 107), and set `related_build`. No source edits, no build, no run, no fix, and no closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T03:56:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d5

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review and build attribution were written through `agent-coord.py issue review` and `issue context`.

# Requested behavior

Review build/test wait issue issue-a59a0058df76 without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete any missing readiness fields, append review and evidence, and report a code fix separately if one is needed.

# Findings

- Wait identity: wait_id `game-dll-build:2026-11-22T03:22:47.155Z`, kind `game-dll-build`, stage `game_build`, elapsed 60.412s vs 60.0s budget, recorded 2026-11-22T03:23:50.898Z, request ids `auto-6` and `2026-11-22T03:21:19.164Z-45d0a631`.
- Issue record completeness: readiness `ready=true`, `missing=[]`; wait_id, stage, reason, elapsed, evidence, and reviewer wake present. Set `related_build` to the actual in-flight attempt for attribution.
- Journal `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` carries exactly one `dev-loop.wait.over_budget` and one `dev-loop.wait.reviewer-wake` for this wait_id; wait-id dedupe held.
- Not a hang: attempt `game-dll-20261122T032247328342Z-79f775` (source_generation 6) succeeded; generation 446 DLL published 2026-11-22T03:28:47.747Z, total 360.398s, worker 331.196s. It began 03:22:47.328Z, immediately after the prior attempt `game-dll-20261122T031729161388Z-2185e7` failed at 03:22:47.154Z on a read-only-assignment compile error (owner codex-ingame-gui-reconnect).
- Dominant phases (`.dev/build-timings/game-dll-20261122T032247328342Z-79f775.json`): changed_source_preflight 74.96s, compile 110.79s, link 126.98s (gnu_linker 126.64s), publish 9.68s, object_cache_scan 6.58s. At the 60.412s deadline it was inside the 74.96s preflight, with compile and link still ahead.
- First divergence: the flat 60s `WAIT_ISSUE_SECONDS` (`devscripts/dev-loop.py:109-110`) is below normal whole-game DLL build cost; `check_wait_budget` (`devscripts/dev-loop.py:3520-3536`) measures age from `game_build_started_utc`, so any healthy build over 60s files an issue. Corpus `.dev/build-timings/game-dll-*.json` (n=179): p50 141.0s, p75 377.5s, p90 1004.1s; 112/179 (62.6%) exceed 60s.
- Attribution defect (persists): the recorded evidence has `generation=444` and `build_id=2026-11-22T02:37:06.972018Z`, but the in-flight attempt was generation 446 (`game-dll-20261122T032247328342Z-79f775`). `self.game_build_generation` lags because it retains the previous failed attempt's generation at trip time; `source_generation=6` is correct.
- Recurrence: the brain holds 62 non-closed issues titled `Wait over budget: game-dll-build at game_build` (44 in_review, 18 new) out of 87 build-friction issues. `record_wait_incident` (`devscripts/build_friction.py:131-169`) dedupes only by exact wait_id, and the wait_id embeds a timestamp, so every >60s occurrence creates a new issue instead of an occurrence on a canonical one.

# Specification alignment

- Followed the router's issue-intake-and-triage, agent-proof-and-verification, change-to-evidence-latency, and agent-coordination guidance.
- The observed behavior matches `docs/specs/dev-loop/change-to-evidence-latency.md:122-127`, which mandates one issue and one wake per over-budget wait occurrence. `docs/specs/dev-loop/agent-build-queue.md:314` still lists the default wait budget as an open review question.

# Exact implementation changes

- None to source. Append-only issue review (id 107, verdict `not-wedged-policy-miscalibrated`) and `related_build=game-dll-20261122T032247328342Z-79f775` on issue-a59a0058df76.

# Validation

- Build status: no build requested; no compiler, module worker, or second dev-loop was invoked.
- Evidence read: `.dev/build-timings/game-dll-20261122T032247328342Z-79f775.json`, dev-loop journal over_budget/reviewer-wake/game-generation-ready events, 179-sample `.dev/build-timings` corpus, `devscripts/build_friction.py`, `devscripts/dev-loop.py`.
- Review artifact: issue-a59a0058df76 review id 107; state now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8.

# Measured evidence

- This attempt: total 360.398s, worker 331.196s; changed_source_preflight 74.96s, compile 110.79s, link 126.98s, publish 9.68s.
- Wait class counts: 87 build-friction issues; 62 non-closed `game-dll-build at game_build` wait issues (44 in_review, 18 new).
- Build-timing corpus (n=179): p50 141.0s, p75 377.5s, p90 1004.1s; 112 > 60s.

# Regression review

- No regression entry appended. The wait is a correct detection of a >60s build; the recurring cause is budget calibration plus duplicate-issue policy, not a newly broken build.

# Human acceptance

- Not applicable (build/test wait review). Whether a calibrated game_build budget, progress-based stall gating, and signature-based dedupe are acceptable policy remains a human decision.

# Agent handoff context

- Owner: unassigned; systemic build-stage/latency owner is `build-coordinator` on issue-136d4553ff92.
- Next exact step: define per-class/per-stage budgets from `.dev/build-timings`, dedupe wait issues by kind+stage signature, fix active-attempt attribution of `game_build_generation`, and gate issue creation on absence of forward progress.
- Do not redo: this wait-id, wake, timing, and journal review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review.

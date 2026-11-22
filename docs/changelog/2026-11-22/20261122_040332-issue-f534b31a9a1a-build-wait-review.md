# Issue-f534b31a9a1a Build-Wait Review

# Task

- Task ID: build/test wait review wake-3d8f30a1ca67
- Session ID: session-1e353688c20e / work-d797747b53ae
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-f534b31a9a1a (game-dll-build wait over 60s at stage game_build)
- Summary: Reviewed the exact wait occurrence, the in-flight build attempt and per-file/stage evidence, wait-id dedupe, the reviewer wake path including its stale-adapter retry, and the dev-loop journal. Attributed the issue to its real in-flight attempt and appended one evidence review (verdict `confirmed-wait-policy-noise`, review id 109) to issue-f534b31a9a1a. No source edits, no build, no run, no fix, and no closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T04:03:32Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d5

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review and build attribution were written through `agent-coord.py issue review` and `issue context`.

# Requested behavior

Review build/test wait issue issue-f534b31a9a1a without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete any missing readiness fields, append review and evidence, and report a code fix separately if one is needed.

# Findings

- Wait identity: wait_id `game-dll-build:2026-11-22T03:37:43.197Z`, kind `game-dll-build`, stage `game_build`, elapsed 62.01s vs 60.0s budget, request `auto-7`, recorded 2026-11-22T03:39:00.523Z.
- Issue record completeness: readiness `ready=true`, `missing=[]`; wait_id, stage, reason, elapsed, evidence, and reviewer wake present. No missing reproduction/expected/acceptance/paths/proof fields to add.
- Journal `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` line 468 carries exactly one `dev-loop.wait.over_budget` and line 467 exactly one `dev-loop.wait.reviewer-wake` for this wait_id; wait-id dedupe held.
- Not a hang: attempt `game-dll-20261122T033743408692Z-e5f86d` (source_generation 7) succeeded. It was in `source_snapshot` when the budget tripped (started 03:38:32.053, completed 03:38:51.363, 19.23s), then compiled `src/npc/npc.cpp` for 213.951s (03:39:02.004 to 03:42:36.352), compiled 11 identity wrappers (~27s each, finishing by 03:43:11), linked for 90.722s, and published generation 447 at 03:45:07.963Z with status ok, total 444.366s, compile 265.042s.
- `changed_source_preflight` 214.82s is the same `npc.cpp` compile, not duplicate work: `devscripts/game_build_worker.py:1768-1799` compiles changed TUs first, and the later scan skips them via `preflight_handled`.
- Attribution defect (persists): the wait evidence's `build_id=2026-11-22T02:37:06.972018Z` and `generation=446` describe the previously published artifact; the in-flight attempt and generation 447 appear only in the free-text `reason`. Cause is `devscripts/dev-loop.py:3532` (`self.latest_build`) and `:3533` (`self.game_build_generation`). Set `related_build` to `game-dll-20261122T033743408692Z-e5f86d`, `related_run=20261122_011031`.
- First divergence / root cause: the flat `WAIT_ISSUE_SECONDS = 60` (`devscripts/dev-loop.py:109-110`) is applied uniformly to `game-dll-build` (`dev-loop.py:3520-3536`), while normal whole-game DLL builds here cost 130-450s and a single `npc.cpp` change adds a 214s translation unit. Healthy builds therefore always trip the budget.
- Recurrence: the brain now holds 62 issues titled `Wait over budget: game-dll-build at game_build` (45 in_review after this review, 17 new) of 88 build-friction issues (was 62/87 at review time). `record_wait_incident` dedupes only by exact wait_id, and wait_id embeds a timestamp, so each >60s build mints a new issue rather than an occurrence.
- Reviewer wake delivery: wake-3d8f30a1ca67 queued 03:38:47.823Z, codex adapter launched 03:39:00.184Z, `wake.retry` (stale launched request recovered by scheduler) 03:40:47.622Z, re-claimed by opencode 03:46:17.216Z and launched 03:46:17.278Z. Detection/queueing were prompt; adapter delivery lagged queueing by 7m29s.
- Fix status: a per-kind budget / no-progress predicate plus active-attempt attribution is already recorded on canonical owner issue-136d4553ff92 at 2026-11-22T03:46:01Z, so no duplicate suggestion was appended; human approval is still required before code change.

# Specification alignment

- Followed the router's issue-intake-and-triage, agent-proof-and-verification, change-to-evidence-latency, and agent-coordination guidance.
- Treated wait detection, build identity, wake delivery, and behavior as separate claims; no behavior completion was claimed.

# Exact implementation changes

- None to source. Append-only issue review (id 109, verdict `confirmed-wait-policy-noise`) on issue-f534b31a9a1a, plus `related_build`/`related_run` attribution to the actual attempt and run.

# Validation

- Build status: no build requested; no compiler, module worker, second dev-loop, or test run was invoked.
- Evidence read: `.dev/build-timings/game-dll-20261122T033743408692Z-e5f86d.json` and `.jsonl`, dev-loop `events.jsonl` wait/stage/file/result events, `.dev/agent-wake/events.jsonl` wake lifecycle for wake-3d8f30a1ca67, `devscripts/dev-loop.py`, `devscripts/game_build_worker.py`, and the issue/brain records.
- Review artifact: issue-f534b31a9a1a review id 109; state now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8.

# Measured evidence

- This attempt: total 444.366s, worker 367.171s, compile 265.042s, link 90.722s, 12 compiled / 630 skipped; `npc.cpp` alone 213.951s for a 206,006-byte / 3,931-line unit.
- Wait class counts: 88 build-friction issues; 62 `game-dll-build at game_build` wait issues (45 in_review, 17 new).
- Reviewer wake: queued 03:38:47.823Z, delivered via opencode 03:46:17.278Z (7m29s), after a stale codex-adapter launch was recovered.

# Regression review

- No regression entry appended. The wait is a correct detection of a >60s build; the recurring cause is budget calibration plus duplicate-issue policy, not a newly broken build.

# Human acceptance

- Not applicable (build/test wait review). Whether a calibrated game_build budget, progress-based detection, and signature-based dedupe are acceptable policy remains a human decision.

# Agent handoff context

- Owner: unassigned; systemic build-stage/latency owner is `cpp-latency-pressure-profile` on issue-136d4553ff92; wake-delivery reliability relates to `build-runtime-subagents`.
- Next exact step: approve and implement per-class/per-stage wait budgets or a no-progress predicate, dedupe wait issues by kind+stage signature, and fix active-attempt attribution in wait evidence.
- Do not redo: this wait-id, wake, timing, and journal review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review.

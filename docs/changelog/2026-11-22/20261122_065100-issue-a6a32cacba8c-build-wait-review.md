# Issue-a6a32cacba8c Build-Wait Review

# Task

- Task ID: build/test wait review wake-ee339c68b652
- Session ID: session-57965cc25be2 / work-25da5647633c
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-a6a32cacba8c (game-dll-build wait over 60s at stage game_build, elapsed 60.275s)
- Summary: Reviewed the exact wait occurrence, the in-flight attempt and its stage timeline, wait-id dedupe, reviewer wake record, and build attribution. Appended one evidence review (review id 138, verdict `confirmed_legitimate_wait_succeeded; duplicate_issue_occurrence=true; code_fix_needed=no; wake_queued_once=true`). No source edits, no build, no run, no fix, and no closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T06:51:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d5

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only issue review was written through `agent-coord.py issue review` (append-only discussion, original wait evidence preserved).

# Requested behavior

Review build/test wait issue issue-a6a32cacba8c without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete missing readiness fields, append review and evidence, and report a code fix separately if needed.

# Findings

- Wait identity: wait_id `game-dll-build:2026-11-22T06:08:02.690Z`, kind `game-dll-build`, stage `game_build`, elapsed 60.275s vs 60.0s budget, recorded 06:09:03.004Z, request_ids `[auto-6]`, source_generation 6.
- Issue record completeness: readiness `ready=true`, `missing=[]`, score 8/8. wait_id, stage, reason, elapsed, source_generation, started_utc, and reviewer wake present. No missing reproduction/expected/acceptance/paths/proof fields.
- In-flight attempt: `game-dll-20261122T060802960932Z-cbaf8e` (started 06:08:02.961Z, 271ms after the wait start), generation 464, revision 7181e1d6, changed path `src/npc/npc-navigator.cpp`, build note f1db1c02f256. It SUCCEEDED: completed 06:10:05.908Z, total 122.937s, worker 87.274s. Not wedged and not a hang.
- Timeline: `changed_source_preflight` 61.469s (includes src/npc/npc-navigator.cpp compile 59.465s), link 16.619s, object_cache_scan 4.381s, compile 1.346s, publish 0.545s. The single changed translation unit alone accounts for ~59.5s of compile; a one-file change is intrinsically at/over the 60s budget.
- First divergence: the flat 60s `WAIT_ISSUE_SECONDS` (`devscripts/dev-loop.py:109`) is below normal whole-game DLL cost. `check_wait_budget` (`devscripts/dev-loop.py:3520-3536`) ages from `game_build_started_utc` and keys wait_id on that timestamp, so any healthy build over 60s files a new issue. This is a true-positive detection, not a false alarm.
- Attribution defect (persists): recorded evidence carries `generation=463` and `build_id=2026-11-22T02:37:06.972018Z` (previously published artifact), while the in-flight attempt was generation 464. `self.game_build_generation` advances only on completion (`devscripts/dev-loop.py:2967`) and `build_id` is the last EXE artifact, so neither identifies the active game-DLL attempt. `source_generation=6` is correct. Same off-by-one observed in issue-c481f05923e4 (gen 466 vs attempt 467) and issue-86b3b2ce1e73 (gen 467 vs attempt 468); already reviewed on issue-6b55a3773999.
- Recurrence: per-occurrence issue creation follows the current spec, but the brain already holds many duplicate-title `Wait over budget: game-dll-build at game_build` issues. This occurrence is duplicate-of-signature noise, not a new failure mode.
- Reviewer wake delivery: wake-ee339c68b652 created 06:09:04.753Z, claimed/launched 06:30:50.512Z/06:30:50.582Z (about 21.8 min late, attempts 2). Detection and queueing were correct; delivery lagged.

# Specification alignment

- Followed the router's issue-intake-and-triage, agent-proof-and-verification, change-to-evidence-latency, and agent-coordination guidance.
- `docs/specs/dev-loop/change-to-evidence-latency.md:122-127` mandates one issue plus one reviewer wake per exact wait occurrence and names wait_id as the dedupe key, so per-occurrence issue creation is currently spec-conforming; the noise follows from the uncalibrated 60s budget and lack of a signature-level canonical issue.
- Treated wait detection, build identity, wake delivery, and behavior as separate claims; no behavior completion claimed.

# Exact implementation changes

- None to source. Append-only issue review (id 138) on issue-a6a32cacba8c.

# Validation

- Build status: no build requested; no compiler, build.py, module worker, or second dev-loop was invoked.
- Evidence read: `.dev/build-timings/game-dll-20261122T060802960932Z-cbaf8e.json` and `.jsonl`, sibling attempts `...T060035...-4e8b8d`, `...T060441...-574f91`, `...T062119...-8db74c`, `.dev/agent-wake/requests/wake-ee339c68b652.json`, `devscripts/dev-loop.py`, `devscripts/build_friction.py`, and the `.dev/build-timings` corpus.
- Review artifact: issue-a6a32cacba8c review id 138; state now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8.

# Measured evidence

- This attempt: total 122.937s, worker 87.274s; changed_source_preflight 61.469s, link 16.619s, object_cache_scan 4.381s, compile 1.346s, publish 0.545s; slowest compile src/npc/npc-navigator.cpp 59.465s.
- Comparison: gen463/source4 total 245.06s, gen463/source5 total 32.96s, gen465/source7 total 121.59s.
- Timing corpus (n=209): p50 123.0s, p75 341.2s, p90 903.4s, p95 1748.2s; 132/209 (63.2%) >= 60s.
- Wake created 06:09:04.753Z, launched 06:30:50.582Z (~21.8 min).

# Regression review

- No regression entry appended. The wait is a correct detection of a >60s build; the recurring cause is budget calibration plus per-occurrence issue policy, not a newly broken build.

# Human acceptance

- Not applicable (build/test wait review). Whether to calibrate a per-stage game_build budget and add signature-based canonical accumulation is a human decision.

# Agent handoff context

- Owner: unassigned; systemic build-stage/latency owner is `build-coordinator` on issue-136d4553ff92; avoidable-wait program is issue-e044a931c54b; wake-delivery latency relates to `build-runtime-subagents`.
- Next exact step: define per-stage budgets from `.dev/build-timings`, accumulate repeated same-stage waits on a canonical signature issue, and fix `game_build_generation`/`build_id` attribution to the active attempt.
- Do not redo: this wait-id, wake, timing, and journal review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review.

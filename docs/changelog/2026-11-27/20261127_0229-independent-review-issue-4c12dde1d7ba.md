# issue-4c12dde1d7ba Independent Issue Review

# Task

- Task ID: wake wake-3f30ae257296, issue issue-4c12dde1d7ba (Wait over budget: game-dll-build at game_build)
- Owner: unowned build-friction record; wait-instrument umbrella is issue-e044a931c54b (owner build-runtime-subagents)
- Focus: bounded independent review of the 2026-11-21 wait occurrence plus one development-process review of the evidence used
- Summary: Read-only review. Appended one issue review (APPEND-ONLY, prior text preserved), one process proposal to issue-e044a931c54b, one build-resource gap note to issue-136d4553ff92, and one issue-linked question to build-runtime-subagents. No source edit, no build/test, no compiler, no second dev-loop, no fix claimed, nothing closed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-27T02:29Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted edits. This session edited no source and reverted nothing.
- Other agents already worked this wait class: build-wait-reviewer reviewed issue-4c12dde1d7ba on 2026-11-21; build-wait-reviewer also reviewed newer occurrences (issue-f6177f624ba9, issue-dea21a9c722d, issue-e99701641a1c, issue-5d8ebe281ce6); map-authored-parity scheduled this review.
- Files created by this session: this changelog plus append-only records written through `agent-coord.py issue review / issue suggest / ask` into docs/issues markdown and the coordination DB.

# Requested behavior

- Review the issue brief/readiness, proof status, linked artifacts, and latest relevant journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and a next discriminating check; then append one specific process proposal with a measurable target and smallest experiment, or state none found.

# Findings

- Readiness ready=true, score 8/8, missing=[]; state in_review. Proof expectation is the dev-loop wait event + issue record + reviewer wake; this is a build-friction instrument, not a gameplay/runtime behavior claim.
- Root cause unchanged and reconfirmed: a single flat `WAIT_ISSUE_SECONDS` (default 60) applied to game_build by total elapsed in `check_wait_budget`, with no progress/stall test. The spec (`docs/specs/dev-loop/change-to-evidence-latency.md:122-127`) mandates one issue per exact wait occurrence with wait_id dedupe, so the budget/rule change is a spec decision, not a code regression.
- Fresh recurrence: 10 distinct build-friction issues auto-created 2026-11-27 00:21:50–01:46:00Z (84 min), all at 60.04–62.34s: issue-ee02d14f4ccd, issue-2aa79d9fe477, issue-551c405dc80c (exe), issue-51b425637ea9, issue-a9f9f82c92f7, issue-a996e6c1bfc2, issue-dea21a9c722d, issue-e99701641a1c, issue-5d8ebe281ce6, issue-f6177f624ba9.
- The flagged builds were progressing, not stalled: total 90.9s, 112.0s, and 1,015.8s on the retained attempts; the 1,015.8s attempt genuinely failed link (separate owner).
- Baseline: of 17 game-dll attempts started 2026-11-27, 8 succeeded at 38.1–292.9s (median ~171s) and 9 failed at 33.9–1,015.8s; 6 of 8 successes and 5 of 9 failures exceeded 60s. The timer cannot distinguish healthy from failing builds.
- Partial improvement (contrary evidence): newer daemon run 20261127_014331 logs generation=750 and a clean `[GAME BUILD] revision=...` reason; older run 20261127_001718 still logged generation=null and a truncated-JSON reason. Retained journals now contain over_budget/reviewer-wake events (they did not at the 2026-11-21 review).
- Wake for this occurrence created once (wake-8360523c37ab) but ends status=failed despite review id 48 being appended; wake-completion status is unreliable as a success signal.
- Acceptance "exact build/run IDs" is only partly met: this issue's evidence has build_id and source_generation but generation=null and no run_id; build_id is the last published artifact, not the attempt.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: coordination begin/sync/observe, build evidence kept separate from runtime proof, no source/build change, no duplicate issue created, resource-gap unknown treated as unknown not zero.
- Used only read-only journal/timing inspection and agent-coord writes; no compiler, module worker, `build.py`, or second dev-loop.

# Exact implementation changes

- None to source or build. Append-only issue review (id 514) + process proposal + resource-gap note + one issue-linked ask + this changelog.

# Validation

- Commands used: `agent-coord.py begin/sync/observe/proof status/issue show/issue review/issue suggest/ask`, read-only scan of `logs/2026-11-27/**/dev-loop/events.jsonl`, and reads of `.dev/build-timings/game-dll-20261127*.json` and `.dev/agent-wake/requests/wake-8360523c37ab.json`.
- Build status: no build requested; no compiler, worker, or dev-loop build request invoked.

# Measured evidence

- 7 retained `dev-loop.wait.over_budget` events 2026-11-27 00:22:08–01:46:16Z (6 game-dll-build, 1 exe-build), elapsed 60.04–62.34s, each creating an issue; plus 3 wakes for other game-dll-build wait issues.
- 2026-11-27 game-dll attempts: 8 successes (38.1, 51.4, 109.3, 166.0, 173.8, 186.7, 190.6, 292.9s) and 9 failures (33.9–1,015.8s).
- build_resource_gaps for 14ccc6, e552d5, a33f5a: captured but missing matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual/gameplay walk-through: not applicable (build-friction policy).
- Still unverified: which daemon owns the checkout lock in the 2026-11-27 window; a progress-aware/per-class budget replay; wake-completion status correctness.

# Agent handoff context

- Process proposal filed on issue-e044a931c54b task wait-instrument: replay the 7 retained waits + the 1,015.8s failure against a progress-aware rule (N=120s no stage/timeline advance) and a measured per-class budget; success = 0 wakes on the 8 successes while still surfacing the 1,015.8s failure.
- Resource-gap note filed once on issue-136d4553ff92 (build-stage observability umbrella).
- Changing WAIT_ISSUE_SECONDS or the spec exact-wait-id rule needs human approval; do not implement from this review.

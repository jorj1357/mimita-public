# issue-e7a231054fb4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T19:13:48.684Z — build-wait-reviewer

related_build: 2026-11-22T17:39:49.763492Z
related_run: 20261122_174002

## Review (confirmed)

### 2026-11-22T19:25:30.066Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: confirmed, no code change needed for this occurrence.

Occurrence (direct observation, run 20261122_174002, journal logs/2026-11-22/20261122_174002/dev-loop/events.jsonl):
- wait_id game-dll-build:2026-11-22T17:40:44.184Z, stage game_build, budget 60.0s, elapsed 62.054s (over by 2.054s).
- Issue opened at 17:41:46.409Z (coord.issue.opened, pid 25564) with build_id 2026-11-22T17:39:49.763492Z, source_generation 6, revision ed6094681c3a...
- Wake queued exactly once at 17:41:48.737Z (wake.queued wake-9c372c52c7e1) and launched (wake.claimed/launched). Acceptance for the record/wake fields is met; no duplicate issue exists for this wait_id.

Build reality (why it went over):
- dev-loop.game-build.started 17:40:53.404Z attempt game-dll-20261122T174044377377Z-410480 (changed path src/npc/npc.cpp, note ac94e40208d3).
- dev-loop.game-build.result 17:46:00.145Z status ok, generation 507, compile_seconds 233.054, link_seconds 62.352, total_seconds 315.698.
- Wait clock starts ~9.2s before the build attempt actually started, so elapsed includes pre-build queue time; the build itself then ran ~5.3 min, far over 60s.

First divergence / secondary defect:
- Journal shows dev-loop.wait.issue-failed at 17:42:18.883Z for this wait_id with error: agent-wake.py run --once timed out after 30 seconds, reason "could not record over-budget wait". This is a false-negative diagnostic: the issue and wake were already created (17:41:46 / 17:41:48). Cause: the dev-loop synchronously waits on a one-shot wake runner that returns only after the launched adapter exits, so a long-lived adapter trips the 30s timeout and logs failure even though recording succeeded.
- Consequence: repeated over-budget waits have produced many sibling build-friction issues (e.g. issue-d668de6bfc5e, issue-f6c050fa46bc, issue-9202020e4618) with the same title. Each carries a distinct wait_id, so this is threshold noise rather than literal duplication of one wait, but it matches the existing root cause recorded in issue-3a39801d6420: the flat 60s total-elapsed threshold is shorter than a normal incremental game-DLL build, and the failure path is misleading.

Inference vs unknown:
- Direct: timings, ids, wake queued once, issue-failed event.
- Inference: the 30s timeout is the synchronous one-shot runner, not issue creation.
- Unknown: whether the dev-loop retries recording after a timeout (it did not add a second over_budget event for this wait_id in this journal).

No source edit made; no file lease claimed. Behavior issue remains open pending a policy fix (budget/stage-aware threshold and non-blocking wake dispatch), which should be claimed as a separate code task.

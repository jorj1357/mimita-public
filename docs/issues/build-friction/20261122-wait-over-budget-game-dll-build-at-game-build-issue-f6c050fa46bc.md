# issue-f6c050fa46bc: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass)

### 2026-11-22T19:29:12.028Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer (wake-57d4bd9d78f1).

Verdict: pass on issue creation and per-wait-id dedupe; the wait was real in-progress build work, not a hang. Limitations: the dev-loop journal has no `dev-loop.wait.over_budget` or `dev-loop.wait.reviewer-wake` event for this wait_id, the immediate wake launch timed out, and the 60s budget is far below normal whole-game DLL build time.

EVIDENCE
Journal: logs/2026-11-22/20261122_174002/dev-loop/events.jsonl
- L73 18:13:53.659Z hot.build.plan generation 8 (changed src/avatar/cosmetic-system.cpp and .h)
- L84 18:14:17.268Z dev-loop.game-build.started source_generation 8, revision dfc38c43...
- L85 18:16:05.682Z dev-loop.wait.issue-failed wait_id game-dll-build:2026-11-22T18:13:53.662Z, error: agent-wake.py run --once timed out after 30 seconds
- L94 18:19:49.653Z dev-loop.game-build.result status ok generation 509
- L95 18:19:54.420Z dev-loop.game-generation.ready generation 509
Build timing: .dev/build-timings/game-dll-20261122T181354741248Z-395888.json
- success true, total_seconds 358.851, compile_max 177.179s (src/avatar/cosmetic-system.cpp), link 61.869s
So the 97.348s elapsed at issue creation was normal in-progress compilation of the changed TU, not a stalled build.
Wake queue: .dev/agent-wake/requests/wake-57d4bd9d78f1.json
- created 18:15:33.447Z for this issue, exactly one request, attempts 2, claimed and launched 18:35:55Z
Dedupe: rg over this journal shows 4 `dev-loop.wait.over_budget` and 4 `dev-loop.wait.reviewer-wake` events, but 3 `dev-loop.wait.issue-failed` events (17:40:44.184Z, 18:13:53.662Z, 18:19:58.794Z), all with the same 30s agent-wake timeout. For those three the durable over_budget/reviewer-wake event is missing even though the issues and queued wakes exist.

ACCEPTANCE CHECK
- wait_id, stage, reason, elapsed, and evidence are present.
- The `build_id` field is the EXE build id (2026-11-22T17:39:49.763492Z), not the game-DLL attempt id (game-dll-20261122T181354741248Z-395888). The game-DLL attempt id is present only indirectly via the journal/timing artifact.
- Exactly one reviewer wake request was queued for this issue (dedupe holds), but it was delivered about 20 minutes late after the first launch attempt timed out.
- The event name `dev-loop.wait.issue-failed` is misleading: the issue was created; only the immediate wake launch and the over_budget/reviewer-wake journal records failed.

RECURRENCE
The same 60s wall-clock budget is below typical game-DLL build time (this attempt 358.9s) and produced separate issues at 18:05 (issue-d668de6bfc5e), 18:13 (this issue), 18:23 (issue-e7b2635a5d05), 18:34 (issue-9202020e4618), plus many earlier ones. Per-occurrence issue creation is by design, but the noise indicates the budget does not fit this workload.

RECOMMENDED FIX (not applied by reviewer; report only)
1. Do not run the wake subprocess synchronously inside DevLoop.check_wait_budget. If immediate launch is retained, catch subprocess.TimeoutExpired in build_friction.wake_reviewer and return queued+launch_timeout instead of raising, and record dev-loop.wait.over_budget before launching so the journal is complete even when launch fails. See devscripts/build_friction.py:243-246 and devscripts/dev-loop.py:3548-3572.
2. Record the game-DLL build_attempt_id in the wait evidence, or rename the misleading build_id field.
3. Raise or make configurable the game-DLL wait budget, or base it on build progress (last stage/completed TU) rather than pure wall clock, so a healthy multi-minute build does not file an issue on every attempt.
Overlaps existing issue-84c584957e65 (late over-budget reviewer dispatch) and issue-7d3dcfd7433b (wait-stage measurement).

Confidence: high for the wait being legitimate and for the wake-launch timeout; medium on whether the budget itself should be raised versus made progress-based.

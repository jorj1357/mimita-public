# issue-d17523339353: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-27T19:01:59.775Z — build-wait-reviewer

Review (build-wait-reviewer): occurrence verified; two follow-ups, neither a duplicate issue.

Verified in C:\mimita-v9\logs\2026-11-27\20261127_174537\dev-loop\events.jsonl:
- L56 dev-loop.wait.over_budget wait_id=game-dll-build:2026-11-27T17:56:33.046Z, kind=game-dll-build, stage=game_build, elapsed=60.912s, budget=60.0s, build_id=2026-11-27T17:45:19.700118Z, gen=799, source_generation=54, request_ids=[build-request-df640d36aadf], issue=issue-d17523339353, created=true. The wait is genuine (whole-DLL build of 646 sources at high priority), not a false positive.
- L55 dev-loop.wait.reviewer-wake for the same wait_id, queued=true.
- .dev/agent-wake/events.jsonl: wake-538c984b697a wake.queued exactly once at 17:57:36.566Z; claimed 17:57:37.445Z (codex); wake.retry 18:34:04.733Z "previous adapter process exited; trying fallback"; claimed+launched by opencode 18:42:09.856Z (pid 8496, this session).

So the static acceptance ("one issue and one queued review wake per wait_id") is met. Two problems remain:

1) Mispattributed launch result. L55's reviewer-wake for issue-d17523339353 embeds a launch_result whose prompt is for issue-6e15970df117 (exe-build:42:2026-11-27T17:27:27.229Z) / wake-ca3c45e6a7a8, not this wait. Cause: build_friction.wake_reviewer runs `agent-wake.py run --once` and records whatever that once-runner launched (build_friction.py:244-251); at 17:57:37 that once-runner also launched the older stale wake-ca3c45e6a7a8 (pid 20704, 17:57:37.535Z) alongside this wake's codex launch (pid 27756, 17:57:37.475Z). The per-wait event therefore does not prove its own reviewer wake launched.

2) Delivery latency ~44.7 min. Effective opencode launch of this wait's reviewer was 18:42:09.856Z vs queue 17:57:36.566Z, because the codex adapter process exited and the fallback retry was not claimed until 18:34-18:42. "Wakes one reviewer automatically" is met on paper but not promptly in practice.

Related fragility (same owner, not a duplicate): L160 dev-loop.wait.issue-failed for wait_id game-dll-build:2026-11-27T18:32:32.984Z reports `agent-wake.py run --once` timing out after 30 seconds while recording the wait, so that over-budget wait was never recorded as an issue. The inline 30s once-runner (dev-loop.py:3914-3921, build_friction.py:244-246) couples issue recording to wake launch latency.

Smallest next checks (not started; no source edit or build made in this review): (a) when recording reviewer-wake, verify run --once launched the requested wake id/issue and record "not launched yet" instead of another wake's result; (b) record queue->claim->launch latency on the wait event and flag >N min as its own friction; (c) record the wait issue before/independently of the wake launch so a launch timeout cannot drop the issue. Ownership left unclaimed. Fix proof expectation: a dev-loop.wait.reviewer-wake whose launch_result wake id matches that wait's queued wake, plus recorded queue-to-launch latency, on a run where an older wake is also pending.

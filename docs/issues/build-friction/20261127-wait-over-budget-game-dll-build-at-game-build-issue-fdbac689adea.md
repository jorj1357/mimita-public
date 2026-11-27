# issue-fdbac689adea: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise-and-dispatch-defect)

### 2026-11-27T19:10:49.946Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence against the pinned build attempt, the dev-loop journal, and the reviewer-wake queue. Two separate findings; neither needs a source edit in this review, and I made no edits or build/run requests.

WAIT OCCURRENCE (verified)
- wait_id: game-dll-build:2026-11-27T17:46:16.304Z
- started_utc 17:46:16.304Z; elapsed 60.1s; budget_seconds 60.0; stage game_build; kind game-dll-build
- issue evidence build_id 2026-11-27T17:45:19.700118Z, generation 799, source_generation 46, request_ids [2026-11-27T17:37:20.043Z-d1b69cb8:mode3:game]
- reason is readable, not a mid-JSON tail: "[GAME BUILD] revision=3a43ff66... sources=646 jobs=10 priority=high cache=... seeded=0" (issue-6e08d89b55e3's unreadable-reason symptom does not apply here)
- The 60s elapsed is genuine work, not a stall: attempt game-dll-20261127T174616512286Z-7470c4 had six changed TUs in compile from 17:47:00.6Z, each 388-393s wall (aim-commands 388.7s, player-avatar-animations 389.6s, server-projectiles 390.7s, simulate-tick 392.0s, vehicle-system 393.5s, engine-tick-camera 387.3s). At the 60s mark all six were still mid-compile.
- The attempt failed at 17:53:37.9Z after 440.8s in changed_source_preflight: src/engine/engine-tick-camera.cpp:1360 error 'sVehicleCamera' was not declared. That compile error is owned by the feature agent (anim-socket-crouch build note), not this wait issue.
- Postmortem resource: mean host CPU 99.97%, peak 100.0%, minimum free memory 57.8 MB. Compile-dominated build under near-saturation, so a flat 60s threshold cannot distinguish it from a hang.

FIRST DIVERGENCE (owner + line)
1) Wait policy (noise/duplication): devscripts/dev-loop.py:3885-3901 fires on wall-clock age alone with one global WAIT_ISSUE_SECONDS=60 (no stage-duration or progress check); devscripts/build_friction.py:131-169 keys uniqueness to wait_id, and wait_id embeds the per-attempt start time, so each >60s attempt mints a new issue. Current store: 254 open 'Wait over budget: game-dll-build at game_build' issues and 287 open 'Wait over budget:*' issues. This is the already-open defect in issue-9a4503f6ed68 (item 4) and issue-81d40f5f20b3; not new.
2) Reviewer dispatch + journaling (new evidence for the open dispatch defect): devscripts/build_friction.py:244-246 wraps `agent-wake.py run --once` in subprocess.run(..., timeout=30) with no TimeoutExpired guard. For this wait that call timed out: journal event dev-loop.wait.issue-failed at 17:47:55.167Z, error "Command [...agent-wake.py, run, --once] timed out after 30 seconds", reason "could not record over-budget wait". The issue WAS recorded (created 17:47:18.967Z) and the wake request WAS queued (wake-53293a27a9c5 created 17:47:24.471Z, wake.queued 17:47:24.479Z), so the reason is misleading; the timeout aborts dev-loop.py:3913-3921 before dev-loop.wait.reviewer-wake and dev-loop.wait.over_budget are written, so this occurrence has NO dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake event (only wait.issue-failed). That breaks the issue's proof expectation.
- Wake dispatch timeline (wake-53293a27a9c5.json + .dev/agent-wake/events.jsonl): queued 17:47:24.479Z (exactly once); codex claimed/launched 17:57:37.324/17:57:37.404Z pid 30232 (drained ~10m later by the next over-budget wait, not by its own run --once); wake.retry to opencode 18:34:04.697Z "previous adapter process exited; trying fallback"; opencode claimed/launched 18:42:09.705/18:42:09.779Z pid 31952. Queue-to-final-launch 3285.3s (~54m45s) with two wake.launched events for one wait. This is the pattern in issue-84c584957e65; it is not a stalled build.
- The same 30s timeout recurred 4x in this run (17:47:55, 18:18:16, 18:22:42, 18:34:04), all "agent-wake.py run --once timed out after 30 seconds".

ASSESSMENT / NEXT (no edit, no build, no run)
- Not a hang. The underlying build did real work and failed on a genuine compile error; the wait is wait-policy noise plus a dispatch/journaling defect.
- Suggested smallest safe owner fix (report only, not implemented here): catch subprocess.TimeoutExpired around build_friction.py:244-246 and still write dev-loop.wait.reviewer-wake/over_budget (or make the wake launch asynchronous), so a slow adapter cannot lose the durable wait events or mislabel the failure. Do not change the 60s policy here; that is issue-9a4503f6ed68 / issue-136d4553ff92.
- Cross-links: issue-9a4503f6ed68 (canonical wait feature, flat budget + per-attempt duplication), issue-81d40f5f20b3 (prior noise review), issue-84c584957e65 (late reviewer dispatch), issue-6e2d7b91e7d9 (other cause of missing wait journal events), issue-136d4553ff92 (build latency/header fanout).
- Evidence confidence: high (pinned journal lines, build-timing postmortem, wake request + wake events). No files edited; no build/test requested.

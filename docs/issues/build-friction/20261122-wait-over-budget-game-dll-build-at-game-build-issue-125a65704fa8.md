# issue-125a65704fa8: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-benign-wait)

### 2026-11-22T18:00:30.612Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded review; no source edits, no compiler/build, no second dev-loop, no runtime started.

Occurrence verified from root dev-loop journal `logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl`:
- line 1099 `dev-loop.wait.over_budget`: wait_id `game-dll-build:2026-11-22T16:32:41.717Z`, kind game-dll-build, stage game_build, elapsed 63.177s, budget 60s, build_attempt `game-dll-20261122T163256885716Z-84a726`, source_generation 9, request_ids ["auto-9"], last published generation 502.
- Trigger reason: `[GAME_STAGE] stage=manifest_snapshot state=started time_utc=2026-11-22T16:33:13.404Z elapsed_ms=16518.193`.
- line 1098 `dev-loop.wait.reviewer-wake`: one wake for this issue/wait. `.dev/agent-wake/events.jsonl` shows exactly one `wake.queued` (wake-f2fe02f8fdc3 at 16:34:13.530Z); the first codex launch at 16:34:15.944Z did not complete, the wake was retried and launched on opencode at 17:23:12.192Z (this activation). Wake delivery is confirmed; the first adapter attempt failed.

Build timeline from `.dev/build-timings/game-dll-20261122T163256885716Z-84a726.jsonl`:
- build.started 16:32:56.886Z (15.169s after the wait clock started).
- manifest_snapshot stage 16:33:13.404Z -> 16:34:32.300Z, duration 61.520s; subphases progressed normally (manifest_inputs_load 1.13s, owner_resolver 9.62s, source_ownership_and_digest 13.85s, header_digest 12.19s, build_manifest 43.95s total, source_mirror_update 10.57s).
- manifest_validation 16:34:48.551Z -> 16:35:05.830Z, 2.18s.
No stalled, crashed, or silent stage. Confidence: high that this is a genuine but benign long build, not a hang.

First divergence / avoidable portion: the wait clock starts at `game_build_started_utc` when the worker thread starts (devscripts/dev-loop.py:2882-2883), while the timed build begins at `build.started` 15.169s later. At the 60s crossing (~16:33:44.9) the actual build had executed ~48.0s and the active `manifest_snapshot` stage was ~31.5s old. Neither build execution nor the active stage had reached the 60s budget. `check_wait_budget` also hardcodes stage "game_build" (devscripts/dev-loop.py:3531-3547) although the active stage was manifest_snapshot. This clock inflation and coarse stage label are the likely reason one issue is filed per long build.

Recurrence in this same run: issue-edbac5d40c37 (16:17), issue-21dba28546bc (16:18), issue-cfe902b4b659 (16:24), this issue (16:32), issue-34993e327e19 (17:17), issue-e45921a75b6e (17:21); earlier issue-3a39801d6420 and issue-d7e0bdaf919a. I did not create another duplicate. Related tracked work: issue-7d3dcfd7433b (measure every dev-loop wait stage) and issue-6e08d89b55e3 (wait reason stores raw output); total DLL stage speed is issue-136d4553ff92.

Verdict: confirmed-benign-wait. Acceptance fields are present and ready; the issue/wake records exist. Recommendation for an owner (not claimed or implemented here): start the game-DLL wait clock at build.started (or exclude pre-build setup) and report the active sub-stage so the 60s budget measures actual build execution.

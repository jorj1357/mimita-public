# game-dll-build wait review (issue-ee437972915e)

Read-only bounded review by `build-wait-reviewer` (wake-742bed53da17), 2026-11-24T17:43Z. No source edit, build, runtime change, process control, or fix claim.

## Occurrence

Wait `game-dll-build:2026-11-24T16:47:16.707Z` fired at 60.076 s against attempt `game-dll-20261124T164717275109Z-44a127` (generation 595, source_generation 12, success at 17:00:47.276Z; total 810.0 s, worker 760.391 s, compile 743.031 s, link 9.891 s; 276 compiled / 369 skipped of 644 sources). Journal `logs/2026-11-24/20261124_150432/dev-loop/events.jsonl`; summary `.dev/build-timings/game-dll-20261124T164717275109Z-44a127.json`.

The wait was legitimate but early: `src/combat/weapon-types.h` (267 dependents) plus `src/effects/effect-part.h` (56) forced 276 TUs; slowest TU `src/network/server-gamemode.cpp` 84.94 s. The flat 60 s budget fired at ~7.4 % of build wall time and cannot distinguish a header-fanout rebuild from a hang.

## Acceptance findings

- Queue contract held: exactly one `wake-742bed53da17` queued at 16:48:22.319Z for this wait_id.
- Journal and identity failed. The issue evidence carries `build_id=2026-11-24T07:48:35.684015Z` (previous EXE build) and `generation=null`; the in-flight attempt id is only in the journal. `dev-loop.wait.over_budget` and `dev-loop.wait.reviewer-wake` are absent; the journal instead has `dev-loop.wait.issue-failed` at 16:48:52.667Z because `agent-wake.py run --once` hit its 30 s subprocess timeout.
- Dispatch failed: codex launched 17:04:41.005Z, retry 17:08:26.715Z, opencode launched 17:08:29.237Z, attempts=2. Queue-to-final-launch 966.9 s (>5 min target). Only the opencode attempt produced `agent-coord` activity (begin 17:11:01.538Z).

First divergence: `devscripts/build_friction.py:237-246` blocks on `agent-wake.py run --once` with `timeout=30`; `TimeoutExpired` escapes `wake_reviewer` after `devscripts/dev-loop.py:3909` has already added the wait_id to `reported_wait_ids`. Result: the wait is logged as failed, the success events are never written, and the claimed wake waits for the scheduler. Reported, not fixed.

## Resource observation

`.dev/build-timings/game-dll-20261124T164717275109Z-44a127.resource.json`: 273 samples @1 s, mean host CPU 99.344 %, peak 100 %, minimum available memory 7,823,360 B (~7.5 MiB), runtime alignment unavailable. Direct build-phase pressure; not causal for the wait policy. gap_key `build-resource-sampling:game-dll-20261124T164717275109Z-44a127` recorded once on issue-136d4553ff92 with an issue-linked question to build-coordinator. The two later gap keys (968a7e, 8723b2) were already being tracked on issue-136d4553ff92 and issue-ed3ac775283d, so no duplicate append.

Dispatch recurrence appended to issue-84c584957e65; build-pressure occurrence appended to issue-2a5c5da53565. No duplicate wait issue, no close, no runtime proof claim.

## Next discriminating checks

- Measure attempts and queue-to-final-launch on the next 5 over-budget waits after the launch-timeout fix; target attempts=1 and <=300 s.
- Join each wait evidence `build_id`/`generation` to its `dev-loop.game-build.started` attempt id; target no stale EXE id and a real attempt id.
- Compare a matched idle-host build of the same header-fanout change against this 810 s attempt to separate intrinsic cost from host saturation.

# issue-ee437972915e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed_legitimate_build_acceptance_fail_journal_identity_dispatch)

### 2026-11-24T17:40:50.082Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait `game-dll-build:2026-11-24T16:47:16.707Z` (60.076 s at the flat 60 s budget, stage `game_build`) against attempt `game-dll-20261124T164717275109Z-44a127` (generation 595, source_generation 12, success; total 810.0 s, worker 760.391 s, compile 743.031 s, link 9.891 s, publish 0.578 s; 276 compiled / 369 skipped of 644 sources). Journal: `logs/2026-11-24/20261124_150432/dev-loop/events.jsonl`; summary `.dev/build-timings/game-dll-20261124T164717275109Z-44a127.json`.

Direct: the build was real and succeeded at `2026-11-24T17:00:47.276Z`, so the wait fired at ~7.4 % of build wall time. The trigger was a high-fan-out change: `src/combat/weapon-types.h` (267 dependents) plus `src/effects/effect-part.h` (56) forced 276 TUs; slowest TU `src/network/server-gamemode.cpp` 84.94 s. A flat 60 s budget cannot distinguish this legitimate 13.5 min rebuild from a hang.

Acceptance: FAIL on identity and journal. The issue evidence carries `build_id=2026-11-24T07:48:35.684015Z` (previous published EXE build) and `generation=null`; the waited-on game-DLL attempt id `game-dll-20261124T164717275109Z-44a127` appears only in the journal, so "build/run IDs" is not satisfied by the record. `dev-loop.wait.over_budget` and `dev-loop.wait.reviewer-wake` are absent for this wait_id; the journal instead has one `dev-loop.wait.issue-failed` at `16:48:52.667Z`: `agent-wake.py run --once` timed out after 30 s.

Wake: queued exactly once as `wake-742bed53da17` at `16:48:22.319Z` (queue contract holds). Dispatch failed: codex claimed/launched `17:04:40.975Z`/`17:04:41.005Z`; `wake.retry` to opencode `17:08:26.715Z`; opencode claimed/launched `17:08:29.198Z`/`17:08:29.237Z`; attempts=2, queue-to-final-launch 966.9 s (~16 m 7 s), violating the <=5 min/one-launch target. Only the opencode attempt produced `agent-coord` activity (begin `17:11:01.538Z`); the codex attempt spent ~4 min then was replaced.

First divergence: `devscripts/build_friction.py:237-246` runs `agent-wake.py run --once` with `timeout=30`; `TimeoutExpired` escapes `wake_reviewer`, and `devscripts/dev-loop.py:3909-3930` has already added the wait_id to `reported_wait_ids` before the call. The loop therefore logs the whole wait as `issue-failed`, never records `over_budget`/`reviewer-wake`, and leaves the claimed wake for the scheduler. Smallest safe fix: launch detached (Popen/no-wait) or treat a launch timeout as queued-but-unconfirmed; always record `over_budget` before launching. Reported only; no code edit, no claim.

Resource (observation, not cause): `.resource.json` captured 273 samples @1 s, mean host CPU 99.344 %, peak 100 %, minimum available memory 7,823,360 B (~7.5 MiB); runtime alignment unavailable (`fresh_server_and_client_journal_evidence_not_matched`), so live mode/workload is unknown. gap_key `build-resource-sampling:game-dll-20261124T164717275109Z-44a127` appended to issue-136d4553ff92. Dispatch recurrence appended to issue-84c584957e65; pressure occurrence appended to issue-2a5c5da53565.

Not done: no closed issue, no runtime proof claim, no duplicate wait issue. Recommendation: make the wait budget stage/expected-duration aware (for example suppress while compile progress is advancing, or scale by changed-fanout class) and include `build_attempt_id`/generation in wait evidence; both need owner/human decision because they change shared wait policy.

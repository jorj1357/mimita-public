# game-dll-build wait independent review (issue-0322fa45b0e8)

Read-only bounded review by `issue-review-sweeper` (wake-d4324fdcd3cf), 2026-11-24T12:56Z. No source edit, build, runtime change, process control, or fix claim.

## Original occurrence

Reviewed wait `game-dll-build:2026-11-20T20:58:50.904Z` (60.257 s at the 60 s budget check, stage `game_build`) against attempt `game-dll-20261120T205850979297Z-7cdcb2` (generation 415, source_generation 41, success, total 563.53 s, object_scan 340.257 s). Journal `.dev/build-timings/game-dll-20261120T205850979297Z-7cdcb2.json`.

Exactly one reviewer wake request (`wake-15cdbf8cf0e0`) exists for this wait_id: created 20:59:52.893Z, claimed/launched 23:44:04.061Z/109Z, attempts=2, status=failed (`adapter process exited`), updated 2026-11-21T00:13:29.066Z. Review id 29 was appended 2026-11-21T00:06:02.028Z, so the per-wait wake-queue contract held, but the reviewer activated ~2h44m after the wait and after the build succeeded.

Identity mismatch reproduced: wait evidence carries `build_id=2026-11-20T20:20:51.788061Z` and `generation=414`, while the in-flight attempt was generation 415. The `build_id` field names the previous published build, not the waited-on attempt, so the acceptance field "build/run IDs" is only weakly satisfied. This identity root remains tracked on umbrella issue-9a4503f6ed68.

## Recurrence (new evidence)

The current build-friction listing returns 100 issues (90 `in_review`, 8 `new`, 2 `assigned`), 91 titled `Wait over budget: game-dll-build at game_build`; the 60 s budget is still flat (`devscripts/dev-loop.py:113-115`) and dedupe is per wait_id (`devscripts/build_friction.py:131-169`).

Fresh pair: wait `game-dll-build:2026-11-24T12:22:08.237Z` (issue-3472692a57c2) carries `generation=593` / `build_id=2026-11-24T07:48:35.684015Z` while attempt `game-dll-20261124T122208532874Z-63061a` completed as generation 594 (success, total 97.566 s, compile 61.449 s, `server-projectiles.cpp` 60.521 s, link 22.383 s). Resource capture: mean host CPU 99.853 %, peak 100 %, memory used 94 %, minimum available 312,864,768 B, `matched_prebuild_resource_baseline` unavailable. The alarm fired on a real successful build; whether 97.566 s was intrinsic or contention-inflated is unknown.

Wake dispatch for that occurrence (`wake-e471a391868c`): queued 12:23:09.565Z, codex launched 12:23:21.844Z, scheduler retry 12:38:27.642Z, opencode launched 12:43:54.499Z, attempts=2. Two `wake.launched` events for one wait_id, ~1244.9 s queue-to-final-launch. This was appended as a net-new occurrence on the existing owner issue-84c584957e65 (no duplicate proposal).

## Process review

No net-new process proposal. The late/duplicate dispatch is already owned by issue-84c584957e65 (<=5 min, exactly one launch) and the budget/dedupe policy by issue-9a4503f6ed68. This sweep added the 2026-11-24T12:22 occurrence to issue-84c584957e65.

## Next discriminating checks

- Join the next wait issue's evidence `build_id`/`generation` to its `dev-loop.game-build.started` attempt id and result generation.
- Measure queue-to-final-launch and `attempts` on the next 5 over-budget waits after the adapter-preference/work-landed fix; target <=300 s, attempts=1, 0 duplicate launches per wait_id.
- Time one matched idle-host incremental build of the same one-TU change to test whether the 60 s budget is mis-calibrated for incremental builds versus merely contended.

Issue remains `in_review`; no leases held.

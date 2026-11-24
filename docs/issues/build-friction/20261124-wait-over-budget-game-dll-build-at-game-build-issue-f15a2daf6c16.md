# issue-f15a2daf6c16: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-24T19:41:40.810Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer; no source edits. Observed directly in logs/2026-11-24/20261124_170535/dev-loop/events.jsonl: wait game-dll-build:2026-11-24T19:08:17.245Z fired at 19:09:19.574Z with elapsed 60.318s > 60s budget. The record is complete (wait_id, kind=game-dll-build, stage=game_build, started_utc 19:08:17.245Z, elapsed 60.318, budget 60.0, build_id 2026-11-24T17:05:20.935679Z, generation 600, source_generation 28, request_ids [2026-11-24T19:08:17.158Z-94fd8c25:mode3:game]) and exactly one dev-loop.wait.over_budget plus one dev-loop.wait.reviewer-wake (wake-afa5d52a1438, adapter codex) exist for this wait_id; per-wait dedupe held. Acceptance is met except the reason field.

Cause of the wait (direct): the Mode 3 batch sealed at 19:08:17.158Z (-94fd8c25) included changed headers src/combat/weapon-types.h and src/vehicles/vehicle-types.h. Attempt game-dll-20261124T190817284712Z-0003b3 then ran 411.7s, compiling 266 TUs (378 cache hits) at jobs=10 while host CPU averaged 98.755 percent of 12 logical CPUs (peak 100) and available memory fell to 25,145,344 B (~24 MiB) of 15.65 GiB. The build did not merely overrun; it FAILED at compile (404.599s) with src/vehicles/vehicle-system.cpp:700 error: collisionHalf was not declared in this scope. The timing summary cache_invalidations marks vehicle-system.cpp reason object file missing, so the sealed batch captured an in-progress edit. The current file declares collisionHalf at line 699 and generation 606 built cleanly, so this specific compile break is already resolved.

Reason defect (known, not re-filed): evidence.reason is not a readable stage summary but the last 240 chars of a worker line, e.g. 9932.729,build_attempt_id:game-dll-20261124T190817284712Z-0003b3,... It comes from dev-loop.py:3112/3142 _record_game_build_output setting game_build_detail to line.rstrip()[-240:] and dev-loop.py:3892 reading it as the reason. This is the same defect as issue-6e08d89b55e3, where I appended this wait as a fresh occurrence instead of creating a duplicate issue.

Resource evidence: appended to issue-2a5c5da53565 (build-phase pressure under high CPU/low memory), cross-linked to issue-136d4553ff92 (build lifecycle) and issue-196c3045b4cb (header fan-out).

Inference and uncertainty: a header-fanout rebuild on a saturated host plausibly explains both the 60.318s wait and the 411.7s build, but this sample does not separate queue delay from compile contention; runtime_alignment is unavailable (fresh_server_and_client_journal_evidence_not_matched) and no top competing process was captured, so live-workload attribution and the cause of the ~24 MiB floor are unknown. Contrary evidence: the batch only changed 2 paths according to changed_paths, yet 266 TUs recompiled, confirming the headers rather than the source count drove the work.

Next check (owners): (1) fix reason readability under issue-6e08d89b55e3; (2) under issue-136d4553ff92, prevent a Mode 3 batch from sealing a compiler input whose object is missing and whose edit is still in progress. No new duplicate wait issue is warranted for this wait_id.

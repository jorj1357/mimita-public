# issue-a1eebc340521: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-recurring-systemic-wait)

### 2026-11-21T22:28:15.975Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded build-wait review by build-wait-reviewer (wake-6b7994156e83). Read-only: no source edited, no build/test requested by this reviewer, no fix or close claimed.

EXACT WAIT
- wait_id game-dll-build:2026-11-21T22:09:14.345Z; attempt game-dll-20261121T220914705908Z-729a42; request_ids [auto-2], source_generation 2.
- Dev-loop run journal logs/2026-11-21/20261121_215237/dev-loop/events.jsonl: dev-loop.wait.over_budget at 22:10:19.374Z (elapsed 60.195s, budget 60.0s) and dev-loop.wait.reviewer-wake at 22:10:19.373Z (wake-6b7994156e83). Exactly one issue (issue-a1eebc340521) and one reviewer wake for this wait_id: the acceptance clause is satisfied.
- The wait reason captured source_snapshot started at 22:09:58.043Z (elapsed_ms 43337). Per the same journal source_snapshot completed at 22:10:43.829Z after 45.447s. At the instant the 60s budget crossed the build was actively copying/snapshotting sources, not stalled.

ATTRIBUTION DEFECT (confirmed)
- The recorded evidence build_id is the EXE artifact 2026-11-21T02:26:50.306117Z and generation 433 (the previously published game generation). It does not carry the in-flight attempt id 729a42 nor its target generation 434. This matches the known defect at devscripts/dev-loop.py:3403-3406 (records self.latest_build / self.game_build_generation) already reported under issue-136d4553ff92. The wait record therefore cannot name the attempt that actually caused it.

ROOT CAUSE = SYSTEMIC BUDGET MISMATCH (duplicate symptom, not a distinct defect)
- Attempt 729a42 total wall time was 321.428s (started 22:09:14.705Z, completed 22:14:36.187Z). A normal whole-game DLL build is about 300-430s, so a flat WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:108-109) must fire on essentially every build attempt.
- wait_id is derived from game_build_started_utc (devscripts/dev-loop.py:3396) and devscripts/build_friction.py:143-202 opens a new issue per wait_id, so each build attempt over 60s yields a new Wait over budget: game-dll-build at game_build issue plus a reviewer wake. This issue is one member of that recurring family; the owner of the systemic analysis is issue-136d4553ff92 (build-coordinator). Recommend stage-aware or no-stage-progress-for-N-seconds budgeting, coalescing per build attempt, and carrying attempt id + target generation in wait evidence. Reported as evidence/cross-link only; no fix implemented.

ATTEMPT OUTCOME AND LINK FAILURE (transient, already resolved)
- 729a42 failed at link in 26.31s with undefined references to MapEditor::setGenerationZoneStreamRadius (map-editor-commands.cpp:921), setGenerationZoneCellSize (:926), and setGenerationZoneRepeatAxes (:937). Changed paths were src/gamemode/map-config.cpp and src/mapeditor/{map-editor.h,map-editor.cpp,map-editor-commands.cpp}, change notes 42dd838516f3 and 2c45ea6a767c from infinite-city-scale-fog-editor.
- This was a partially-applied in-flight edit, not a persistent defect. The failing snapshot was taken 22:09:58-22:10:43; those source files were subsequently modified (map-editor.h 22:15:21Z, map-editor-commands.cpp 22:15:55Z, map-editor.cpp 22:19:39Z). The next attempt game-dll-20261121T221437598388Z-a59f5e succeeded as generation 435 (426.708s, no worker_errors), and the current tree defines all three methods at map-editor.cpp:2728/2764/2803.
- Conclusion: no hung wait and no outstanding link defect from this attempt; do not file a duplicate bug. The remaining wait-friction work belongs to issue-136d4553ff92.

CONFIDENCE: high for the wait/attribution facts (journal plus per-attempt summary .dev/build-timings/game-dll-20261121T220914705908Z-729a42.json); high that the link failure was transient (gen 435 success observed).

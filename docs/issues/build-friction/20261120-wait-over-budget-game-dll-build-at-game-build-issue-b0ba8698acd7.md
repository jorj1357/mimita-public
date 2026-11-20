# issue-b0ba8698acd7: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (evidence-confirmed)

### 2026-11-20T17:20:18.693Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded wake review (wake-f62ce3ee3cb6). This wake was requested to review issue-b0ba8698acd7 and was itself an instance of issue-62ab3daa615d: the opencode session received only the first line of the prompt, so the task was reconstructed from MIMITA_WAKE_ID/MIMITA_WAKE_ISSUE and the on-disk prompt file.

Wait occurrence
- wait_id: game-dll-build:2026-11-20T16:38:24.042Z
- kind/stage: game-dll-build / game_build
- elapsed: 60.024s (budget 60.0s)
- generation: 405, source_generation: 21
- build attempt: game-dll-20261120T163826804025Z-9b84b5

Evidence (read-only, .dev/build-timings/game-dll-20261120T163826804025Z-9b84b5.json)
- success=false, failed_phase=compile, total_seconds=1466.52
- worker_error: "compile failed: 1 file(s) in 336.59s"
- compile error: src/network/server-gamemode.cpp:3949 'const struct GroundState' has no member named 'groundNormal' (also 3950-3951 and a JSON push_back at 3936)
- changed_paths for the attempt: src/actor/player-ai.cpp, src/mapeditor/map-editor-projects.cpp
- worker metrics: compile_seconds=1360.694, object_scan_seconds=500.072, dependency_check_ms=121770.77

Finding
The over-budget wait was not an idle or stale control path: the game-DLL stage legitimately ran far past 60s and then failed with a real compile error in src/network/server-gamemode.cpp:3949 (GroundState::groundNormal does not exist). The compile error is the actionable defect; the 60s wait issue is its symptom. This issue needs no separate code fix beyond dedupe/wait-policy, but the compiler failure needs its own owner/issue.

Duplication note
Adjacent wait issues cover the same build window (issue-bb273ac1386c game-dll-build 17:02:55, issue-40dfaa2a803c game-dll-build 16:21:27). Recommend recording recurrences as occurrences rather than new issues.

Confidence: high for the compile failure and wait timing; medium for the wait-policy dedupe recommendation. No source files were edited.

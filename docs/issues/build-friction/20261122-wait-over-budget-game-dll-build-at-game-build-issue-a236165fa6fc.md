# issue-a236165fa6fc: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T14:44:49.068Z — build-wait-reviewer

related_build: 2026-11-22T08:00:31.376619Z
related_run: 20261122_141131
evidence: {'wait_id': 'game-dll-build:2026-11-22T14:21:40.449Z', 'outcome': 'successful_build', 'verdict': 'not_a_stall', 'generation': 498, 'build_attempt_id': 'game-dll-20261122T142140783029Z-c00839', 'revision': '3ed4ed87436cfa1e139cf58e86087e0881007fce0cad13d40bcfc7e6e536ad9c', 'budget_seconds': 60.0, 'elapsed_seconds': 61.027, 'over_by_seconds': 1.027, 'worker_started_utc': '2026-11-22T14:21:40.449Z', 'over_budget_flagged_utc': '2026-11-22T14:22:49.281Z', 'build_result_utc': '2026-11-22T14:22:52.093Z', 'generation_ready_utc': '2026-11-22T14:22:57.757Z', 'build_total_seconds': 71.217, 'compile_seconds': 49.798, 'link_seconds': 10.385, 'snapshot_and_codegen_seconds': 9.0, 'slowest_units': [{'path': 'src/npc/npc-navigator.cpp', 'seconds': 27.272}, {'path': 'src/network/server-npcs.cpp', 'seconds': 21.732}], 'journal': 'logs/2026-11-22/20261122_141131/dev-loop/events.jsonl', 'wait_policy': 'devscripts/dev-loop.py:3486-3547 WAIT_ISSUE_SECONDS default 60 applied to total elapsed since worker start', 'root_cause': 'Flat 60s total-elapsed threshold is shorter than a normal fast incremental game-DLL build (this one took 71.2s: 49.8s compiling two TUs + 10.4s link + ~9s snapshot/codegen). The build was healthy and published generation 498 at 14:22:57, ~8s after the flag, so it went over budget only because total elapsed crossed 60s, not because of a hang or infrastructure fault.', 'secondary_defect': 'wait.reason is a truncated JSON tail of the last GAME_FILE progress line (begins with 59755.693, followed by build_attempt_id etc), not a human-readable stage/path reason; low evidence quality.', 'flood_context': "build_friction.record_wait_incident keys uniqueness on wait_id, so every fresh game-DLL build that crosses 60s opens a new issue with the same title; the board already holds dozens of identical 'Wait over budget: game-dll-build at game_build' issues."}

## Review (reviewed-no-fix)

### 2026-11-22T14:44:49.175Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed against the exact run journal (run 20261122_141131, dev-loop events.jsonl).

Finding: this wait was NOT a stall. Build attempt game-dll-20261122T142140783029Z-c00839 for generation 498 started 14:21:40.449Z, was flagged over budget at 14:22:49.281Z (elapsed 61.027s vs 60s, +1.027s), and then completed successfully: dev-loop.game-build.result status=ok at 14:22:52.093Z (total 71.217s, compile 49.798s, link 10.385s), generation 498 published at 14:22:57.757Z. Slowest units: src/npc/npc-navigator.cpp 27.272s and src/network/server-npcs.cpp 21.732s.

Root cause (high confidence): devscripts/dev-loop.py:3486-3547 flags on total elapsed since worker start using a flat WAIT_ISSUE_SECONDS default of 60. A fast incremental game-DLL build that must compile even one ~20-30s TU plus link and snapshot/codegen routinely exceeds 60s, so healthy builds produce guaranteed false positives. The 60s budget is right for exe-build-queue/pending waits but too tight for game-dll-build.

Secondary defect: the recorded wait.reason is a truncated JSON tail of the last GAME_FILE progress line (it begins with '59755.693,' then build_attempt_id/source_generation/...), not a human-readable reason, which weakens the evidence.

Flood: build_friction.record_wait_incident treats each distinct wait_id as a new issue (docstring says so explicitly), so every over-60s game-DLL build opens another identical-titled issue. The board already has dozens. The issue's own acceptance (one issue + one wake per wait occurrence) is met; the design is what needs changing.

Recommended fix (not claimed/edited here; requires separate claim + human approval):
1. Make the game-dll-build wait progress-aware instead of total-elapsed: only flag when no phase/file progress event has arrived for N seconds (e.g. 60s), or reset the timer at first compile submission. This distinguishes a slow-but-progressing build from a real hang.
2. Or raise/scale the game-dll-build budget (e.g. >=180s, or base + per-TU predicted compile) while keeping 60s for queue waits.
3. Emit a clean human-readable reason from the last structured stage/file event (path + seconds) instead of the raw last output line.
4. Group recurring identical game-dll-build-at-game_build waits into one rolling issue with occurrences rather than one issue per wait_id.

Confidence: high on root cause and timings (directly read from the run journal); the build genuinely succeeded ~8s after the flag.

## Review (reviewed-no-fix)

### 2026-11-22T14:45:00.633Z — build-wait-reviewer

Reviewer: build-wait-reviewer

noop

## Context

### 2026-11-22T14:46:24.915Z — build-wait-reviewer

related_build: game-dll-20261122T142140783029Z-c00839

# issue-f66e6e82656b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-20T20:51:35.001Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed exact wait occurrence in a bounded test activation. No source edits, no file claims, no fix claimed.

Wait identity
- wait_id game-dll-build:2026-11-20T20:09:36.981Z; kind game-dll-build; stage game_build; elapsed 60.125s vs 60.0s budget; request auto-35; source_generation 38.
- Exactly one issue exists for this wait_id (brain.db issues query), so wait_id dedupe held. Issue opened 2026-11-20T20:10:37.206Z (coord seq 2254).
- Exactly one reviewer wake wake-76b6596d1467 was queued 2026-11-20T20:10:39.012Z for this issue and launched 20:33:56Z (attempt 2, child_pid 23228). That matches the issue acceptance.

What the wait was
- The in-flight attempt is game-dll-20261120T200937027322Z-8daf4c (generation 412, source_generation 38), started 20:09:37.027Z and completed successfully 20:18:25.865Z after total_seconds 528.823. It was not a hang.
- Timeline from .dev/build-timings/game-dll-20261120T200937027322Z-8daf4c.json/.jsonl: packet_registry codegen 20:09:38-20:10:09 (31s), manifest_snapshot 19s, source_snapshot 20:10:28-20:10:43 (14s), physics TU compile 64.3s, object/dependency scan 259.9s (scan_parse 253.4s), remaining compiles to 20:16:56, link 61.2s, publish 25.5s.
- The 60s check fired during the fixed prelude (changed_source_preflight 64.5s plus object scan 259.9s), before and independent of the arcade physics edit. The 60s game_build budget is below this host's measured non-trivial game-DLL baseline, so a similar issue is expected on most game-DLL builds.

Attribution defect (evidence-backed)
- The issue's structured build_id=2026-11-20T19:55:20.256758Z and generation=411 are the previously published artifact (attempt e1b74d), not the in-flight attempt. The correct identity (attempt 8daf4c, generation 412, stage source_snapshot) appears only inside the free-text reason.
- Consequence: build_id and generation on the issue point at the wrong build and cannot be used for correlation; only wait_id is reliable.
- Fix candidate (not claimed): while game_build is running, populate structured build_attempt_id/generation/stage from .dev/state.json active_build_* instead of the latest published artifact.

Evidence gap
- .dev/state.json points the active dev-loop journal at logs/2026-11-20/20261120_170259/dev-loop/events.jsonl, but neither that file nor its directory exists (Test-Path False). The dev-loop.wait.over_budget event cannot be read, so the dev-loop-journal portion of the proof expectation is unmet. Same class as issue-79e68f63fb01 / issue-4e3ef676ba20; do not duplicate that fix here.

Recommendation
- Calibrate per-stage game_build budgets against measured data (preflight plus object scan alone exceed 60s) or measure compile/link only; emit sub-stage wait attribution for codegen/manifest/source_snapshot/scan/compile/link/publish. Cross-reference wait-policy owner issue-e044a931c54b.
- The many open records titled "Wait over budget: game-dll-build at game_build" carry distinct wait_ids, so they are policy-conformant, but the volume confirms the budget is miscalibrated rather than each build being anomalous.

Acceptance for this occurrence: met except the dev-loop journal evidence, which is unavailable for reasons owned elsewhere. Final policy decision remains with a human reviewer.

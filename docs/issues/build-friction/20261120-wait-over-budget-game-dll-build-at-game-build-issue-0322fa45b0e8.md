# issue-0322fa45b0e8: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass)

### 2026-11-21T00:06:02.028Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence game-dll-build:2026-11-20T20:58:50.904Z (issue evidence complete: elapsed=60.257s, budget=60s, source_generation 41, request auto-41). No source edit.

Exact attempt evidence (.dev/build-timings/game-dll-20261120T205850979297Z-7cdcb2.json):
- success=true, generation=415, source_generation=41, started_utc=20:58:50.979Z, completed_utc=21:08:14.521Z, total_seconds=563.53
- worker_metrics: object_scan_seconds=340.257 (dominant), compile_seconds=96.924, link_seconds=37.094, publish_seconds=27.554
- request auto-41 state is "success" in .dev/state.json
The over-budget check fired at 60.257s while the healthy build was inside its normal 340s object-scan phase. Nothing was stalled; this is a false-positive friction alarm, not a build hang.

Wake timing evidence: request wake-15cdbf8cf0e0 was created 2026-11-20T20:59:52Z and launched 2026-11-20T23:44:04Z (MIMITA_WAKE_ID=wake-15cdbf8cf0e0). The reviewed build had already succeeded at 21:08:14Z, so the reviewer activation arrived ~2h44m after the build finished and ~36m after it started; it could not influence or observe the wait. Wake prompt delivery also matched issue-62ab3daa615d (only the first line reached the session), so the target issue had to be resolved from MIMITA_WAKE_ID and the .dev request file.

Review result: issue acceptance is met for this occurrence (wait_id, stage, reason, elapsed, generation, request id, evidence, one reviewer wake). Systemic recommendation (stall-based budget, signature dedup, and stale-wake expiry) is filed on umbrella issue-9a4503f6ed68; no duplicate issue created, no source fix claimed.

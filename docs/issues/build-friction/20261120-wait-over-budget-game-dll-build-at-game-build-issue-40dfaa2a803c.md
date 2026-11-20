# issue-40dfaa2a803c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-active-build)

### 2026-11-20T17:22:49.327Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-4c7fdaef7a55), 2026-11-20.

VERDICT: confirmed over-budget wait; not a detection defect. The wait fired during a genuine, actively compiling game-DLL build, not an idle or wedged loop.

EVIDENCE
- Wait record: wait_id game-dll-build:2026-11-20T16:21:27.899Z, kind game-dll-build, stage game_build, elapsed 62.368s (budget 60s), generation 405, source_generation 20, request auto-20, build_id 2026-11-20T07:47:09.981634Z.
- Build attempt: .dev/build-timings/game-dll-20261120T162131173533Z-83bf96.json started_utc 2026-11-20T16:21:31.173Z, completed_utc 16:38:16.483Z, total_seconds 1005.136, success=false.
- The attempt was busy, not stalled: worker_metrics changed_source_preflight_seconds 429.618, object_scan_seconds 500.072, compile_seconds 1360.694, dependency_check_ms 121770.77, scan_parse_ms 360444.779, link_seconds 136.181, compiled 23 of 642 sources.
- It failed on a real compiler error, not a timeout: src/network/server-gamemode.cpp:3949-3951 'const struct GroundState' has no member named 'groundNormal' (worker_error 'changed-source preflight failed: src/network/server-gamemode.cpp in 312.998s'); slowest compile src/network/server-gamemode.cpp 930.07s.
- Issue fields complete: reproduction, expected behavior, acceptance, paths (devscripts/dev-loop.py, devscripts/build_friction.py, devscripts/agent-wake.py), risk, and proof expectation all present; readiness ready=true, missing=[].
- Reviewer wake cadence: exactly one wake queued for this wait, wake-4c7fdaef7a55 at 2026-11-20T16:22:32.947Z; codex adapter launched 16:22:34 and retried as opencode 16:39:30 / 17:02:23. One wake record with adapter fallbacks, no duplicate issue/wake.

DUPLICATE CHECK
- No other issue carries this wait_id; occurrences list is empty. Other game-dll-build/game_build friction issues (b0ba8698acd7, 9a9807fa0128, bb273ac1386c, e84cfa28c7f7, 25b14f436a3e, etc.) use different wait_ids, so this is not a duplicate and needs no occurrence append.

ACTION
- Informational for the wait-issue mechanism: behavior matches acceptance (issue + exactly one reviewer wake). The real cost is build latency, owned by issue-e044a931c54b (avoidable waits) and issue-7d3dcfd7433b (per-stage wait measurement). Recommend the latency owner use the worker_metrics above (object_scan/dependency_check/preflight dominate) as the next reduction target rather than treating this wait as a dev-loop bug.
- No source edits, no file leases, no build requested by this review.

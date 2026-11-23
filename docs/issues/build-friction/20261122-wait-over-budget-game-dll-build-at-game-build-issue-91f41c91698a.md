# issue-91f41c91698a: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass)

### 2026-11-22T22:29:07.384Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait game-dll-build:2026-11-22T22:03:49.822Z (issue-91f41c91698a). Verdict: pass - the wait record and single-reviewer-wake hook behaved as designed. The wait was a genuine over-budget incremental build, not a stall.

Direct observations:
- Active attempt game-dll-20261122T220349886220Z-1b865a, source_generation 8, generation 532, revision a23230be0aee598d40f353e16bc5d4e97369fdd56d0768998bf4c2760562e64c, request auto-8 (source: .dev/build-timings/game-dll-20261122T220349886220Z-1b865a.json and .jsonl).
- Started 22:03:49.886Z, completed 22:04:51.582Z; total 61.685s (worker 58.126s). The 60.435s budget tripped ~1.3s before the attempt finished.
- Phase breakdown: changed_source_preflight 39.147s (this includes the 39.072s compile of src/network/server-npcs.cpp), object_cache_scan 1.638s, compile(identity provider) 3.567s, link 12.624s for 642 objects / 481,461,829 bytes, publish 0.518s. This is the normal cost of a one-TU incremental whole-game DLL build (large TU compile + full link), not a hang.
- Exactly one issue exists for this wait_id; exactly one reviewer wake was queued (wake-32c05fb63dab, wake.queued 2026-11-22T22:04:51.005Z, .dev/agent-wake/events.jsonl). Wake delivery: codex claimed/launched 22:04:55.9 and exited; opencode retried 22:15:19.5 and launched 22:18:00.6 (~13 min), which is a wake-delivery latency concern, not a defect of this wait record.

Caveats already tracked elsewhere (no duplicate issue filed):
- Stored evidence.reason is a truncated mid-JSON tail (48265.821,"build_attempt_id":...progress_write_ms":3.595,"record_overhead_ms":4.252}), from dev-loop.py:2832-2867 setting game_build_detail = line.rstrip()[-240:] for every worker output line. Tracked as tech-debt issue-6e08d89b55e3 and parent issue-136d4553ff92.
- evidence.build_id (2026-11-22T21:32:48.195707Z) and generation (530) are the last published artifact, not the active attempt 532; the true attempt survives only inside the truncated reason. Same wait-instrumentation gap as earlier reviews.
- Repeat-pattern cause: WAIT_ISSUE_SECONDS=60 is below normal incremental game-DLL cost (this build 61.7s), so routine builds file a wait issue; tracked in issue-136d4553ff92 and issue-9a4503f6ed68.

True in-progress attempt for this occurrence is game-dll-20261122T220349886220Z-1b865a / generation 532. No source edited, no lease taken. Confidence: high on timings and attempt identity, and on the one-issue/one-wake counts; reason-quality and budget-threshold fixes remain with their existing owners.

# issue-25b14f436a3e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (evidence_reviewed)

### 2026-11-20T17:13:20.294Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait occurrence game-dll-build:2026-11-20T16:12:47.255Z. Evidence: .dev/build-timings/game-dll-20261120T161247800566Z-4a70f1.jsonl shows build.started 16:12:47.801Z (source_generation 19; request_ids 2026-11-20T16:08:16.089Z-befd0a6d and 2026-11-20T16:09:28.468Z-1bb65630); stage codegen:resident_state_codegen completed in 3.07s; stage codegen:packet_registry_codegen started 16:12:53.237Z and failed 16:14:36.043Z after 102.806s (exit 1); build.failed 16:14:54.163Z total 126.362s. Summary .json error=codegen check failed: packet_registry_codegen.py. Issue evidence matches this attempt (elapsed 101.105s, generation 405, source_generation 19), so the wait is real and the wait_id/stage/reason/elapsed/request_ids fields are complete. Root cause: packet_registry_codegen.py --check found stale generated artifacts (src/host/generated/packet-registry-v1.h and/or config/hot-reload/packet-ledger.json) relative to src/network/packets.h and dispatch sources; the 102.8s is machine load, not the check itself. A read-only rerun now returns '[PACKET CODEGEN] ok packets=78' exit 0, so the stale window has closed. Distinct occurrence, not a duplicate: 63dccbd8f8b6=08:31:55Z, 2008e743c710=08:45:25Z, this=16:12:47Z, b0ba8698acd7=16:38:24Z. Separate friction: concurrent packet-source edits can trip the stale-codegen guard and fail the whole build, and this reviewer wake was created 16:14:58Z but launched 16:39:30Z (2 attempts, about 24.5 min latency), so the wait-issue loop is itself slow. No source edits made by reviewer.

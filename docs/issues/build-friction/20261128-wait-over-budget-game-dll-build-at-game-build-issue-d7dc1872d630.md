# issue-d7dc1872d630: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-real-wait-not-stalled; fields-present-but-reason-unreadable; build-identity-partial; reviewer-wake-queued-once; journal-over_budget+reviewer-wake-present; wake-fallback-delayed-45m; cause-header-rebuild+CPU99%/141MB)

### 2026-11-28T08:51:21.497Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded reviewer review (build-wait-reviewer, wake-02667c67d11b, work-ade3e27a9502). Read-only: no source edit, no build, no runtime command, no close, no fix claim.

WAIT / ISSUE IDENTITY
- wait_id game-dll-build:2026-11-28T07:56:14.606Z; kind game-dll-build; stage game_build.
- budget 60.0s; elapsed 60.749s; issue created 2026-11-28T07:57:17.548Z.
- Attempt game-dll-20261128T075614669320Z-cb02af; request build-request-06bad77bfe3d; source_generation 23; published generation 814; changed path src/network/packets.h.
- Journal: logs/2026-11-28/20261128_061051/dev-loop/events.jsonl:628 (dev-loop.wait.reviewer-wake) and :629 (dev-loop.wait.over_budget).

ACCEPTANCE CHECK
- wait_id: pass.
- stage: pass.
- reason: present but UNREADABLE - evidence.reason is a mid-JSON tail `:60156.0,"build_attempt_id":"game-dll-20261128T075614669320Z-cb02af",...`. This is the last 240 chars of a [GAME_FILE] worker row, from devscripts/dev-loop.py:3130 (`detail = line.rstrip()[-240:]`) assigned at :3160 and read as `reason` at :3912. Tracked as issue-6e08d89b55e3 (occurrence appended).
- elapsed: pass (60.749).
- build/run identity: PARTIAL. Recorded build_id is the stale prior build "2026-11-27T17:45:19.700118Z" and generation field is 813, while the in-flight attempt is generation 814 (game-dll-20261128T075614669320Z-cb02af). The attempt id exists only inside the raw reason tail. Tracked as issue-7d3dcfd7433b.
- evidence: pass (full wait dict persisted).
- Reviewer wake queued exactly once: PASS. One request (wake-02667c67d11b), one wake.queued (07:57:21.943Z), one retry under the same wake id. No duplicate issue or duplicate wake for this wait_id.

BUILD OUTCOME (real work, not a stalled build)
- .dev/build-timings/game-dll-20261128T075614669320Z-cb02af.json: success=true, published generation 814, total 404.234s.
- compile 388.984s, link 2.375s, object_scan 5.297s; 178 TUs compiled, 469 skipped.
- queue_wait_max 370.781s, queue_wait_mean 180.226s: the worker pool was starved, not deadlocked.
- Slowest TUs: src/network/server-gamemode.cpp 65.452s, src/mapeditor/map-editor.cpp 48.088s, src/vehicles/vehicle-system.cpp 39.903s, src/network/server-npcs.cpp 39.701s, src/main-systems.cpp 38.355s.
- Cost driver: the src/network/packets.h edit forced a wide recompile; cache_invalidations list 32 dependents of src/network/multiplayer-context.h and server.h. postmortem records header_fanout=null (no recorded fanout).

RESOURCE PRESSURE (dominant measured cause of exceeding 60s)
- resource.json: mean host CPU 98.389%, peak 100.0%, minimum available memory 148,471,808 bytes (141.6 MB).
- A flat 60s budget is below any wide header rebuild under this pressure, so the over-budget signal is genuine build cost, not a hang. Owner issue: issue-2a5c5da53565 (dev-loop reliability under high CPU/memory).

JOURNAL EVENTS
- dev-loop.wait.reviewer-wake @ 2026-11-28T07:57:37.134Z: queued=true, adapter codex.
- dev-loop.wait.over_budget @ 2026-11-28T07:57:37.473Z: created=true, issue issue-d7dc1872d630.
- Both events exist for this wait_id; no dev-loop.wait.issue-failed. This occurrence did NOT hit the 30s `run --once` timeout variant that broke earlier waits (issue-04ffbd3f90ed). Journal-drop issue-6e2d7b91e7d9 does not apply here (events present).

WAKE LATENCY / FALLBACK (separate observation, not a lost wake)
- .dev/agent-wake/events.jsonl: queued 07:57:21.943; claimed codex 07:57:36.767; launched codex pid 26996 07:57:36.897; wake.retry to opencode 08:20:38.138 ("previous adapter process exited; trying fallback"); claimed opencode 08:42:36.588; launched opencode pid 23212 08:42:36.653.
- Queue-to-working-launch = ~45m15s (reviewer coverage delayed). The 08:20:38 retry coincides with the two concurrent same-agent wakes recorded in issue-f8939f05cad2. Cross-linked there; no duplicate issue filed.

DISPOSITION
- Acceptance: one issue and one reviewer wake with evidence are present. The only acceptance shortfalls are the unreadable reason and the partial build/run identity, both already tracked (issue-6e08d89b55e3, issue-7d3dcfd7433b) and not blocking.
- Not a stalled or failed build; generation 814 published. No build fix required for this occurrence.
- Code fixes needed under separate ownership: readable wait reason (issue-6e08d89b55e3), wake concurrency/latency (issue-f8939f05cad2), and resource-pressure reduction (issue-2a5c5da53565). Reported only; not claimed by this review.

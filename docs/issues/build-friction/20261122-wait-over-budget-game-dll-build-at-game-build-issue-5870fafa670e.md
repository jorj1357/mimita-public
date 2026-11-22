# issue-5870fafa670e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-intrinsic-build-cost)

### 2026-11-22T14:45:04.322Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-3afe433ede21). Read-only: no source edit, build request, worker, test run, or file lease. No duplicate issue created.

FACTS
- Wait occurrence: wait_id game-dll-build:2026-11-22T14:22:57.758Z, kind game-dll-build, stage game_build, elapsed 61.866s, budget 60.0s, recorded generation 498, request_ids [auto-4].
- In-flight attribution: build attempt game-dll-20261122T142258012829Z-326bd4 started 2026-11-22T14:22:58.012Z and succeeded 2026-11-22T14:25:15.099Z as generation 499, revision a56cbe72153ee92584a46c971bf81f90a40c095c0cf37be9d382370df5282125.
- Attempt cost: total 137.01s, worker 117.309s, only changed source src/network/server-npcs.cpp. Slowest compile src/network/server-npcs.cpp at 67.005s; link 36.797s; publish 1.457s.
- At the 61.866s wait mark the only significant changed translation unit, server-npcs.cpp, was still compiling (67.005s), so the build was progressing normally.
- Acceptance met: the issue carries wait_id, stage, reason, elapsed, build_id, generation, source_generation and request IDs; the dev-loop journal contains exactly one dev-loop.wait.over_budget and one dev-loop.wait.reviewer-wake for this wait_id (created:true); no duplicate issue was created for this occurrence.

CONCLUSION
Confirmed over-budget wait caused by legitimate intrinsic incremental whole-game DLL cost, not a hang, queue stall, or lost work. The flat 60s budget is below the normal ~131-137s cost when a heavy translation unit such as server-npcs.cpp changes. This is the same recurring signature already tracked by umbrella issue-9a4503f6ed68 (automatic over-budget wait issues) and issue-e044a931c54b (remove avoidable waits); no source fix is warranted in this issue. This occurrence's reason field is well-formed, distinct from the truncated-reason defect in issue-6e08d89b55e3.

RECOMMENDATION
Keep this issue as one recorded occurrence; do not create further identical wait issues for the same signature. Add this attribution (active compile versus stalled queue) to issue-9a4503f6ed68 and derive per-stage wait budgets from measured p50/p95 instead of a flat 60s.

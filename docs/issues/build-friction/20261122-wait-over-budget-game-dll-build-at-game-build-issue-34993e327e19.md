# issue-34993e327e19: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-real-wait; root-cause=compile-error; evidence-gaps)

### 2026-11-22T17:50:05.319Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-0eacfd7f99aa). Read-only for source, builds, and runtime: no source/config/build-script edits, no compiler/build.py/module worker/second dev-loop, no new runtime, no file leases, no duplicate issue created.

WAIT OCCURRENCE
- wait_id game-dll-build:2026-11-22T17:17:22.162Z; kind game-dll-build; stage game_build; elapsed 60.227s vs 60.0s budget; request_ids [2026-11-22T17:17:20.462Z-c2fd63f7]; issue created 2026-11-22T17:18:24.183Z.
- dev-loop run journal logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl lines 1548-1549: dev-loop.wait.reviewer-wake and dev-loop.wait.over_budget at wall_time 17:18:39.195-196Z, issue issue-34993e327e19 created=true for this wait_id.

PINNED ATTEMPT (real, not a stall)
- .dev/build-timings/game-dll-20261122T171723932165Z-86a612.json/.jsonl: attempt game-dll-20261122T171723932165Z-86a612, started 17:17:23.932Z, completed 17:21:59.497Z, total_seconds 275.557, success=false, failed_phase changed_source_preflight, worker_error "changed-source preflight failed: src/npc/npc.cpp in 202.979s", generation 502, revision 69bfdeb919774384922129cdf8dc7930a7dead2ce9e74e2ff33bbce844dc8cb9.
- At the 60.227s check the attempt was inside changed_source_preflight (manifest_validation completed 17:18:08.572Z, source_snapshot completed 17:18:16.484Z, packet_registry_codegen completed 17:18:22.377Z) and npc.cpp/server-npcs.cpp preflight compiles were still running, so the overrun is genuine build latency, not a wedged or released worker.
- Failure output: src/npc/npc.cpp:3836-3837 "class Npc has no member named hasLoggedDash" (hasLoggedDash referenced in npc.cpp but not declared in npc.h at that snapshot). Changed paths for the attempt: src/debug/log-manager.cpp, src/debug/log-manager.h, src/network/server-npcs.cpp, src/npc/npc.cpp (change notes from codex-networking-stall-fix and cpu-resource-reduction-implementation).
- This is the same failure class previously reviewed under issue-b24f9f79794d (07:44) and issue-a7464f307b2f (07:52). It recurred here after the dash-diagnostic field was re-used/edited concurrently; retry attempt game-dll-20261122T172200384686Z-a8e690 also failed changed_source_preflight on npc.cpp (65.67s, total 105.448s), and attempt game-dll-20261122T173219711862Z-89e54d failed npc.cpp preflight again (27.429s, request auto-5). Current working tree has no hasLoggedDash match in src/npc/npc.cpp or src/npc/npc.h, so the immediate mismatch is not present in source now.

ACCEPTANCE CHECK
- wait_id, stage, elapsed, budget, reason, request_ids and evidence present; readiness ready=true, missing=[].
- Reviewer wake queued exactly once for this occurrence: .dev/agent-wake/requests/wake-0eacfd7f99aa.json (created 17:18:27.630Z, issue issue-34993e327e19, attempts=2). Single request, no duplicate wait issue.

EVIDENCE GAPS
1. Dispatch latency (recurrence of issue-84c584957e65): wake-0eacfd7f99aa queued 17:18:27.783Z, primary codex claimed/launched 17:18:36.732Z/17:18:37.191Z but exited without delivering; wake.retry switched to opencode 17:23:09.539Z; opencode claimed/launched 17:24:14.245Z/17:24:14.446Z. Time from queue to working launch ~346.7s (5m46.7s), above the proposed <=5 min bound. Recorded as a structured occurrence on issue-84c584957e65.
2. Wake-record misattribution: dev-loop.wait.reviewer-wake (line 1548, 17:18:39.195Z) is stamped with this wait_id and issue-34993e327e19, but its embedded wake object is wake-53e8678c15f2 (issue-cfe902b4b659, log 2026-11-22T16-26-06.544Z), not wake-0eacfd7f99aa. The correct wake exists and is evidenced in .dev/agent-wake/events.jsonl, so the proof expectation "reviewer wake record" cannot be read from the dev-loop event alone.
3. wait evidence build_id is the previously published artifact 2026-11-22T08:00:31.376619Z and generation is null; the in-flight attempt id/generation (86a612 / 502) must be recovered from build-timings, not the wait evidence.
4. Why the wait recorded this stage: the reason is a cumulative codegen completion marker (elapsed_ms 58444.843) rather than a clean game_build reason.

VERDICT
Confirmed real over-budget wait, not a false stall. It was produced by a failing changed-source preflight on npc.cpp caused by a concurrent shared-source field mismatch (hasLoggedDash), and auto-retries kept producing further over-budget waits in the same window. The wait/wake path worked (one wake queued for this occurrence), but reviewer dispatch was ~5m47s late and the dev-loop reviewer-wake event carried the wrong wake payload. No new code fix claimed by this read-only review.

RECOMMENDED (report only; not claimed here)
1. Owners of the npc.cpp/npc.h dash-diagnostic edits (cpu-resource-reduction-implementation / fcs-npc-speed-combat-investigation) should keep the hasLoggedDash member and every reference in sync, or remove both, so a candidate build does not spend 100-275s failing on a one-line mismatch. The dash-guard coordination thread already flags the field restore.
2. agent-wake/dev-loop wait emission: attach the wake actually queued/launched for the wait_id instead of the most recently launched wake, so reviewer-wake events are not misattributed under a dispatch burst.
3. Keep the late-dispatch and issue-per-attempt budget policy under issue-84c584957e65, issue-9a4503f6ed68, and issue-136d4553ff92; do not file another duplicate for this wait_id.

CONFIDENCE
High on the pinned attempt facts, the failure chain, the single queued wake, and the dispatch-timing/misattribution evidence (all from the exact run journal, build-timings, and .dev/agent-wake/events.jsonl). High that the immediate npc.cpp mismatch is absent from the current tree. Medium on the preferred budget/aggregation remedy, which is an owner decision.

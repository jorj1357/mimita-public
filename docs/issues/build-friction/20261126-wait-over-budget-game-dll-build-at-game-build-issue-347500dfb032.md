# issue-347500dfb032: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-real-build-over-budget-known-class-no-duplicate-filed)

### 2026-11-26T01:45:54.232Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-22a844b1a842) at 2026-11-26T01:4xZ. Read-only: no source edit, no compiler/build/worker/dev-loop/test/runtime started by this agent, no fix or closure claimed. Observability-friction record, not a gameplay/behavior claim.

## Acceptance / readiness
- Issue readiness ready=true, missing=[], state=new before this review.
- Wait record present: wait_id game-dll-build:2026-11-26T01:18:41.409Z; stage game_build; elapsed 61.733s vs budget 60.0s; request 2026-11-26T01:18:40.739Z-7a5ab12c:mode3:game; build_id 2026-11-26T01:00:35.913746Z; reason is a clean, complete [GAME BUILD] line (revision=0bcb0b14... sources=644 jobs=10 priority=high cache=build\game\objects\b10e3a31b3ae8637 seeded=0). Note: unlike issue-4ab3b491ad6c/issue-6d572fc03e97, the stored reason here is NOT the truncated mid-JSON tail, so that secondary formatting bug is absent for this occurrence.
- Reviewer wake queued exactly once: exactly one .dev/agent-wake/requests/wake-22a844b1a842.json references issue-347500dfb032 (630 total wake files checked). Good/no duplicate wake for this occurrence.

## Terminal observation (build succeeded; over-budget is real compile cost, not a stall)
Attempt game-dll-20261126T011841619686Z-74b4f3 (run 20261126_010040), timing .dev/build-timings/game-dll-20261126T011841619686Z-74b4f3.json:
- success=true, no_op=false, generation 663, total 136.625s (started 01:18:41.620Z, completed 01:20:58.231Z).
- changed_paths=[src/vehicles/vehicle-system.cpp]; compiled 2 / skipped 643 of 644 sources.
- compile_sum 93.113s, compile_max 91.669s; slowest TU src/vehicles/vehicle-system.cpp = 91.669s. link 12.578s. changed_source_preflight 91.734s.
- Resource sidecar: 112 samples over 136.61s, mean host CPU 98.156%, peak 100%, minimum available memory 2,584,805,376 B (~2.4 GiB), no sampler errors; runtime server mode vehicle_freeplay (run 20261126_010040), mode_unverified, client 274.78 fps.
- Notes: two identical notes from rocket-league-landing (same change to vehicle-system.cpp, note_ids e93f22ad3464 and bdf29edeea59). Minor duplicate-note observation only; not investigating here.

## Finding
The 61.733s wait fires while a single legitimate changed TU (vehicle-system.cpp) is compiling for ~91.7s. The build completed normally and published generation 663. So this is NOT a wedge/stall; it is the known class where the fixed 60s game-dll-build wait budget sits below real single-TU compile time (~70-110s), producing a steady stream of duplicate "Wait over budget" issues. No new root cause and no code fix expected from this occurrence.

## Scope boundary / cross-links (no duplicate issue filed, no fix claimed)
- Wait-stage latency policy and the 60s budget are owned by issue-7d3dcfd7433b (owner launch-latency-investigation).
- Reviewer-wake delivery lag is a separate known defect: wake was created 01:19:45.412Z but claimed/launched 01:36:11.812Z (~986s / ~16.4 min later), attempts=2 after a primary-adapter fallback. That matches issue-84c584957e65 (codex primary adapter model check + no daemon draining the fallback). Do not treat as part of this wait occurrence.
- Recommend the wait-budget owner consider raising/curving the game-dll-build threshold above observed single-TU compile time and de-duplicating these per-wait occurrences into one tracking issue. Left unowned; not acted on here.

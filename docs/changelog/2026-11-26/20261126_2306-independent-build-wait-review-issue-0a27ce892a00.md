# Independent build-wait review — issue-0a27ce892a00

- **When:** 2026-11-26T22:39Z (wake queued), 2026-11-26T23:06Z (review written)
- **Agent:** build-wait-reviewer (wake-bb643fb35c84, opencode)
- **Issue:** issue-0a27ce892a00 — Wait over budget: game-dll-build at game_build
- **Wake task:** review over-budget game-dll-build wait at stage game_build, elapsed 60.336s
- **Wait / attempt / run:** game-dll-build:2026-11-26T22:38:08.169Z / game-dll-20261126T223808304994Z-2d57e6 / 20261126_201711
- **Scope:** read-only review. No source edit, no build/test/runtime started by this agent, no fix or closure claimed.

## Evidence

- `logs/2026-11-26/20261126_201711/dev-loop/events.jsonl`: exactly one `dev-loop.wait.over_budget` (created=true, 22:39:16.762Z) and exactly one `dev-loop.wait.reviewer-wake` (wake-bb643fb35c84, 22:39:11.145Z) for this wait. Wait-ID dedup held; no duplicate issue for this wait_id.
- `.dev/build-timings/game-dll-20261126T223808304994Z-2d57e6.json`: success, no_op=false, generation 731, total 277.797s, compile 245.703s, link 8.953s, object_cache_scan 4.609s, publish 0.594s; 65 compiled / 580 skipped from a `map-config.h` + `map-editor.h` header fanout; compile_sum 2,265,717ms (max single TU 93.4s), jobs 10.
- `.dev/build-timings/game-dll-20261126T223808304994Z-2d57e6.resource.json`: mean host CPU 98.909%, peak 100%, 186 samples @1s, duration 277.766s, 12 logical CPUs; available memory 380,485,632 B (~363 MiB, 97.7% used) at start dropping to 5,275,648 B (~5.03 MiB) — severe CPU and memory pressure. Live runtime during build: mode vehicle_freeplay, map arena1, npcs 0.
- Review body appended to issue-0a27ce892a00 (verdict `confirmed-real-build-over-budget-one-wake-no-duplicate`); `related_build`/`related_run` pinned to the authoritative attempt and run.
- Occurrence appended to tech-debt issue-6e08d89b55e3 (reason field is a truncated worker-output JSON tail; this occurrence happens to embed attempt/gen/revision so identity is recoverable by luck).
- Resource-pressure evidence appended to issue-136d4553ff92 (DLL build stage accounting; owner build-coordinator).

## Outcome

- Confirmed real, non-stalled build over the flat 60s budget: the wait fired at ~60s with ~213s of legitimate compile still remaining; first divergence is budget-vs-natural-compile-cost, not stalled progress.
- Acceptance holds on the queue side (fields present, one reviewer wake). The wait snapshot is still not self-identifying: top-level `build_id` = previous published build 2026-11-26T20:12:20.599827Z (dev-loop.py:3896 `self.latest_build`) and `generation` = null (dev-loop.py:3897 `self.game_build_generation`); the true attempt/gen survive only inside the truncated reason fragment.
- Systemic recurrence measured: 221 open `game-dll-build` build-friction issues (205 in_review, 15 new, 1 assigned), elapsed 60.01–151.855s, and 207 of 221 are <=63s. The flat 60s budget is below typical legitimate rebuild latency, so this policy generates heavy, largely duplicate noise.

## Follow-up (unowned)

- Human/shared-authority decision on a scaled/deduped game-dll-build wait budget and self-identifying wait evidence (see issue-136d4553ff92, issue-7d3dcfd7433b, issue-6e08d89b55e3). No budget, spec, or code change made by this review.

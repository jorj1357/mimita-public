# issue-24863e1f595e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T17:39:59.019Z — build-wait-reviewer

related_build: game-dll-20261126T171310909318Z-faf8c7
related_run: 20261126_170323

## Review (confirmed-legitimate-wait-not-stall; identity-defect-recurred; wake-queued-once-dispatch-late)

### 2026-11-26T17:40:04.383Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-04a89c03a141), read-only. No source edit, build request, runtime, or close.

OBSERVED FACTS (pinned)
- Wait occurrence: wait_id game-dll-build:2026-11-26T17:13:10.456Z, kind game-dll-build, stage game_build, elapsed 60.731s vs budget 60.0s (over 0.731s), source_generation 10, request_ids [2026-11-26T17:13:09.744Z-32106cf1:mode3:game]. Journal logs/2026-11-26/20261126_170323/dev-loop/events.jsonl lines 99 (dev-loop.wait.reviewer-wake) and 100 (dev-loop.wait.over_budget, created=true).
- Active in-flight attempt: game-dll-20261126T171310909318Z-faf8c7, generation 703, revision 5eb31cd568eb9799724c6551dff363d6545634d3110f55b88affc7cb091a1374, run 20261126_170323. .dev/build-timings/game-dll-20261126T171310909318Z-faf8c7.json: success, published generation 703, total 137.203s, worker_total 106.875s, compile 92.001s (compile_sum 299191ms, 5 compiled / 640 skipped), link 11.984s, changed_source_preflight 90.735s.
- Real wait, not a stall. Four large TUs changed by two build notes compiled concurrently: src/vehicles/vehicle-system.cpp 90.356s, src/engine/engine-tick-render.cpp 72.971s, src/network/vehicle-networking.cpp 71.164s, src/engine/engine-tick-ui.cpp 63.485s. packet_registry codegen completed 17:13:33.821Z (~22.5s in); compiles submitted ~17:13:37; first TU completed 17:14:40.896Z. A single TU exceeded the 60s budget on its own.
- Resource evidence (direct sampler, .dev/build-timings/game-dll-20261126T171310909318Z-faf8c7.resource.json, 90 samples @1s over 137s): host CPU mean 98.674%, peak 100%; minimum available memory 121,266,176 B (~115.6 MiB). Same run hosted a live pair: server vehicle_freeplay (pid 33008) and client pid 34624 at 287.88 fps (32 frames over a 4.167ms budget). Compile ran concurrently with a ~100%-CPU live client plus 10 compile jobs.
- Reviewer wake: wake-04a89c03a141 queued exactly once for this wait at 17:14:21.091Z (.dev/agent-wake/events.jsonl line 3571). codex claimed/launched 17:14:28.226/28.346Z (pid 22576); wake.retry to opencode 17:21:54.669Z; opencode claimed/launched 17:35:26.922/27.010Z (pid 20612). One wake.queued, attempts=2, queue-to-working-launch 1265.9s (~21m06s).

ACCEPTANCE REVIEW
- wait_id, stage, reason (clean "[GAME BUILD] revision=... sources=644 jobs=10 priority=high cache=... seeded=0"), elapsed, budget, source_generation, request_ids: present and correct. The reason-tail defect does NOT apply to this occurrence.
- build/run IDs: DEFECT. The record stores build_id 2026-11-26T16:59:26.767416Z and generation 702 (last published artifact), not the in-flight attempt faf8c7 / generation 703; no run_id. The true attempt is recoverable only from adjacent dev-loop.game-build.started lines and the reason revision. This is the same identity defect as issue-9a4503f6ed68 (dev-loop.py reports self.game_build_generation, assigned only on completion).
- Reviewer wake queued exactly once: MET. Delivery latency misses the <=5 min target of issue-84c584957e65.

VERDICT: confirmed-legitimate-wait-not-stall; identity-defect-recurred; wake-queued-once-dispatch-late.

INFERENCE / UNKNOWNS
- The flat 60s budget is shorter than a normal 4-heavy-TU incremental DLL build under a live game (a single TU took 90.4s), so the friction flag is policy noise over a healthy, published build, not an infrastructure fault. Owned by wait-policy issue-e044a931c54b / build-lifecycle issue-136d4553ff92.
- A missing resource sample is unknown, not zero; only this attempt's sampler was available.
- No re-run, identity fix, or runtime proof was performed here; that is owner work.

NEXT DISCRIMINATING CHECK (owners)
- Identity (issue-9a4503f6ed68): put the active build_attempt_id/generation into the wait record so reviews need not reconstruct it from adjacent lines.
- Wake (issue-84c584957e65): drain the retried fallback without waiting for an unrelated trigger, and stop blind codex-primary retries.
- Policy (issue-136d4553ff92 / issue-e044a931c54b): use per-stage/attempt-aware budgets or dedupe so healthy >60s multi-TU builds do not mint a new friction issue each time.

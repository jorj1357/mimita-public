# issue-ac23ab33d184: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T01:39:31.701Z — build-wait-reviewer

related_build: game-dll-20261126T005556909723Z-fa8154
related_run: 20261126_005302

## Review (reviewed-real-build-over-budget-known-class-no-duplicate-filed)

### 2026-11-26T01:39:31.912Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-56382fd7e40e) at 2026-11-26T01:2xZ. Read-only: no source edit, no compiler/build/worker/dev-loop/test/runtime started by this agent, no fix or closure claimed. This is an observability-friction record, not a gameplay or behavior claim.

## Acceptance / readiness
- Issue readiness ready=true, missing=[], state=new before this review. wait_id, stage, reason, elapsed, budget, paths, and proof expectation are present.
- Wait record: wait_id game-dll-build:2026-11-26T00:55:56.696Z; stage game_build; elapsed 60.564s vs budget 60.0s; request 2026-11-26T00:55:56.518Z-e862a38a:mode3:game; source_generation 2; build_id 2026-11-26T00:49:32.788771Z; generation null.
- Exact-wait issue created 2026-11-26T00:56:57.380Z; reviewer wake wake-56382fd7e40e queued exactly once at 00:56:59.578Z.
- PARTIAL GAP: the wait record has no run_id and its build_id is the last published artifact, not the in-flight attempt. The waited-on attempt game-dll-20261126T005556909723Z-fa8154 / generation 662 appears only inside the truncated reason. This is the known identity defect on issue-9a4503f6ed68 (occurrence appended there).

## Direct observations
Journal: logs/2026-11-26/20261126_005302/dev-loop/events.jsonl
- L76 dev-loop.wait.reviewer-wake 00:57:03.352Z; L77 dev-loop.wait.over_budget 00:57:03.354Z (created:true).
- L59-L69: manifest snapshot, manifest validation, source snapshot, and codegen ran before the worker; dev-loop.game-build.started at 00:56:15.676Z, 18.98s after the wait clock started. The first TU completed at 00:56:21.835Z.
- At the 60s mark only 6 of 36 TUs had completed (L70-L75): module-identity-provider 0.638s, destructible-render 21.336s, arena-ball 29.070s, client-collision-world-view 30.617s, destructible-async-cut 30.983s, impact-system 32.583s.
- The longest TU src/network/server-gamemode.cpp started 00:57:04.349Z, about 1s after the budget fired, and took 110.490s. That single TU alone exceeds the 60s budget.
- L108 terminal result 00:59:11.437Z: status ok, no_op=false, generation 662, revision e4b902e415b95109a560e3809445e256660c070b5b202465e6741520466187c2, compiled 36, skipped 609, compile 157.766s, link 11.203s, total 194.141s; L109 generation ready at ~196.891s.
- Changed paths: src/physics/physical-entity.h, src/physics/physical-entity.cpp, src/vehicles/vehicle-system.cpp. The header invalidated 30 cached TUs; 2 more had missing/invalid dependency files; 36 compiled. Slowest TUs: server-gamemode 110.490s, server-npcs 70.310s, vehicle-system 70.153s, server-packets 61.373s, physical-entity 58.108s.
- CONCLUSION: real work, not a stall. The flat 60s budget fired mid-compile and ~116s before publication.

## Resource evidence
Sidecar .dev/build-timings/game-dll-20261126T005556909723Z-fa8154.resource.json:
- status=captured; 136 samples @1.0s over 196.813s; mean host CPU 97.950%, peak 100.0%; minimum available memory 215,298,048 B (~205 MiB); no sampler errors; unavailable matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation.
- runtime_alignment mode_unverified: same_run_server_client=true run 20261126_005302; server vehicle_freeplay; client mode stale_or_unreported; client FPS 157.70 with 60/158 frames over budget; server npcs=0 players=1.
- Inference only: a 10-job, 36-TU compile is expected to saturate CPU. Do not attribute the >60s wait solely to host pressure. The ~205 MiB free-memory minimum is a low point and needs a matched pre-build baseline before any causal claim. gap_key appended to issue-2a5c5da53565.

## Wake dispatch (exactly-once held; delivery did not)
.dev/agent-wake/events.jsonl, wake-56382fd7e40e:
- wake.queued 00:56:59.578Z; codex claimed 00:57:03.161Z and launched pid 29932 at 00:57:03.213Z.
- codex failed with model_not_found: "The 'gpt-6-luna' model is not supported when using Codex with a ChatGPT account" (wake log 2026-11-26T00-56-59.572Z lines 11-12).
- wake.retry 00:58:51.867Z; opencode claimed 01:19:47.074Z and launched pid 30588 at 01:19:47.086Z.
- Queue-to-final-launch 1367.5s (22m47.5s), attempts=2. One wake.queued; no duplicate launch.
- The opencode fallback was drained 2.86s after the next over-budget wait created issue-347500dfb032 at 01:19:44.214Z. This matches the no-daemon fallback-drain mode already owned by issue-84c584957e65 (occurrence appended there).

## Evidence-quality gaps (already owned)
- reason is a mid-JSON tail starting `s":57328.0,` from dev-loop.py:3112 _record_game_build_output detail=line.rstrip()[-240:], read at dev-loop.py:3892; issue-6e08d89b55e3 (occurrence appended).
- wait identity: build_id last published and generation null, no run_id; in-flight attempt only inside reason; issue-9a4503f6ed68 (occurrence appended).
- reviewer-wake launch_result (build_friction.py:250, launched.stdout.strip()[-2000:]) is also a mid-JSON/prompt fragment, not a structured launch result. Same truncation class; noted for issue-6e08d89b55e3 owner review, not filed separately.

## Conclusion
Confirmed real over-budget wait with a healthy build; no duplicate issue filed. No source fix claimed. Smallest next checks remain: (a) owner decision on a scale/dedupe wait policy under issue-136d4553ff92 / issue-9a4503f6ed68; (b) matched pre-build resource baseline under issue-2a5c5da53565; (c) reliable <=5 min fallback drain under issue-84c584957e65.

## Occurrence

### 2026-11-26T01:39:32.076Z — build-wait-reviewer

Terminal observation for this wait: attempt game-dll-20261126T005556909723Z-fa8154 (run 20261126_005302) succeeded; status ok, no_op=false, generation 662, compiled 36/644 (609 skipped), compile 157.766s, link 11.203s, total 194.141s, generation ready ~196.891s. Changed paths src/physics/physical-entity.h/.cpp and src/vehicles/vehicle-system.cpp; 30 TUs were invalidated by physical-entity.h plus 2 missing dependency files; slowest TU server-gamemode.cpp 110.490s, server-npcs 70.310s, vehicle-system 70.153s. Resource sidecar captured 136 samples over 196.813s, mean host CPU 97.950%, peak 100%, minimum available memory 215,298,048 B (~205 MiB), no sampler errors, 3 unavailable fields. The reviewer wake was queued once at 00:56:59.578Z but the final fallback launch was 01:19:47.086Z after codex model_not_found. No duplicate issue, no fix claimed.

Evidence: {"budget_seconds": 60.0, "build_attempt_id": "game-dll-20261126T005556909723Z-fa8154", "build_total_seconds": 194.141, "changed": ["src/physics/physical-entity.h", "src/physics/physical-entity.cpp", "src/vehicles/vehicle-system.cpp"], "compile_seconds": 157.766, "compiled": 36, "generation": 662, "journal": "logs/2026-11-26/20261126_005302/dev-loop/events.jsonl", "link_seconds": 11.203, "resource": {"duration_seconds": 196.813, "mean_host_cpu_percent": 97.95, "minimum_available_memory_bytes": 215298048, "peak_host_cpu_percent": 100.0, "runtime_alignment": "mode_unverified", "sample_count": 136}, "resource_artifact": ".dev/build-timings/game-dll-20261126T005556909723Z-fa8154.resource.json", "run_id": "20261126_005302", "skipped": 609, "slowest_tu": "src/network/server-gamemode.cpp", "slowest_tu_seconds": 110.49, "timing": ".dev/build-timings/game-dll-20261126T005556909723Z-fa8154.json", "wait_elapsed_seconds": 60.564, "wait_id": "game-dll-build:2026-11-26T00:55:56.696Z", "wake": {"attempts": 2, "final_adapter": "opencode", "final_launch_utc": "2026-11-26T01:19:47.086Z", "primary": "codex", "primary_failure": "model_not_found", "queue_to_final_launch_seconds": 1367.508, "queued_utc": "2026-11-26T00:56:59.578Z"}}

## Agent discussion and suggestions

### 2026-11-26T01:40:52.728Z — build-wait-reviewer

Message: `msg-64014f8cef10` (question)

Subject: Occurrence fa8154: 60s wait fired before 110.5s TU on physical-entity.h fanout

Bounded review message (no fix claimed). Wait game-dll-build:2026-11-26T00:55:56.696Z (issue-ac23ab33d184) was real work, not a stall: attempt game-dll-20261126T005556909723Z-fa8154 (run 20261126_005302) succeeded gen 662 in 194.141s, 36 TUs compiled from a physical-entity.h change (30 TUs invalidated by the header + 2 missing dep files). The flat 60s budget fired after only 6/36 TUs; the longest TU src/network/server-gamemode.cpp started ~1s after the wait and took 110.490s. This repeats the class already recorded on your umbrella issue-136d4553ff92. Please confirm 136d remains the home for the scale/dedupe wait-budget decision and that the smallest next check is unchanged (scale/dedupe the game-dll-build wait budget). I appended the review and occurrence to issue-ac23ab33d184; no duplicate issue filed.

# issue-e3eb52067a38: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-no-fix)

### 2026-11-25T00:34:05.657Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (independent wait review; no source edited, no fix claimed)

Observed facts (direct):
- The issue record is complete and readiness=ready with missing=[]: wait_id game-dll-build:2026-11-24T23:09:34.664Z, kind game-dll-build, stage game_build, elapsed 60.501s vs budget 60.0s, build_id 2026-11-24T19:27:02.742428Z, generation 627, source_generation 21, request_ids [2026-11-24T23:09:34.275Z-6eecf5ea:mode3:game]. One over-budget record was created (dev-loop.wait.over_budget created=true, wall 23:10:39.436Z) and exactly one reviewer wake was queued for this wait (dev-loop.wait.reviewer-wake, wall 23:10:39.435Z).
- The wait overlapped real compile execution, not a stalled worker. The failed attempt's changed-source preflight was 77.702s; the only unit started, src/engine/engine-tick-camera.cpp, ran 71.457s and then failed compile. The other five changed sources were cancelled after that failure. The 60.501s crossing therefore occurred roughly 11s before that single TU finished.
- The attempt failed at changed_source_preflight for a real code defect, not for the wait: src/engine/engine-tick-camera.cpp:1186:45 error: 'remainder' is not a member of 'glm'; the compiler suggests std::remainder. The file still contains glm::remainder at line 1186.
- Build totals (game-dll-20261124T230934953855Z-9ace92): total 95.572s; phases changed_source_preflight 77.709s, identity_and_cache 0.077s, toolchain_setup 0.147s. The batch's build note is from opencode-deepseek-v4-1 and the failing file is part of the same in-flight vehicle-camera change.
- Resource sampler (69 samples @1s over 95.484s): mean host CPU 99.978%, peak 100.0%, minimum available memory ~59.7 MB; runtime alignment mode_unverified (server funcounterstrike; client mode stale/unreported). gap_key build-resource-sampling:game-dll-20261124T230934953855Z-9ace92.

Inference / uncertainty:
- The over-budget wait is structural for this workload: one heavy translation unit under a saturated host can exceed the 60s budget by itself. Whether host saturation (CPU ~100%, memory near exhaustion) materially lengthened the 71.457s compile is unknown; matched_prebuild_resource_baseline is unavailable, so no causal claim is made.
- The wait policy has produced roughly 90 open issues titled "Wait over budget: game-dll-build at game_build". That matches this issue's stated risk ("A noisy wait policy could create duplicate friction issues"). The per-wait_id issue creation in build_friction.record_wait_incident is deliberate, so this is a calibration/policy decision, not a code bug.

Contrary evidence:
- Nothing here supports a hung or dead worker: the worker was actively compiling, produced a measured 71.457s TU compile, and failed fast on a real compile error. A "build stuck" claim is not supported.

First divergence (data path): the whole-game build first diverges in the compile plan/preflight when it invokes the compiler on src/engine/engine-tick-camera.cpp and glm::remainder is unresolved. The over-budget wait is downstream noise from that long compile, not an independent failure.

Next discriminating check (smallest):
1. Owner opencode-deepseek-v4-1 replaces glm::remainder at engine-tick-camera.cpp:1186 (glm::mod or std::remainder) and seals one game-DLL attempt; confirm changed_source_preflight passes and dev-loop.game-build records success=true/published for the revision.
2. Wait policy: reuse the 2026-11-24T08:39 build-wait-reviewer suggestion already on issue-7d3dcfd7433b (per-kind/per-workload budget or N-consecutive-occurrence throttle instead of one 60s issue+wake per occurrence) rather than filing further duplicates.

Code defect is reported separately as issue-fb9f3f56f1f1 and the owner has been notified. No source edited; no fix claimed.

Confidence: high for the observed facts and first divergence; medium that budget calibration is the correct policy fix (human/root decision); unknown for any causal CPU-pressure effect.

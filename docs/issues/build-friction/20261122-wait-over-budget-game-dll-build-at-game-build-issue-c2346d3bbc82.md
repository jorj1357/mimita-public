# issue-c2346d3bbc82: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise; root-cause=heavy-single-source-rebuild)

### 2026-11-22T08:39:23.721Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-7c55f10f7f4e), read-only: no source edits, no build requested, no run started.

FACTS
- Wait occurrence: wait_id game-dll-build:2026-11-22T08:11:24.254Z, kind game-dll-build, stage game_build, elapsed 60.568s vs 60.000s budget (+0.568s, ~0.95% over); issue created 2026-11-22T08:12:24.961Z. request_ids [2026-11-22T08:11:24.018Z-487a56c0].
- Pinned attempt: .dev/build-timings/game-dll-20261122T081124311953Z-1d0369.json and .jsonl; started 2026-11-22T08:11:24.312Z, revision 16826795c09bcdea5dfac1470f132d4276cce7bda172938d1d559a213940b8b4, generation 478, source_generation 0, changed_paths [src/npc/npc.cpp]. Change note e0f981bb2e26 by fun-cs-npc-objective: Route failed-plan objective fallback direction into the movement command.
- RESULT: SUCCESS. build.completed 2026-11-22T08:13:01.261Z, exit_code 0, generation 478, published build/game/generation-478/game-g478.dll. total_seconds 96.939, worker_total_seconds 64.943, compiled 2 / skipped 641, jobs 10.
- Phase durations (ms): changed_source_preflight 36354.418, object_cache_scan 6302.409, compile 1793.777 (module-identity-provider only), link 17424.091, publish 599.226, coherence_validation 837.879, toolchain_setup 380.591, vendor_compile 23.573. Slowest compile: src/npc/npc.cpp 35.951s. So the changed .cpp is compiled inside changed_source_preflight, and that heavy TU plus link/scan account for ~97s.
- Progress check: at the 60.568s mark (~08:12:24.9) the builder was inside changed_source_preflight (started 08:11:57.628, completed 08:12:33.983); it was actively compiling, not stalled or wedged.
- Wake: .dev/agent-wake/requests/wake-7c55f10f7f4e.json created 2026-11-22T08:12:26.402Z; events.jsonl wake.queued once at 08:12:26.411Z (WAKE_c451c347...). First adapter (codex) launched 08:12:27.685Z; stale-launch recovered and retried with opencode at 08:25:04, same wake id, no re-queue. Exactly one issue and one reviewer wake for this wait_id.
- Readiness/acceptance: ready=true, missing=[], score 8/8; wait_id, stage, reason, elapsed, budget, request_ids, and evidence are present.
- Evidence defects (persist, not fatal): evidence.build_id is the previously published artifact 2026-11-22T08:00:31.376619Z and generation=null though this attempt ran generation 478; related_build is empty (set to the pinned attempt below). The dev-loop runtime journal path logs/2026-11-22/20261122_011031/dev-loop/events.jsonl still does not exist, so the proof expectation Dev-loop wait event cannot be read; tracked by issue-0498cbef4ef3. The build-timings journal is the available pinned evidence.

ROOT CAUSE
Benign build-latency wait, not a failure or stall. A one-file change to the heavy TU src/npc/npc.cpp (~36s compile) plus changed-source preflight, object-cache scan, and a ~17s whole-game link required ~97s wall time, so the flat 60s game_build budget necessarily trips on a normal rebuild even when the build succeeds. This is the wait-policy aggregation problem already owned by issue-9a4503f6ed68 / issue-e044a931c54b; in-flight identity is issue-136d4553ff92.

VERDICT
Confirmed-wait-policy-noise (real build latency, benign). The issue record and single-wake requirement pass. No new code fix is requested by this review and no runtime_proven behavior proof is claimed or required for this tooling wait.

RECOMMENDED (report only; not claimed by this agent)
1. Derive a stage-aware game_build budget from observed build-timing history (e.g. p95) or gate on lack of progress (stall) instead of flat total elapsed.
2. Aggregate repeated game-dll-build waits by kind+stage signature with occurrences, instead of one issue and wake per attempt.
3. Populate wait evidence (build_id, generation, revision) from the in-flight attempt rather than the last published artifact.
4. Restore the dev-loop runtime journal so the recorded proof expectation can be evaluated.

CONFIDENCE: high on the pinned attempt, progress, success, and wake facts; medium on the preferred budget remedy, which is an owner decision.

## Context

### 2026-11-22T08:39:47.810Z — build-wait-reviewer

related_build: game-dll-20261122T081124311953Z-1d0369
evidence: {'text': 'Pinned attempt .dev/build-timings/game-dll-20261122T081124311953Z-1d0369.json: generation 478, revision 16826795..., success=true, total_seconds 96.939; changed_paths [src/npc/npc.cpp]; note e0f981bb2e26 by fun-cs-npc-objective. Wait progress at 60.568s was inside changed_source_preflight (36354ms), not a stall. Reviewer wake-7c55f10f7f4e queued once; codex retry then opencode retry on same wake id.'}

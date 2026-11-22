# issue-44857c7d5b01: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-22T03:22:27.053Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review of this exact wait occurrence (no source edits, no build, no run, no fix or closure claimed).

Wait identity and detection
- wait_id game-dll-build:2026-11-22T02:41:15.723Z; kind game-dll-build; stage game_build; elapsed 62.086s vs 60.0s budget; over-budget recorded 2026-11-22T02:42:21.132Z.
- logs/2026-11-22/20261122_011031/dev-loop/events.jsonl has exactly one dev-loop.wait.over_budget and one dev-loop.wait.reviewer-wake for this wait_id; wait-id dedupe held (created=true, single issue).

Not a hang; the build legitimately exceeded 60s
- In-flight attempt game-dll-20261122T024115879642Z-8937c9 (generation 442, source_generation 2, revision f9b0d4267a56da7c9d96598e979bc4d1863aaf478576ce24a434b6f5ddf8576f, request auto-2) started 2026-11-22T02:41:15.879Z and succeeded: success=true, total_seconds 412.492, worker_total_seconds 323.573; generation-442 DLL published 02:48:08.
- At the 62s deadline the build was inside changed_source_preflight (135.54s) / compiling src/npc/npc.cpp (135.24s). Dominant phases (.dev/build-timings/game-dll-20261122T024115879642Z-8937c9.json): changed_source_preflight 135.54s, link 144.12s (gnu_linker 144.02s), publish 73.18s (external_dependency_manifest 63.31s), compile wall 29.24s. No stall.
- First divergence: the flat 60s budget is below the normal whole-game DLL build cost. Corpus .dev/build-timings/game-dll-*.json (n=174): p50 136.1s, p75 377.5s, p90 1005.1s; 107/174 (61.5%) exceed 60s. This wait is budget/policy noise, not a build anomaly.

Attribution defect (persists)
- Wait evidence carries build_id=2026-11-22T02:37:06.972018Z and generation=441 (the previously published artifact) plus source_generation=2; the in-flight attempt id and generation 442 appear only in the free-text reason. A reader cannot tie the wait to the attempt without reading the journal. occurrences stays empty because over-budget suppression keys on the same wait_id, so the terminal success is never attached.

Recurrence / duplicate proliferation
- The brain now holds 56 issues titled 'Wait over budget: game-dll-build at game_build' (39 in_review, 17 new) out of 79 build-friction issues. record_wait_incident (devscripts/build_friction.py:131-169) dedupes only by exact wait_id, and wait_id embeds a timestamp, so every >60s occurrence creates a new issue instead of appending an occurrence to a canonical issue. This conflicts with AGENTS 0d duplicate-owner guidance and this issue's own risk note ('A noisy wait policy could create duplicate friction issues.').

Reviewer wake delivery latency (new for this occurrence)
- wake-82085899f063 was created 2026-11-22T02:42:18.871Z but only claimed/launched at 03:04:04.055Z/03:04:04.156Z (adapter opencode, attempts=2) — ~21.8 min late. The wake was queued exactly once; the delay is in wake-queue delivery, not in wait detection.

Readiness/acceptance
- readiness ready=true, missing=[]; reproduction, expected_behavior, acceptance, paths, and proof_expectation are present. The acceptance ('carries wait_id, stage, reason, elapsed, build/run IDs, evidence; one reviewer wake for this occurrence') is met for wait mechanics, except that build_id/generation point at the wrong artifact.

Confidence: high that this is a correct detection with a miscalibrated threshold and a truthful in-flight build. No code fix claimed or owned here. Systemic owner remains build-coordinator on issue-136d4553ff92 (per-class budgets, signature dedupe, active-attempt attribution); wake-delivery latency relates to build-runtime-subagents.

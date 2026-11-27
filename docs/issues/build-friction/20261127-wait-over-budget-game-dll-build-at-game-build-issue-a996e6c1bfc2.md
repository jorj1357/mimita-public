# issue-a996e6c1bfc2: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-over-budget-expected-full-dll-wall)

### 2026-11-27T01:32:29.099Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed the wait occurrence and its build artifacts. Verdict: confirmed over-budget, but expected given natural full-DLL wall time; no build hang or failure.

WAIT (from issue evidence): kind=game-dll-build, stage=game_build, budget=60.0s, elapsed=61.107s, wait_id=game-dll-build:2026-11-27T00:56:44.578Z, source_generation=20, request_ids=[2026-11-27T00:56:44.232Z-d260eaf6:mode3:game].

BUILD OUTCOME (build attempt game-dll-20261127T005644836418Z-43dc27, generation 748): success=true, total_seconds=173.781, completed 2026-11-27T00:59:38.613Z, dll=build/game/generation-748/game-g748.dll. The wait fired at 61.1s while the build was still compiling; the build then ran ~112s longer to a clean publish. So there is no first-divergence defect in this occurrence: the wait event, one build-friction issue (this one), and one reviewer wake (wake-6112a7f36d8c, created 00:57:48Z, launched 01:24:02Z) all exist exactly once.

PHASE EVIDENCE (phase_durations_ms): changed_source_preflight=96328, compile=3563, link=50171, object_cache_scan=3109, publish=531. Slowest compiles: src/actor/player-ai.cpp=96.179s, src/combat/client-collision-world-view.cpp=84.018s at jobs=10; the third TU (generated module-identity-provider.cpp) was 3.409s. Two heavy changed TUs dominate the wall time.

RESOURCE EVIDENCE: mean_host_cpu_percent=99.15, peak=100.0, minimum_available_memory_bytes=258949120 (~247 MiB), 123 samples @1s. Runtime alignment unavailable (mode_match=false, reason=fresh_server_and_client_journal_evidence_not_matched); no matched pre-build baseline and no host commit-charge/limit.

INTERPRETATION: The 60s game-dll-build budget is below the natural ~174s wall time of a 2-heavy-TU relink at 10 jobs, so this wait-over-budget is a policy expectation, not a build defect. Host CPU was saturated and available memory low during the build; that pressure is a plausible contributor to the slow TUs but is not proven causal here.

DUPLICATE FAMILY / NEXT STEP: This is one of a recurring wait-over-budget family (also issue-5d8ebe281ce6 and ~43 open duplicates as reported to build-coordinator in msg-0c08eefc21a3 / thread-a90e92681659); do not create further duplicates. The actionable owner is the budget-policy decision (build-coordinator): whether the 60s budget should scale with changed-TU cost and job count, or whether only stage-specific waits (link/preflight) should be flagged. This occurrence itself needs no code fix.

PROOF: Acceptance met - wait_id, stage, reason, elapsed, build/run IDs, and evidence are present; exactly one reviewer wake is queued for this occurrence. Runtime/behavior proof is not applicable to a build-friction wait; the pinned build-timings artifacts above are the evidence.

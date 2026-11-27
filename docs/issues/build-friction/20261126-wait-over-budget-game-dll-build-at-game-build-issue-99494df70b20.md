# issue-99494df70b20: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T22:51:38.697Z — build-wait-reviewer

related_build: game-dll-20261126T192711707688Z-92a892 (generation 721, revision 73da78d3c7232ae5157198a0bc05e858815fd9f9fde4f48c10cbfb083c83932c)
related_run: 20261126_190324

## Review (confirmed-true-positive)

### 2026-11-26T22:51:44.569Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (activation wake-ddb6deb5412c), read-only: no source edit, no build/test request, no runtime/behavior claim, no issue close.

VERDICT: confirmed true positive. A real whole-game DLL compile exceeded the 60 s wait budget; it was not wedged. The wait record, the single build-friction issue, and the single reviewer wake are correct. The underlying slow build is a genuine header-fanout cost, not a false alarm.

WAIT UNDER REVIEW (pinned)
- wait_id: game-dll-build:2026-11-26T19:27:11.618Z
- kind/stage: game-dll-build / game_build
- elapsed at record: 61.06 s vs budget 60.0 s
- started_utc: 2026-11-26T19:27:11.618Z
- source_generation: 6
- request_ids: [2026-11-26T19:27:11.399Z-4bb4d8c2:mode3:game]
- wait record's build_id: 2026-11-26T17:45:43.947108Z (previous published artifact); generation: null
- actual in-flight attempt: game-dll-20261126T192711707688Z-92a892, generation 721, revision 73da78d3c7232ae5157198a0bc05e858815fd9f9fde4f48c10cbfb083c83932c

BUILD OUTCOME (direct artifacts)
- .dev/build-timings/game-dll-20261126T192711707688Z-92a892.jsonl
  - build.started 19:27:11.823Z; build.completed 19:30:21.048Z; elapsed_ms 189218 (189.2 s); exit_code 0; no_op false; dll build\game\generation-721\game-g721.dll
  - phase timings: manifest_snapshot 2.234 s; source_snapshot 0.296 s; object_cache_scan 3.125 s; compile 171.672 s; link 4.734 s; coherence_validation 0.656 s; publish 0.454 s. Compile is 171.7 s of 189.2 s (90.8%).
  - 89 translation units compiled; sum of per-TU compile seconds 1607.3 s wall-parallel.
  - cache invalidation at compile start: 60 TUs "dependency newer than object: ...\src\network\server.h"; 26 TUs "...\src\physics\physical-entity.h"; 2 "missing or invalid dependency file". Both headers are in build note 0cbde5b2a491 (agent rocket-league-aerial: vehicle-networking.cpp, server.h, physical-entity.h/.cpp), so this was a header-fanout rebuild.
- .dev/build-timings/game-dll-20261126T192711707688Z-92a892.resource.jsonl: sample_count 149; duration 189.391 s; mean_host_cpu_percent 92.522; peak 100.0; minimum_available_memory_bytes 39149568 (~37.3 MiB). unavailable_fields: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation.

WAIT RECORD / WAKE / DEDUPE (pinned)
- logs/2026-11-26/20261126_190324/dev-loop/events.jsonl line 85: dev-loop.wait.over_budget, wait_id game-dll-build:2026-11-26T19:27:11.618Z, created=true, issue issue-99494df70b20, wall 19:28:27.628Z.
- Same file line 84: dev-loop.wait.reviewer-wake, queued=true, adapter codex, wake-ddb6deb5412c, wall 19:28:27.626Z.
- Exactly one issue references this wait_id (this issue); no duplicate build-friction issue exists for it.
- Nuance: the wait payload's build_id/generation describe the previously published artifact (2026-11-26T17:45:43.947108Z; generation null), not the in-flight attempt. The true attempt id/generation/revision are present in the same record's reason string and are captured above. This is a labeling defect, not a detection failure (already tracked in the wait-identity/reason thread).

ROOT-CAUSE ATTRIBUTION (observed vs inferred)
- Observed: the compile phase alone (171.672 s) is 2.86x the 60 s budget, driven by an ~89-TU header-fanout rebuild after server.h/physical-entity.h edits. The wait is a true positive, not a stall.
- Inferred: a single flat 60 s budget cannot distinguish a normal header-fanout rebuild from a wedged build; every such header edit will deterministically create an issue + reviewer wake. 24 game-dll wait-over-budget issues were created on 2026-11-26 alone (224 still in_review all-time), matching the issue's documented "noisy wait policy" risk.
- Unknown: whether the ~92.5% host CPU / ~37 MiB free memory slowed compile further; there is no matched prebuild baseline (unavailable_fields), so host pressure cannot be quantified. Do not claim CPU pressure as cause.

ACCEPTANCE
- Issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence: PASS (readiness ready=true, missing=[]).
- A reviewer wake is queued exactly once for this wait occurrence: PASS (one reviewer-wake event, wake-ddb6deb5412c; one referencing issue).

CROSS-LINK / OWNERSHIP
- Recurrence and the flat-budget policy belong to issue-7d3dcfd7433b "Measure every dev-loop wait stage and reduce avoidable latency" (in_review; owner launch-latency-investigation) and issue-e044a931c54b "Waiting is the enemy..." (assigned; build-runtime-subagents). No code fix claimed here; this reviewer does not own those paths.
- Leave this issue open; it is a true-positive occurrence. Consolidating/deduping the per-wait issue stream is a policy decision for the umbrella owners, not this review.

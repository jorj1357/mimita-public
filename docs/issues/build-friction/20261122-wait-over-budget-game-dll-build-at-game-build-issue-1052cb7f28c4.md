# issue-1052cb7f28c4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T07:59:55.664Z — build-wait-reviewer

related_build: game-dll-20261122T071752060151Z-f50f45

## Review (confirmed-wait-policy-noise)

### 2026-11-22T08:02:42.767Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-f02b52d937f6), read-only: no source edits, no build, no run started.

FACTS
- Wait occurrence: wait_id game-dll-build:2026-11-22T07:17:51.971Z, kind game-dll-build, stage game_build, elapsed 60.306s vs budget 60.000s (0.306s / 0.51% over).
- Pinned build: .dev/build-timings/game-dll-20261122T071752060151Z-f50f45.json (attempt started 2026-11-22T07:17:52.061Z). success=true, total_seconds=104.204, worker_total_seconds=67.528, compile_seconds=32.952, link_seconds=26.058, source_count=642, compiled=2, changed=[src/npc/npc-navigator.cpp], generation=471, source_generation=11, note by fun-cs-npc-objective. This was a normal, successful incremental game-DLL build; the wait crossed 60s at ~58% of its total duration. No wedge, stall, or failure.
- Budget owner: devscripts/dev-loop.py:109 WAIT_ISSUE_SECONDS default 60s; check_wait_budget keys game waits at devscripts/dev-loop.py:3520-3536. build_friction.record_wait_incident keys uniqueness on wait_id only (devscripts/build_friction.py:143-202), so every distinct game_build_started_utc creates a new issue.
- Flood measurement: 102 open "Wait over budget" issues (79 in_review, 23 new, 0 done/cancelled) in the shared brain, nearly all "game-dll-build at game_build". This is systemic policy noise, not a per-occurrence defect.
- Evidence-quality defect in this record: the issue build_id (2026-11-22T02:37:06.972018Z = latest_successful_build_id) is ~5h stale relative to the wait, and evidence generation=470 while the in-flight attempt produced generation 471. The wait dict uses self.latest_build / self.game_build_generation instead of the in-flight attempt id/generation, so the "build/run IDs" the acceptance requires are not reliably tied to the occurrence.

ROOT CAUSE
A single flat 60s budget is below the normal duration of a full candidate game-DLL build (~104-117s observed), so every ordinary build trips a "friction" issue and a reviewer wake. Combined with per-wait_id dedupe, recurring identical waits fan out into duplicate issues. This contradicts issue-9a4503f6ed68 acceptance ("a second scan does not duplicate the issue") and the risk recorded on this very issue ("A noisy wait policy could create duplicate friction issues").

VERDICT
Confirmed legitimate/benign wait; confirmed wait-policy miscalibration; confirmed duplicate-issue flood. The wait did NOT represent a build problem. No build-runtime defect is asserted here; no runtime_proven behavior proof is required for this tooling-policy finding and none is claimed. No code edited by this review.

RECOMMENDED FIX (report only; claim separately)
1. Stage-aware budget: game_build/game-dll-build budget derived from observed successful build duration (e.g. >= p95 or ~180s) while keeping queue/setup stages tight.
2. Dedupe by stable signature (kind+stage) and append occurrences instead of creating a new issue per wait_id; retain every occurrence.
3. Populate wait evidence from the in-flight attempt (attempt id, generation, revision, slowest phase) rather than stale latest_successful_build_id / game_build_generation.
Track at canonical umbrella issue-9a4503f6ed68 (owner wake-proof-wait) with wait-latency measurement at issue-7d3dcfd7433b (owner launch-latency-investigation). Do not lower the budget by skipping build correctness.

CONFIDENCE: high on the benign nature and the policy/flood cause (pinned artifacts + budget code); high that this occurrence was not a build failure.

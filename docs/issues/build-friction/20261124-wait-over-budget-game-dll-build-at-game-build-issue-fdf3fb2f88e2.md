# issue-fdf3fb2f88e2: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-24T22:29:55.060Z — build-wait-reviewer

related_build: game-dll-20261124T212309428914Z-4ffd27

## Review (confirmed-recurring-known-defect)

### 2026-11-24T22:32:33.169Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded read-only review by build-wait-reviewer (wake-5899733e11d5), 2026-11-24T21:24-22:30Z. No source edit, build request, claim, or close performed.

ACCEPTANCE CHECK for this per-occurrence issue (wait_id game-dll-build:2026-11-24T21:23:08.791Z)
- wait_id/stage/reason/elapsed: present (game_build, 60.65s, budget 60.0s, revision 021cb811..., sources=644). PASS.
- build/run IDs: present but MISALIGNED. Evidence carries build_id=2026-11-24T19:27:02.742428Z (last completed game build / last successful EXE artifact) and generation=621, while the waited-on in-flight attempt was game-dll-20261124T212309428914Z-4ffd27, generation 623. source_generation=23 is correct. related_build/related_run were empty; reviewer set related_build to the correct attempt. Root: dev-loop.py:3896 uses self.latest_build and dev-loop.py:3897 uses self.game_build_generation, which is assigned only on completion (dev-loop.py:3259), so mid-build evidence cannot name the waited-on generation. PARTIAL/known defect.
- exactly one reviewer wake for this wait occurrence: PASS. .dev/agent-wake/events.jsonl has one wake.queued line (wake-5899733e11d5); dev-loop journal line 94 is the single wait.reviewer-wake.
- exactly one issue for this wait_id: PASS (one row for this wait_id in the store).

OBSERVED FACTS
- The waited-on build was healthy, not hung: dev-loop events lines 96-99 show src/physics/physical-entity.cpp compiled in 98.206s (changed_source_preflight 98.275s), link 24.684s; result status=ok generation 623, total_seconds=203.352; generation ready duration 214.216s. The perimeter fired at 60.65s mid-preflight, ~143s before result. Budget 60s < a single changed-TU preflight, so the trip is expected under current policy.
- Attempt resource sampling: mean host CPU 99.991%, peak 100%, minimum available memory 34,410,496 B (~32.8 MiB). This is the same pressure class cpu-optimization-loop already reported to root-resource-loop/root-resource-profiler (Gen623 failure pressure); no new resource issue created to avoid duplication.
- Flood is aggregate, not per-wait: 179 issues currently share the title "Wait over budget: game-dll-build at game_build" (162 in_review, 16 new, 1 assigned). record_wait_incident dedupes only by wait_id (build_friction.py:131-169), so each attempt mints another issue. This is the risk named on this issue and is owned under issue-9a4503f6ed68 (owner wake-proof-wait); occurrence appended there.

EVIDENCE
- logs/2026-11-24/20261124_205956/dev-loop/events.jsonl L93-99 (game-build.started, wait.reviewer-wake, wait.over_budget, TU, result, ready)
- .dev/build-timings/game-dll-20261124T212309428914Z-4ffd27.json (success, total_seconds 203.352, phase durations), .jsonl, .resource.json
- .dev/agent-wake/events.jsonl (1 wake for this issue), store check (1 issue for this wait_id, 179 same-title)

INFERENCE / CONFIDENCE
- High confidence: wait identity misalignment (generation/build_id stale) and healthy build outcome; the per-occurrence acceptance holds for one-issue/one-wake. Medium/known: resource pressure is informational and separately owned.

NEXT STEP (not claimed)
- issue-9a4503f6ed68 already carries the reviewer recommendation: aggregate repeated waits by (kind, stage) signature as occurrences on one issue (mint a new issue only when signature/owner phase changes), and carry build_attempt_id/run_id in wait evidence. No shared wait-authority change without human confirmation.

# issue-b73ec4904f0c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass)

### 2026-11-26T19:08:01.753Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent review of issue-b73ec4904f0c (build-friction: game-dll-build wait at game_build over budget). I verified all fields and evidence against the pinned dev-loop journal; issue readiness returns ready=true, missing=[], and no duplicate issue references the wait_id.

EVIDENCE (pinned)
- Journal: logs/2026-11-26/20261126_173922/dev-loop/events.jsonl
- wait_id: game-dll-build:2026-11-26T17:42:30.447Z
- build_attempt_id: game-dll-20261126T174230593869Z-fc2392; generation=710; revision=0ab32c4dd32155ae9d9d9659d5f4ae16779bada235af44a86f76dbfe75e9be97; source_generation=3
- dev-loop.game-build.started 17:42:37.548Z changed_paths = src/network/vehicle-networking.cpp, src/physics/physical-entity.cpp, src/physics/physical-entity.h, src/render/post-fx.h (two shared headers -> fanout)
- 57 dev-loop.game-build.file compile events at generation 710; dev-loop.game-build.result status=ok generation=710 total_seconds=156.594 at 17:45:07Z

ACCEPTANCE CHECK
- "issue carries wait_id, stage, reason, elapsed, build/run IDs, evidence": PASS. Issue evidence JSON has wait_id, kind=game-dll-build, stage=game_build, elapsed_seconds=60.572, budget_seconds=60.0, build_id=2026-11-26T17:36:42.013401Z, source_generation=3, generation=710, request_ids=[2026-11-26T17:42:30.250Z-ed75280e:mode3:game], started_utc, and reason (truncated compile-progress blob).
- "reviewer wake queued exactly once": PASS. Exactly one dev-loop.wait.over_budget event and exactly one dev-loop.wait.reviewer-wake event for this wait_id; the wake fields are queued=true, adapter=codex, launch_error="", wake=wake-ac40b3f26f39.

CAUSE / NOISE ASSESSMENT
The wait is genuine, not a policy defect: elapsed 60.572s was only 0.572s over the 60s budget, but the underlying build was a real header-fanout rebuild of 57 translation units that ran to 156.594s (legitimate shared-header cost from physical-entity.h and post-fx.h). The wait policy correctly caught a real fanout build and produced one issue + one wake; it did not spam duplicates.

CONCLUSION
Acceptance satisfied; no code fix required for the wait-detection/wake machinery. Remaining cost is the inherent fanout rebuild time (tracked separately by build-cause/fanout work). Reporting reviewer evidence only; not editing source, not closing, not claiming runtime proof.

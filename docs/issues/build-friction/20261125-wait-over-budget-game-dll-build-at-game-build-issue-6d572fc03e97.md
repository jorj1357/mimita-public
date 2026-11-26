# issue-6d572fc03e97: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-real-over-budget-build)

### 2026-11-26T00:13:14.605Z — build-wait-reviewer

Reviewer: build-wait-reviewer

VERDICT: Confirmed real over-budget build wait; not a stall; build succeeded.

REVIEWER: build-wait-reviewer (activation wake-471f743f065f)
WAIT: kind=game-dll-build stage=game_build elapsed=68.915s budget=60.0s
wait_id=game-dll-build:2026-11-25T23:50:11.848Z
started_utc=2026-11-25T23:50:11.848Z
request_id=2026-11-25T23:50:11.513Z-05c594a1:mode3:game
build_attempt_id=game-dll-20261125T235012012371Z-a9ad0a
revision=56c4855e1ec9fa3540a8479c7699d2a34777462e67b26c00a5c47c34e885e9e9
source_generation=2  generation=657

EVIDENCE (observed):
- Dev-loop journal logs/2026-11-25/20261125_234424/dev-loop/events.jsonl has dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake for this exact wait_id at wall_time 23:51:30Z / 23:51:29Z.
- Build timing .dev/build-timings/game-dll-20261125T235012012371Z-a9ad0a.json: success=true, total_seconds=134.64, compile=94.891s (21 TUs, compile_sum=842.8s, compile_max=80.8s map-editor.cpp), link=10.485s; dll generation-657 published 23:52:26Z.
- Trigger: header change src/vehicles/vehicle-types.h (+ src/vehicles/vehicle-system.cpp, root-resource-loop note 9759e055708c) caused a ~20-TU header-fanout rebuild, pushing wall compile past the flat 60s total-elapsed threshold while the build was healthy.

RESOURCE PRESSURE (observed, same attempt):
- resource summary .dev/build-timings/game-dll-20261125T235012012371Z-a9ad0a.resource.json: host logical CPUs=12; mean host CPU 98.869%, peak 100.0%; minimum_available_memory_bytes=107,323,392 (~102 MiB); duration 134.625s; 90 samples @1s.
- Sampler reported runtime_alignment=unavailable (fresh server/client mode/workload not matched), so live-game workload during the build is UNKNOWN.

DEDUPE / WAKE ACCEPTANCE:
- Exactly one issue for this wait_id (no duplicate) and exactly one reviewer wake (wake-471f743f065f). record_wait_incident uniqueness on wait_id holds; af48c43ce6c5 (00:03:44Z) is a separate occurrence, not a duplicate.

KNOWN CAVEATS (already tracked, not duplicated here):
- Flat 60s total-elapsed threshold vs normal incremental game-DLL builds, plus wait.reason being a truncated GAME_FILE JSON tail instead of a human-readable reason, are the previously documented systemic defects in issue-a236165fa6fc.
- Evidence build_id=2026-11-25T23:04:22.955793Z is the last published build, not the in-flight attempt; the accurate attempt id is only inside the reason fragment. related_build/related_run are empty. This is the same reason/identity quality gap.

CONCLUSION: Do not treat this as a hang or failure. No source edits made. Issue remains open as one occurrence of the systemic slow-incremental-build friction; resource-pressure details cross-linked to the build-latency/resource work rather than a new issue.

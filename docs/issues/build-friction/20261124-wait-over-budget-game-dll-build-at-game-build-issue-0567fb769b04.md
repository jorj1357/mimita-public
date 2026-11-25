# issue-0567fb769b04: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-25T00:28:19.447Z — build-wait-reviewer

related_build: game-dll-20261124T222855538088Z-1d20e7
related_run: 20261124_221014

## Review (acceptance-met-identity-gap)

### 2026-11-25T00:29:11.740Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-bafa65966226). Read-only: no source edited, no build/run started, no fix or proof claimed.

OBSERVED (run 20261124_221014, journal logs/2026-11-24/20261124_221014/dev-loop/events.jsonl):
- L55 dev-loop.wait.reviewer-wake and L56 dev-loop.wait.over_budget at wall_time 2026-11-24T22:30:02.306Z, wait_id game-dll-build:2026-11-24T22:28:54.886Z, created=true, exactly one wake (wake-bafa65966226, adapter codex). The single-wake-per-wait contract holds.
- The waited-on attempt is game-dll-20261124T222855538088Z-1d20e7, request 2026-11-24T22:28:53.757Z-1f524960:mode3:game, source_generation 6, generation 627. It succeeded: dev-loop.game-build.result and game-generation.ready at 22:33:50/22:33:58, total_seconds=300.575, worker_total_seconds=270.433.
- Phase durations (.dev/build-timings/game-dll-20261124T222855538088Z-1d20e7.json): changed_source_preflight 186.721s (slowest TUs src/main-systems.cpp 186.582s, src/physics/physical-entity.cpp 167.413s), compile wall 193.327s (compile_sum 360.586s over jobs=10), link 65.521s (link.lld_linker 65.651s), publish 7.833s.
- The flat 60s budget (WAIT_ISSUE_SECONDS, devscripts/dev-loop.py:114-115) fired ~126s before the result, during a single-TU preflight that alone exceeds the budget. This wait is structural for a changed-TU build, not a stuck worker.

EVIDENCE GAP (issue acceptance): the wait evidence carries build_id=2026-11-24T19:27:02.742428Z and generation=626; both are the previously published game build, not the in-flight generation 627. Root cause: check_wait_budget reads self.latest_build (last published) and self.game_build_generation, which is assigned only on completion (devscripts/dev-loop.py:3259), while started_utc is set at build start (:3166). wait_id + request_ids + source_generation + reason(revision) do resolve the attempt, but the issue record's "build/run IDs" do not by themselves. related_build/related_run were empty and were set by this review to the correct attempt/run. Same stale-identity defect already tracked on issue-9a4503f6ed68; not a new issue.

RECURRING (not this occurrence's defect): per-wait_id uniqueness mints one issue and one wake per build attempt. Store query found 227 build-friction issues (0 done/cancelled; 47 created 2026-11-24), almost all "Wait over budget: game-dll-build at game_build". Aggregation belongs to issue-9a4503f6ed68; this review appended an occurrence there instead of creating a duplicate.

RESOURCE CONTEXT (direct observation, one build window): .resource.json for the attempt: mean host CPU 99.977%, peak 100.0% over 199 samples @1s; minimum available memory 2,281,472 B (~2.18 MiB, single-sample minimum); live client pid 26492 at 35.4 FPS, 38/38 frames over 4.167ms budget, 19 remote NPCs. Inference: the build ran under heavy host CPU pressure with near-exhausted sampled free memory. Whether memory pressure caused the 186s preflight is not established (no prebuild baseline; unmatched_prebuild_resource_baseline is in unavailable_fields). Cross-linked to issue-ca5eb16d9e21.

CONTRARY EVIDENCE: the build completed successfully and published generation 627, so this occurrence is slow-but-healthy, not a failure; the one-issue/one-wake invariant held exactly with no duplicate issue or wake for this wait_id.

NEXT DISCRIMINATING CHECK (owner decision, not done here): (1) carry the in-flight build_attempt_id, generation, and run_id in wait evidence and populate related_build/related_run at creation; (2) adopt a stage-aware or stall-progress game_build budget (>= observed p95, ~300s here) and coalesce per (kind,stage) per day. Evaluate against issue-9a4503f6ed68's measurable target.

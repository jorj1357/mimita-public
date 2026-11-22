# issue-40ebd14d5cf4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T14:47:13.884Z — build-wait-reviewer

related_build: game-dll-20261122T135736380483Z-d10b1b

## Review (confirmed-with-evidence-gaps)

### 2026-11-22T14:47:52.275Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed against the dev-loop build timings and the agent-wake journal. No source, config, or build-script edits; no compiler/build.py/module worker/second dev-loop invoked; no new runtime; no file leases; no duplicate issue created.

Occurrence: wait_id game-dll-build:2026-11-22T13:57:36.293Z, stage game_build, elapsed 60.863s vs 60.0 budget, request 2026-11-22T13:57:36.183Z-65c1354b.

Matching attempt .dev/build-timings/game-dll-20261122T135736380483Z-d10b1b.json: started 13:57:36.380Z, completed 13:59:34.406Z, success, generation 496, total 118.006s (worker 104.118s). At the 60.863s mark the build was mid compile/link and needed about 57s more; this is a real latency overrun, not a stall or a released worker.

Cost: changed_source_preflight 46.877s, link 35.335s (lld), compile 8.835s (worker compile_sum 55.695s), object_cache_scan 4.829s, publish 1.720s. Slowest TUs: src/effects/zombie-tower-effects.cpp 46.504s (compiled) and module-identity-provider 8.750s. One changed source, jobs=10, cache b10e3a31b3ae8637.

Wake: exactly one wake.queued for wake-a37dc2879375 at 13:58:39.668Z, so the acceptance clause that the reviewer wake is queued exactly once is satisfied. The codex primary adapter claimed/launched at 13:58:41.859Z and then exited; wake.retry switched the request to opencode at 14:22:49.103Z; opencode claimed/launched at 14:24:02.589Z/14:24:03.211Z. Dispatch delay about 25.4 minutes. This is a recurrence of issue-84c584957e65 (late reviewer dispatch), recorded as an occurrence there rather than as a new duplicate issue.

Evidence gaps (reported, not fixed):
- top-level generation is null; the actual attempt generation is 496.
- build_id is the prior published artifact 2026-11-22T08:00:31.376619Z, not the in-progress attempt id game-dll-20261122T135736380483Z-d10b1b.
- no run_id is present in the wait evidence.
- the original wait_id 2026-11-22T13:57:36.293Z has no dev-loop.wait.over_budget event in any run journal. The only related dev-loop.wait.reviewer-wake event (run 20261122_141131, 14:24:03Z) is mislabeled with wait_id game-dll-build:2026-11-22T14:22:57.758Z.

Systemic findings (owner decision; not implemented here):
- The fixed 60s budget is shorter than a normal heavy-TU incremental build: this successful build was 118.0s and g496 worker time was 104.1s. See issue-136d4553ff92.
- build_friction.record_wait_incident dedups only on exact wait_id, so every over-60s build files another near-identical issue: 56 game-dll-build wait issues for 2026-11-22 and 103 total. Budget/dedupe policy change needs an owner decision.

Verdict: confirmed with evidence gaps. Valid, evidence-backed over-budget wait; wake queued once; dispatch was late (known issue-84c584957e65). No code fix claimed.

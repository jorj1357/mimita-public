# Over-budget game-DLL wait review 40ebd14d (build-wait-reviewer)

Date: 2026-11-22 ~14:48Z
Agent: build-wait-reviewer
Context: wake-a37dc2879375
Issue: issue-40ebd14d5cf4 (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, or build-script edits were made. No
compiler, build.py, module worker, or second dev-loop was invoked. No new runtime
was started, no files were leased, and no duplicate issue was created.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T13:57:36.293Z`, stage `game_build`,
  elapsed 60.863s, budget 60.0s, request `2026-11-22T13:57:36.183Z-65c1354b`,
  source_generation 0, generation null.
- Matching attempt `.dev/build-timings/game-dll-20261122T135736380483Z-d10b1b.json`:
  started 13:57:36.380Z, completed 13:59:34.406Z, generation 496, success,
  total 118.006s (worker 104.118s).
- At the 60.863s mark the attempt was mid compile/link and needed about 57s more;
  a real latency overrun, not a released or stalled worker.
- Cost: `changed_source_preflight` 46.877s, link 35.335s (lld), compile 8.835s
  (worker compile_sum 55.695s), object_cache_scan 4.829s, publish 1.720s.
  Slowest TUs: `src/effects/zombie-tower-effects.cpp` 46.504s (compiled) and
  module-identity-provider 8.750s. One changed source, jobs=10.
- Wake `.dev/agent-wake/events.jsonl`: exactly one `wake.queued` for
  wake-a37dc2879375 at 13:58:39.668Z; codex claimed/launched at 13:58:41.859Z
  and exited; wake.retry switched to opencode at 14:22:49.103Z; opencode
  claimed/launched at 14:24:02.589Z/14:24:03.211Z. Queued once, dispatch delayed
  about 25.4 minutes (same class as issue-84c584957e65).

## Findings

1. Legitimate latency wait, not a stall: the successful build needed 118.0s.
2. Evidence gaps (reported, not fixed):
   - top-level `generation` is null; actual attempt generation is 496.
   - `build_id` is the prior published artifact 2026-11-22T08:00:31.376619Z,
     not the in-progress attempt id.
   - no `run_id` in the wait evidence.
   - the original wait_id has no `dev-loop.wait.over_budget` event in any run
     journal; the only related `dev-loop.wait.reviewer-wake` event (run
     20261122_141131, 14:24:03Z) is mislabeled with wait_id
     `game-dll-build:2026-11-22T14:22:57.758Z`.
3. Fixed 60s budget is shorter than a normal heavy-TU incremental build (this
   success 118.0s; g496 worker 104.1s). See systemic issue-136d4553ff92.
4. `build_friction.record_wait_incident` dedups only on exact wait_id, so every
   >60s build files another near-identical issue: 56 game-dll-build wait issues
   for 2026-11-22 and 103 total. Budget/dedupe policy change needs an owner
   decision; not implemented here.

## Claims

- Source evidence: review-only (devscripts/dev-loop.py, devscripts/build_friction.py,
  build timings, agent-wake events).
- Build evidence: attempt game-dll-20261122T135736380483Z-d10b1b, generation 496,
  118.006s, success.
- Runtime evidence: agent-wake events only; no gameplay runtime required.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `confirmed-with-evidence-gaps` to
  issue-40ebd14d5cf4; state `in_review`; pinned `related_build` to the exact
  attempt.
- Appended one occurrence to issue-84c584957e65 (late reviewer dispatch) with the
  wake sequence and delay evidence instead of creating a duplicate issue.
- No code fix claimed.

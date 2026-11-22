# Over-budget game-DLL wait review (build-wait-reviewer)

Date: 2026-11-22 ~08:00Z
Agent: build-wait-reviewer
Context: wake-cc77e9236913
Issue: issue-37dde12f63ff (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, or build-script edits. No compiler,
build.py, module worker, or second dev-loop invoked. No runtime started. This is
one more occurrence of the established build-friction wait signature, so it was
reviewed as a recurrence, not a new analysis.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T07:27:50.398Z`, stage `game_build`,
  elapsed 60.04s vs 60.0s budget, generation 471, source_generation 13.
- Actual in-flight attempt `.dev/build-timings/game-dll-20261122T072750493926Z-7ac0b3.json`:
  generation 472, changed `src/npc/npc.cpp`, success, total 191.924s, completed
  2026-11-22T07:31:02.431Z.
- Cost: changed-source preflight 85.144s (single TU `src/npc/npc.cpp` compile
  84.847s), link 66.853s (gnu_linker 66.792s), object-cache scan 6.103s,
  publish 0.758s.
- Wake `.dev/agent-wake/requests/wake-cc77e9236913.json`: created
  07:28:52.057Z, `attempts=2`, launched 07:45:44.464Z. Exactly one wake for
  this wait occurrence.

## Findings

1. Not a new fault. The wait fired during normal build progress, not a stall:
   both the changed-source compile preflight and the link each exceed the flat
   60s budget by themselves. The build then succeeded in 191.924s.
2. Acceptance passes for the wait record: wait_id, stage, reason, elapsed,
   request_ids, and evidence are present; readiness ready=true, missing=[];
   exactly one reviewer wake was queued.
3. Identity defect persists: evidence records generation 471 and build_id
   `2026-11-22T02:37:06.972018Z` (the previous published artifact) while the
   in-flight attempt was generation 472. `devscripts/dev-loop.py:2967` assigns
   `game_build_generation` only on completion, so `check_wait_budget` at
   `devscripts/dev-loop.py:3533` reads the stale previous value.
4. Journal gap persists: `.dev/state.json` `events_path`
   `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` does not exist, so
   there is no on-disk `dev-loop.wait.over_budget` record for this wait_id
   (open issue-0498cbef4ef3).
5. Policy noise: 114 open build-friction issues, 87 of them
   `game-dll-build at game_build`. `devscripts/build_friction.py:131-169` keys
   uniqueness on wait_id, so each occurrence mints a new issue and wake.

## Claims

- Source evidence: none (review-only).
- Build evidence: attempt game-dll-20261122T072750493926Z-7ac0b3, generation 472,
  success, 191.924s.
- Runtime evidence: none required for this wait-policy issue.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `confirmed-wait-policy-noise` to
  issue-37dde12f63ff (state now in_review).
- Appended recurrence evidence to owner issue-9a4503f6ed68
  (wake-proof-wait).
- No code fix claimed. Owners: issue-9a4503f6ed68 / issue-e044a931c54b
  (stage-aware budget, aggregation), issue-136d4553ff92 (stage accounting /
  in-flight identity), issue-0498cbef4ef3 (restore dev-loop journal events).

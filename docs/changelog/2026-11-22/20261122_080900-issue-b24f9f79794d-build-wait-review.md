# Build-wait review for issue-b24f9f79794d (build-wait-reviewer)

Date: 2026-11-22 ~08:10Z
Agent: build-wait-reviewer
Wake: wake-2f85be04912d
Issue: issue-b24f9f79794d (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source or build-script edits; no compiler, build.py,
module worker, or second dev-loop invoked; no runtime started. Appended one review
and evidence to the issue and recorded this changelog.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T07:44:40.382Z`, stage `game_build`,
  elapsed 60.593s vs 60.0s budget.
- Pinned attempt `.dev/build-timings/game-dll-20261122T074440480025Z-9387dd.json`:
  revision `cfae2085...`, generation 473, changed `src/npc/npc.cpp` and
  `src/npc/npc.h`, notes by fun-cs-npc-objective. `success=false`, total 456.908s,
  compile 352.853s, 92 TUs compiled, error
  `src/network/server-npcs.cpp:904: 'class Npc' has no member named 'hasLoggedDash'`.
- Retry `game-dll-20261122T075217617830Z-a15284` failed with the same error
  (112.196s). Fix attempt `game-dll-20261122T075410208033Z-f3ff3f` removed the
  stale reference from `src/network/server-npcs.cpp` and succeeded in 190.312s,
  generation 476, published 07:57:20Z.
- Wake `wake-2f85be04912d`: one request created 07:45:42Z; codex adapter exited,
  opencode retry claimed/launched 07:55:16Z. Exactly one wake for the wait_id.

## Findings

1. The wait was genuine: a header-touching change recompiled 92 TUs and the same
   change set also left a removed field referenced in `server-npcs.cpp`, so the
   build could not finish under 60s and failed after 456.9s.
2. Issue fields are complete (ready=true, missing=[]), and the one-wake rule
   passes. The immediate compile defect is already fixed in source; no new code
   fix is requested by this review.
3. Known evidence defects persist: the wait evidence build_id is the previous
   published artifact and generation is null, because the in-flight attempt
   identity is not used. `related_build` is empty.
4. The dev-loop runtime journal path from `.dev/state.json`
   (`logs/2026-11-22/20261122_011031/dev-loop/events.jsonl`) does not exist, so
   the "Dev-loop wait event" proof expectation cannot be evaluated; tracked by
   issue-0498cbef4ef3.

## Claims

- Source evidence: none (review only).
- Build evidence: attempts 9387dd (failed), a15284 (failed), f3ff3f (success,
  generation 476) from `.dev/build-timings/`.
- Runtime evidence: none required for this wait-policy/issue review.
- Human acceptance: not performed.

## Handoff

- Appended verdict `confirmed-build-failure-wait` to issue-b24f9f79794d
  (state now in_review).
- Systemic wait-policy ownership remains with issue-9a4503f6ed68 /
  issue-e044a931c54b; in-flight identity with issue-136d4553ff92; missing journal
  with issue-0498cbef4ef3. No code fix claimed.

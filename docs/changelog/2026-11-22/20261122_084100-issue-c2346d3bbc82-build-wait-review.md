# Build-wait review for issue-c2346d3bbc82 (build-wait-reviewer)

Date: 2026-11-22 ~08:39Z
Agent: build-wait-reviewer
Wake: wake-7c55f10f7f4e
Issue: issue-c2346d3bbc82 (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source or build-script edits; no compiler, build.py,
module worker, or second dev-loop invoked; no runtime started. Appended one review
and one context/evidence record to the issue and recorded this changelog.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T08:11:24.254Z`, stage `game_build`,
  elapsed 60.568s vs 60.0s budget; request id
  `2026-11-22T08:11:24.018Z-487a56c0`.
- Pinned attempt `.dev/build-timings/game-dll-20261122T081124311953Z-1d0369.json`:
  revision `16826795...`, generation 478, changed `src/npc/npc.cpp`, note
  `e0f981bb2e26` by fun-cs-npc-objective. `success=true`, total 96.939s,
  compile (changed TU, inside preflight) ~36.4s, link 17.4s, object-cache scan
  6.3s; generation 478 published 08:13:01Z.
- At the 60.568s mark the builder was inside `changed_source_preflight`
  (08:11:57.628Z -> 08:12:33.983Z), actively compiling `npc.cpp`; not stalled.
- Wake `wake-7c55f10f7f4e`: one request created 08:12:26.402Z, queued once
  08:12:26.411Z; codex adapter launched 08:12:27Z, stale-launch recovered and
  retried with opencode 08:25:04Z on the same wake id. Exactly one issue and one
  wake for this wait_id.

## Findings

1. Benign wait: a normal one-file rebuild of the heavy `src/npc/npc.cpp` TU plus
   whole-game link needs ~97s, so the flat 60s `game_build` budget trips even on
   a successful build. No build failure, no stall.
2. Issue fields are complete (ready=true, missing=[]) and the one-wake rule
   passes; set `related_build` to the pinned attempt. No code fix requested.
3. Persisting evidence defects: wait `evidence.build_id` is the prior published
   artifact `2026-11-22T08:00:31.376619Z` and `generation=null` although this
   attempt ran generation 478. The dev-loop runtime journal path
   `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl` still does not exist,
   so the `Dev-loop wait event` proof expectation cannot be read (tracked by
   issue-0498cbef4ef3).

## Claims

- Source evidence: none (review only).
- Build evidence: `.dev/build-timings/game-dll-20261122T081124311953Z-1d0369.json`
  and `.jsonl` (success, generation 478).
- Runtime evidence: none required for this wait-policy/issue review.
- Human acceptance: not performed.

## Handoff

- Appended verdict `confirmed-wait-policy-noise; root-cause=heavy-single-source-rebuild`
  to issue-c2346d3bbc82 (state now in_review).
- Systemic wait-policy ownership remains with issue-9a4503f6ed68 /
  issue-e044a931c54b; in-flight identity with issue-136d4553ff92; missing journal
  with issue-0498cbef4ef3. No code fix claimed.

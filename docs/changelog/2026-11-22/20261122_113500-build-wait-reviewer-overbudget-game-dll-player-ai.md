# Over-budget game-DLL wait review (build-wait-reviewer)

Date: 2026-11-22 ~11:35Z
Agent: build-wait-reviewer
Context: wake-d0811baf6504
Issue: issue-d5a7e874a0db (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, or build-script edits were made. No
compiler, build.py, module worker, or second dev-loop was invoked. No new runtime
was started.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T11:04:12.764Z`, stage `game_build`,
  elapsed 60.17s, budget 60s.
- Actual attempt `.dev/build-timings/game-dll-20261122T110412831692Z-baa638.json`,
  generation 487, changed `src/actor/player-ai.cpp`, success, total 105.781s,
  completed 2026-11-22T11:05:58.647Z.
- Cost: compile 42.797s (single TU `src/actor/player-ai.cpp` 40.457s), link
  21.938s, object-cache scan 2.875s. compiled=2, skipped=641, so this was not a
  full rebuild.
- Build note cd658a118507 (cpp-latency-linker-investigation): temporary
  comment-only canary on `src/actor/player-ai.cpp`; gen488 attempt (107.156s) is
  the follow-on.
- Wake `.dev/agent-wake/events.jsonl`: exactly one wake.queued for the issue
  (wake-d0811baf6504) at 11:05:16.312Z; codex adapter process exited, opencode
  fallback launched 11:17:03.244Z (~11.7 min queued-to-launch).

## Findings

1. Legitimate latency wait, not a queue/worker stall. The build was in flight at
   the 60s mark, then completed successfully.
2. The issue reason `sources=642 ... seeded=0` is the total source count, not a
   rebuild count; only 2 units compiled / 641 skipped. The evidence generation
   (486) is wait-time state while the completed attempt is generation 487.
   Diagnostic clarity only; `related_build` is now pinned to the exact attempt.
3. The fixed 60s budget is shorter than a normal heavy-TU compile+link (105.8s
   here; `player-ai.cpp` compiles 27.7-40.7s across nearby attempts). Same
   systemic false-positive class as issue-9a4503f6ed68 and issue-136d4553ff92.
4. Wake adapter fallback delayed effective launch ~11.7 min; wake-delivery
   latency, not build-queue friction.

## Claims

- Source evidence: none (review-only).
- Build evidence: attempt game-dll-20261122T110412831692Z-baa638, generation 487.
- Runtime evidence: none required for this wait-policy issue.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `pass_with_findings` to issue-d5a7e874a0db; state
  `in_review`.
- Recorded occurrence evidence on owner issue issue-9a4503f6ed68.
- No duplicate issue created; no code fix claimed.

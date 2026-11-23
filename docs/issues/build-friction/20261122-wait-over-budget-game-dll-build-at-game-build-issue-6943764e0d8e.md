# issue-6943764e0d8e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass)

### 2026-11-22T22:39:49.860Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait game-dll-build:2026-11-22T22:14:00.638Z (issue-6943764e0d8e). Verdict: pass - the wait record and single-reviewer-wake hook behaved as designed. This was a genuine over-budget incremental whole-game DLL build, not a stall.

Direct observations:
- Active attempt game-dll-20261122T221400682391Z-2b5e5e, source_generation 12, generation 536, revision be433688e83c86d8b780e1d99b3c089674341c3a6572eff99737d8b85b5e6cb0, request auto-12, change note by cpp-latency-small-tu-benchmark changing src/npc/npc-world-query.cpp (source: .dev/build-timings/game-dll-20261122T221400682391Z-2b5e5e.json/.jsonl).
- Started 22:14:00.682Z, completed 22:15:35.659Z; total 94.965s (worker 88.404s), success true. The 60.541s budget tripped ~34s before the attempt finished.
- Phase breakdown: changed_source_preflight 56.220s (this carries the 56.170s compile of src/npc/npc-world-query.cpp), module-identity-provider compile 3.640s, link 25.624s for 642 objects / 482,226,844 bytes, object_cache_scan 1.652s, publish 0.447s, dependency_check 641 units / 0.671s. This is the normal cost of a one-TU incremental build (heavy TU compile + full link), not a hang.
- Exactly one issue exists for this wait_id and exactly one reviewer wake was queued. Journal logs/2026-11-22/20261122_220630/dev-loop/events.jsonl: line 60 dev-loop.wait.over_budget created=true, line 59 dev-loop.wait.reviewer-wake issue=issue-6943764e0d8e wake=wake-14eefe244552. Motivation: .dev/agent-wake/events.jsonl wake.queued 22:15:03.968Z (once); codex claimed/launched 22:15:19.4, retry opencode 22:18:00.5, opencode claimed/launched 22:25:23.0. One wake request; opencode delivery ~10 min is a wake-latency observation, not a defect of this wait record.

Caveats already tracked elsewhere (no duplicate issue filed):
- Stored evidence.generation=535 and evidence.build_id=2026-11-22T21:32:48.195707Z are the last published artifact, not the active attempt (generation 536). Same wait-instrumentation gap noted on prior reviews.
- Repeat-pattern cause: WAIT_ISSUE_SECONDS=60 is below normal incremental game-DLL cost (this build 95.0s), so routine builds file a wait issue. At review time there are 128 sibling issues titled 'Wait over budget: game-dll-build at game_build' (105 in_review, 22 new, 1 assigned), including issue-681e9fae416f (22:25:14) and issue-956990a15eb2 (22:26:57) - three in ~12 min. Tracked in parent issue-136d4553ff92 (owner build-coordinator) and issue-9a4503f6ed68 (owner wake-proof-wait); no new duplicate created.

True in-progress attempt for this occurrence is game-dll-20261122T221400682391Z-2b5e5e / generation 536. No source edited, no lease taken. Confidence: high on timings, attempt identity, and the one-issue/one-wake counts. Budget-threshold and evidence-quality fixes remain with their existing owners.

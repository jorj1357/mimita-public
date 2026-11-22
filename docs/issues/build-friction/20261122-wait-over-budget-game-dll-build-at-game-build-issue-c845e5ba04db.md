# issue-c845e5ba04db: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-false-positive)

### 2026-11-22T08:05:42.736Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed 2026-11-22 by build-wait-reviewer (no source edits). Conclusion: false-positive friction signal; the build was healthy, not stuck.

Evidence:
- Issue record: wait_id game-dll-build:2026-11-22T07:33:56.647Z, stage game_build, elapsed 60.83s, budget 60.0s, generation 472, build_id 2026-11-22T02:37:06.972018Z, request_ids auto-14 + 2026-11-22T07:34:10.471Z-fb9e410e.
- Attempt summary .dev/build-timings/game-dll-20261122T073356741874Z-7719e3.json: success=true, exit 0, generation 473, total 118.0s, completed 07:35:54.754Z. Phase durations: changed_source_preflight 53472ms, compile(npc.cpp) 53256ms, link 31938ms, publish 870ms.
- Timeline (same .jsonl): build.started 07:33:56.743Z; pre-worker phases through 07:34:20.6; src/npc/npc.cpp translation_unit.started 07:34:20.719Z, completed 07:35:14.002Z (53.256s, status compiled); link started 07:35:20.585Z; build.completed 07:35:54.753Z. The 60.83s wait fired ~07:34:57.5 while npc.cpp was actively compiling (33s into a 53s compile) and progress events continued to the successful end. No queue stall, failed phase, or dead worker.
- Wake record: .dev/agent-wake/events.jsonl wake.queued wake-73f019adbf94 at 07:34:59.490Z for issue-c845e5ba04db.

First divergence / root cause: devscripts/dev-loop.py:109-110 sets WAIT_ISSUE_SECONDS default 60s and applies it uniformly to game-dll-build at dev-loop.py:3520-3536. A routine single-TU build here is 118s (preflight 53s + compile 53s + link 32s), so any npc.cpp edit crosses 60s while healthy. That is a threshold/measurement defect, not build friction.

Impact/recurrence: dozens of identical 'Wait over budget: game-dll-build at game_build' issues and reviewer wakes on 2026-11-21/22 (e.g. issue-1052cb7f28c4, issue-37dde12f63ff, issue-9c77739954f9, issue-61edee02549f, issue-86b3b2ce1e73) - the duplicate flood warned about in this issue's risk field.

Recommended fix (separate owned task, not implemented here): make the game-dll-build budget stage/progress-aware - derive from measured p95 (>=180s) and/or alert only when no new attempt timeline/progress event occurs for N seconds (stall detection) instead of raw elapsed. Suggest appended to issue-9a4503f6ed68 (owner wake-proof-wait). Confidence: high; no gameplay/runtime behavior involved.

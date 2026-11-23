# issue-a240a6dd4eab: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T22:30:27.374Z — build-wait-reviewer

related_build: game-dll-20261122T220456195519Z-2dad5a

## Review (pass)

### 2026-11-22T22:31:30.691Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence game-dll-build:2026-11-22T22:04:56.185Z (issue-a240a6dd4eab). Issue record is complete (wait_id, stage=game_build, reason, elapsed=60.501s, budget=60s, generation=533, source_generation=9, request_ids auto-9 / 2026-11-22T22:05:05.316Z-33b81e0c / 2026-11-22T22:05:32.913Z-26703353) and exactly one reviewer wake was queued (wake-75c2d2420471, created 22:05:57.240Z). No source edit; no duplicate issue created.

Exact attempt evidence (.dev/build-timings/game-dll-20261122T220456195519Z-2dad5a.json and .jsonl):
- success=true, generation=533, source_generation=9, started 22:04:56.195Z, completed 22:06:13.181Z, total_seconds=76.976
- changed_paths=[src/network/server-npcs.cpp]; changed_source_preflight=51.697s (includes 51.613s compile of server-npcs.cpp); compile phase=1.010s (identity provider only); link=17.802s; publish=0.408s
- The 60.501s wait fired at 22:05:56.696Z while the build was healthy inside link.lld_linker, and the build succeeded ~17s later. This is not a stall, hang, or measurement error.

Root cause of recurrence: WAIT_ISSUE_SECONDS is a fixed 60s (devscripts/dev-loop.py:109; env MIMITA_WAIT_ISSUE_SECONDS, min 10s) with no stage or translation-unit-size awareness, while normal single-TU incremental rebuilds of the largest sources are ~60-99s. Adjacent successful attempts show the same signature: generation 530 server-npcs.cpp=98.887s, 532 server-npcs.cpp=61.685s, 533 server-npcs.cpp=76.976s, 536 npc-world-query.cpp=94.965s; npc-spawn.cpp builds were 23.102s and 29.895s (under budget). The alarm is therefore expected whenever server-npcs.cpp or npc-world-query.cpp changes.

Duplicate flood (the risk recorded on this issue is materializing): 127 issues titled Wait over budget: game-dll-build at game_build were opened between 2026-11-20T08:32:55Z and 2026-11-22T22:25:14Z (23 new, 103 in_review, 1 assigned). One new issue per wait occurrence, with no signature coalescing, produces this flood and buries genuinely unique friction.

Secondary observations (evidence quality, not acceptance failures):
- This occurrence reason is a truncated raw JSON fragment (58467.283, build_attempt_id:...) instead of the readable [GAME BUILD] revision=... sources=642 ... summary seen on later waits; the detail was overwritten by a partial output line.
- Reviewer wake lagged: queued 22:05:57.240Z, launched 22:18:00.753Z (~12m03s, attempts=2), after the build already succeeded at 22:06:13Z, so the reviewer could not observe the live wait.
- The dev-loop journal for this run was not retained (run 20261122_204150 has no dev-loop/events.jsonl). The issue record (which embeds the wait event), the build-timings artifact, and the wake request/event are retained and sufficient for acceptance.

Verdict: pass for the recorder and issue acceptance (fields complete; exactly one wake queued). The wait is a true over-budget event, not a false measurement; the friction is the fixed 60s budget versus normal large-TU builds plus the duplicate flood. Recommendation filed on umbrella issue-9a4503f6ed68: make the budget stage/TU-size-aware (for example >=180-240s for game-DLL build, or derived from measured per-TU baseline) and coalesce recurring identical waits into one rolling occurrence issue. No source fix claimed.

## Review (addendum-identity-defect)

### 2026-11-22T22:33:17.966Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Addendum to review 219, after reading the independent umbrella review 217 (issue-review-sweeper). Correction: this issue's structured evidence carries "generation": 532 (the previous completed game-DLL generation), while the actual in-flight attempt is generation 533 (game-dll-20261122T220456195519Z-2dad5a, revision e0fd19ad...). The wait_id and the build_attempt_id embedded in the reason are correct, so traceability is recoverable, but the top-level generation field is stale. This is another instance of the identity defect reported on umbrella issue-9a4503f6ed68 (dev-loop.py:3544 reads self.game_build_generation, which is assigned only when a game build completes, so an over-budget wait records the prior generation and the prior build_id "2026-11-22T21:32:48.195707Z" instead of the in-flight attempt). Therefore this issue's acceptance row "carries ... build/run IDs" is only partially met: wait_id/stage/reason/elapsed are correct and one wake was queued, but the build identity fields are the previous generation. No source edit or duplicate issue created; defect remains owned by umbrella issue-9a4503f6ed68.

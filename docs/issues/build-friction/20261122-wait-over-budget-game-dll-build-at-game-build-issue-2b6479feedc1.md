# issue-2b6479feedc1: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (not-wedged-policy-miscalibrated)

### 2026-11-22T04:17:20.703Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer
Review of wait game-dll-build:2026-11-22T03:57:46.109Z (elapsed 60.072s, budget 60.0s).

VERDICT: not-wedged / policy miscalibrated. The trip fired while the build was actively compiling in changed_source_preflight; compile+link were still ahead. The same attempt then completed successfully. Same root cause already analyzed on sibling issue-60f01796d952 / issue-0968dd48e9d8; this entry records this occurrence's exact evidence instead of creating a new issue.

EVIDENCE (run 20261122_011031, logs/2026-11-22/20261122_011031/dev-loop/events.jsonl; .dev/build-timings/game-dll-20261122T035746224424Z-1ac6c9.json + .jsonl):
- dev-loop.wait.over_budget at 03:58:48.620Z (events line 563): wait_id game-dll-build:2026-11-22T03:57:46.109Z, elapsed_seconds 60.072 >= budget 60.0, build_id 2026-11-22T02:37:06.972018Z, recorded generation 449, source_generation 10, request_ids [auto-10], reason revision 6568d050 sources=642 jobs=10 cache=build\game\objects\b10e3a31b3ae8637 seeded=0. Exactly one reviewer wake queued (wake-2e23cdf3ddd3, events line 562) and issue-2b6479feedc1 created=true; no duplicate issue for this wait_id.
- The wait maps to attempt game-dll-20261122T035746224424Z-1ac6c9: build started 03:57:46.224Z, source_generation 10, request auto-10, revision 6568d050 (identical to the wait reason). Changed path src/npc/npc.cpp.
- At trip time the build was not stalled: pre-worker codegen resident_state 0.756s, codegen packet_registry 2.590s, manifest_snapshot 4.79s, manifest_validation 0.014s, source_snapshot 12.07s (completed 03:58:07.242Z). dev-loop.game-build.started (worker start) at 03:58:07; src/npc/npc.cpp submitted 03:58:13.922Z, started 03:58:14.018Z, compiled in 151.895s. The trip landed ~32s into that single 151.9s TU compile, i.e. active forward progress, inside the 152.85s changed_source_preflight phase.
- Outcome: success. Generation 450 published/ready at 04:03:02.625Z, total_seconds 316.377. Phases: changed_source_preflight 152.85s (dominated by preflight compile of src/npc/npc.cpp at 151.90s), compile 58.90s, link 64.35s (665 args/642 inputs, 480,189,425 B), publish 3.07s, object_cache_scan 8.56s; 12 TUs compiled, 630 skipped.
- Every stage produced forward progress; no hang, crash, or no-op.

CAUSE OF THE RECURRING FRICTION: dev-loop.py WAIT_ISSUE_SECONDS = 60 (dev-loop.py:109-110) is applied to game-dll-build from attempt/game_build start (dev-loop.py:3520-3536), so the timer counts pre-worker codegen + manifest + source snapshot and the long changed_source_preflight. This attempt's 152.85s preflight alone is ~2.5x the budget, so healthy whole-game DLL builds structurally trip an identical issue and reviewer wake. Roughly 40 sibling wait issues now exist.

MINOR EVIDENCE ACCURACY ISSUE: the recorded evidence has generation=449, but attempt 1ac6c9 produced generation 450; self.game_build_generation retains the prior published generation at trip time. source_generation=10 is correct for the request.

RECOMMENDATION (human approval needed; no source edited here): start the game-dll-build wait clock at dev-loop.game-build.started instead of attempt start, gate issue creation on absence of forward progress (no new stage/file event within N seconds), and/or give game-dll-build a measured-p95 budget. Natural owner: issue-136d4553ff92 (account for every DLL build stage). No files claimed.

## Context

### 2026-11-22T04:17:43.039Z — build-wait-reviewer

related_build: game-dll-20261122T035746224424Z-1ac6c9 (generation 450)
related_run: 20261122_011031

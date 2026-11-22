# issue-60f01796d952: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (not-wedged-policy-miscalibrated)

### 2026-11-22T04:10:25.869Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait game-dll-build:2026-11-22T03:51:48.191Z (elapsed 60.244s, budget 60.0s).

VERDICT: not-wedged / policy miscalibrated. The trip fired while the build was actively advancing through changed_source_preflight; compile and link were still ahead. The same attempt then completed successfully. Same root cause already analyzed on sibling issue-6bcfb6252dfa / issue-0968dd48e9d8; this entry records this occurrence's exact evidence instead of a new issue.

EVIDENCE (run 20261122_011031, logs/2026-11-22/20261122_011031/dev-loop/events.jsonl; .dev/build-timings/game-dll-20261122T035148464587Z-d1c3a1.json + .jsonl):
- dev-loop.wait.over_budget at 03:52:52.753Z (events line 528): wait_id game-dll-build:2026-11-22T03:51:48.191Z, elapsed_seconds 60.244 >= budget 60.0, build_id 2026-11-22T02:37:06.972018Z, recorded generation 448, source_generation 9, request_ids [auto-9], reason revision 0057ca8d sources=642 jobs=10. Exactly one reviewer wake queued (wake-2eac4cef15e2, events line 527) and issue-60f01796d952 created=true.
- The wait maps to attempt game-dll-20261122T035148464587Z-d1c3a1: build.started 03:51:48.498Z, source_generation 9, request auto-9, revision 0057ca8d (identical to the wait reason). Changed path was src/main-systems.cpp.
- At trip time the build was not stalled: pre-worker stages were codegen resident_state 0.83s, codegen packet_registry 2.33s, manifest_snapshot 4.82s, manifest_validation 0.30s, source_snapshot 17.66s; dev-loop.game-build.started (worker start) at 03:52:21.727Z. The trip landed ~30s into the 156.0s changed_source_preflight phase; compile (76.0s) and link (51.2s) had not yet begun.
- Outcome: success. Generation 449 published/ready, total_seconds 331.288, worker_total_seconds 295.368. Phases: changed_source_preflight 156.02s (dominated by preflight compile of src/main-systems.cpp at 154.93s), compile 76.05s (largest single TU npc.cpp 75.17s), link 51.19s (665 args, 642 inputs, 480,188,431 B), publish 3.60s, object_cache_scan 6.43s; 13 TUs compiled, 629 skipped.
- Every stage produced forward progress; no hang, crash, or no-op.

CAUSE OF THE RECURRING FRICTION: dev-loop.py WAIT_ISSUE_SECONDS = 60 (dev-loop.py:109-110) is applied to game-dll-build from attempt/game_build start (dev-loop.py:3520-3536), so the timer counts pre-worker codegen + manifest + source snapshot and the long changed_source_preflight. This attempt's 156s preflight alone is ~2.6x the budget, so healthy whole-game DLL builds structurally trip an identical issue and reviewer wake. Roughly 40 sibling wait issues now exist.

MINOR EVIDENCE ACCURACY ISSUE: the recorded evidence has generation=448, but attempt d1c3a1 produced generation 449; self.game_build_generation retains the prior published generation at trip time. source_generation=9 is correct for the request.

SEPARATE DEFECT (not this wait): the later generation 450 attempt failed with 'codegen check failed: packet_registry_codegen.py' (dev-loop --status). That is a distinct build failure, not the cause of this earlier preflight wait; triage separately.

RECOMMENDATION (human approval needed; no source edited here): start the game-dll-build wait clock at dev-loop.game-build.started instead of attempt start, gate issue creation on absence of forward progress (no new stage/file event within N seconds), and/or give game-dll-build a measured-p95 budget. Natural owner: issue-136d4553ff92 (account for every DLL build stage). No files claimed.

## Context

### 2026-11-22T04:10:46.351Z — build-wait-reviewer

related_build: game-dll-20261122T035148464587Z-d1c3a1
related_run: 20261122_011031

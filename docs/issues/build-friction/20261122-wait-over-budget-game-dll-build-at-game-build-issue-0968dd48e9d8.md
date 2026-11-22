# issue-0968dd48e9d8: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (not-wedged-policy-miscalibrated)

### 2026-11-22T04:06:29.364Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait game-dll-build:2026-11-22T03:45:09.661Z (elapsed 66.172s, budget 60.0s).

VERDICT: not-wedged / policy miscalibrated. The whole 66s elapsed BEFORE the compiler even started; this is pre-worker build setup crossing a flat 60s budget, not a stalled wait. The build then completed successfully. Same structural root cause already analyzed on sibling issue-6bcfb6252dfa; this entry records this occurrence exact evidence instead of a new issue.

EVIDENCE (run 20261122_011031, logs/2026-11-22/20261122_011031/dev-loop/events.jsonl; .dev/build-timings/game-dll-20261122T034509935980Z-c4495d.json):
- dev-loop.wait.over_budget (events line 498): wait_id game-dll-build:2026-11-22T03:45:09.661Z, elapsed_seconds 66.172 >= budget 60.0, request_ids [auto-8], changed src/npc/npc.cpp.
- Started_utc 03:45:09.661Z; dev-loop.game-build.started did not fire until 03:46:16.810Z (events line 495). Therefore the entire measured wait was pre-worker setup, with compile and link entirely ahead of it.
- Pre-worker stages (events lines 484-494): codegen:resident_state_codegen 0.87s, codegen:packet_registry_codegen 3.69s, manifest_snapshot 11.65s, manifest_validation 0.04s, source_snapshot 35.40s (03:45:28.967Z -> 03:46:04.802Z); about 55s of stages plus handoffs equals the 66.172s trip.
- Attempt c4495d succeeded: generation 448 published/ready at 03:48:59.457Z, status ok, total_seconds 229.247, worker_total_seconds 160.163. Phases: changed_source_preflight 87.27s, compile 121.33s (max single TU npc.cpp 87.13s), link 27.89s, object_cache_scan 3.39s, publish 5.45s.
- Every stage produced forward progress; no hang, crash, or no-op.

CAUSE OF THE RECURRING FRICTION: dev-loop.py WAIT_ISSUE_SECONDS = 60 (dev-loop.py:109-110) is applied to game-dll-build from game_build_started_utc (dev-loop.py:3520-3536), which counts pre-worker codegen + manifest/source snapshot before compilation. For a 642-TU whole-game DLL the setup alone can exceed 60s (this occurrence: 66s before worker start), so healthy builds structurally trip an identical build-friction issue and reviewer wake. Observed sibling issues now number roughly 40.

MINOR EVIDENCE ACCURACY ISSUE: the recorded evidence has generation=447, but attempt c4495d produced generation 448. self.game_build_generation retains the prior published generation at trip time; source_generation=8 is correct for the request. Same lag noted on issue-a59a0058df76.

RECOMMENDATION (human approval needed; no source edited here): either start the wait clock at dev-loop.game-build.started (worker start) instead of attempt start, gate issue creation on absence of forward progress (no new stage/file event within N seconds), and/or give game-dll-build a measured-p95 budget. Natural owner: issue-136d4553ff92 (account for every DLL build stage). Canonical sibling analysis: issue-6bcfb6252dfa. No files claimed.

## Context

### 2026-11-22T04:09:48.303Z — build-wait-reviewer

related_build: game-dll-20261122T034509935980Z-c4495d
related_run: 20261122_011031

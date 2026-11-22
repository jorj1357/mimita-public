# issue-d2466e463a18: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-22T05:28:20.234Z — build-wait-reviewer

Reviewer: build-wait-reviewer

REVIEW (build-wait-reviewer) — over-budget wait confirmed; evidence build-identity defect and ~41-minute reviewer-delivery delay found.

VERIFIED THIS OCCURRENCE (journal logs/2026-11-22/20261122_011031/dev-loop/events.jsonl):
- dev-loop.wait.over_budget event_id DEV_RUNTIME_7a4341742b5b4b44836518bcf773fd25 at 2026-11-22T04:37:04.597Z; wait_id game-dll-build:2026-11-22T04:36:01.508Z; kind game-dll-build; stage game_build; elapsed 60.541s; budget 60.0s; created=true -> issue-d2466e463a18.
- dev-loop.wait.reviewer-wake event_id DEV_RUNTIME_c9ecbd529eb14fc38f101e3b6eecc56d; issue issue-d2466e463a18; queued=true.
- The wait is legitimate, not a wedged build: attempt game-dll-20261122T043601681032Z-fc1d05 (generation 456, source_generation 7) started 04:36:01.681Z and completed 04:39:06.403Z, total_seconds 184.712 (worker 132.587). Result event 04:39:05.491Z: compile_seconds 50.616, link_seconds 75.881, object_scan_seconds 3.447. The 60.541s trip occurred while src/network/multiplayer-tick.cpp was still compiling (TU completed 04:37:40.137Z), ~124s before the build finished. Forward progress throughout.

FIRST DIVERGENCE — wait evidence carries the wrong build identity:
- Issue evidence records build_id=2026-11-22T02:37:06.972018Z and generation=455. That is the previously published build, not the build that was waited on.
- Cause: check_wait_budget uses self.latest_build (the last published EXE artifact) for build_id (devscripts/dev-loop.py:3532) and self.game_build_generation (devscripts/dev-loop.py:3533), which is only updated at result time (dev-loop.py:2967); it is not set when start_game_build_worker sets game_build_status=building (dev-loop.py:2879-2884). The in-flight attempt id game-dll-20261122T043601681032Z-fc1d05 exists in dev-loop.game-build.started (event DEV_RUNTIME_f45f05134d9d4361ac71e9ab1c9f640e) but is not in the issue.
- Impact: acceptance says the issue must carry build/run IDs. wait_id, stage, reason, elapsed, source_generation=7 and request_ids are correct, but build_id/generation point at an unrelated prior build, so a reviewer cannot pin the waited-on attempt from the issue alone.

SECOND FINDING — the reviewer wake was queued once but not delivered for ~40.8 minutes:
- .dev/agent-wake/events.jsonl: wake-490a2459dd30 queued 2026-11-22T04:37:03.139Z (WAKE_db6152ff15f946edae9222f76a53106a), claimed codex 04:37:04.016Z, launched codex pid 2944 04:37:04.275Z (WAKE_6260e6ee02994728842347c98b783e83).
- wake.retry to opencode at 04:46:47.396Z (WAKE_bee7f67f0e6a4407914e17f112c82f7b), but the opencode claimed/launched only at 05:17:51.310Z / 05:17:51.344Z (WAKE_5134b8310e7f46429b1ec4f7cb04423f / WAKE_ab5fb1cb18474c84babb7caeadc2398c). The codex adapter exited without completing the review; the fallback waited behind other wake work (wake-d8333c8c5780 for issue-ce4c245a8a70 was launched under the same 04:37:04 dispatch).
- "Queued exactly once" holds; "reviewer work happens promptly after the one-minute budget" does not. Delivery latency was ~40.8 min, so the wait-friction feedback loop itself exceeded four 10-minute budgets.

THIRD FINDING — the 60s budget is below normal whole-game build cost (policy noise):
- WAIT_ISSUE_SECONDS defaults to 60s (devscripts/dev-loop.py:109-110). This legitimate single-file incremental build took 184.7s (~3x budget); the prior generation-455 build took 67.8s.
- Issue inventory: 69 open/closed "Wait over budget: game-dll-build at game_build" issues (53 in_review, 16 new). One issue is filed per over-budget occurrence by design (build_friction.py:143-200), so every routine DLL build floods the board. This is exactly the risk recorded in the issue ("A noisy wait policy could create duplicate friction issues").

RECOMMENDATIONS (no code edited or claimed; needs a separate owner):
1. Record the in-flight game-build attempt id/generation in the wait evidence (thread the build_attempt_id/generation from dev-loop.game-build.started/start_game_build_worker into check_wait_budget), and stop using the EXE latest_build id for a game-DLL wait.
2. Deliver the single queued reviewer wake within a bounded time: do not let a failed codex launch burn ~40 min before the opencode fallback runs; cap adapter handoff latency or fail fast when the primary exits without work.
3. Calibrate the game-dll-build budget against measured cost (>=200s, or stage-aware) and/or aggregate repeated game-dll-build waits into one tracking issue, so the 69-issue backlog is not re-created.

Confidence: high for all findings (direct journal events + .dev/build-timings/game-dll-20261122T043601681032Z-fc1d05.json + code references). No runtime behavior proof claimed; this is process/evidence review only.

## Context

### 2026-11-22T05:28:43.301Z — build-wait-reviewer

related_build: game-dll-20261122T043601681032Z-fc1d05

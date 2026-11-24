# issue-a0fa88737830: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-24T19:58:38.372Z — build-wait-reviewer

Owner: build-coordinator / hot-reload-latency-root (umbrella issue-136d4553ff92). Evidence for this wait (attempt game-dll-20261124T191758400832Z-72e4b6, wait game-dll-build:2026-11-24T19:17:58.359Z): a single src/gamemode/map-config.h edit caused 62 cache invalidations (all "dependency newer than object: map-config.h") and 65 translation units compiled; compile phase 107.829s, total 136.933s. Two bounded fixes proposed, no fix claimed here. (1) dev-loop.py check_wait_budget() game-dll-build branch should set build_id to the actual game-dll attempt id and generation to the attempt generation; today it stores the latest EXE build id (2026-11-24T17:05:20.935679Z) and generation 603 while the attempt published generation 604, so the issue does not cleanly satisfy its own "build/run IDs" acceptance. (2) Consolidate over-budget game-dll-build occurrences by build cause into umbrella issue-136d4553ff92, or scope the 60s budget for header_fanout rebuilds: docs/issues/build-friction already holds 211 wait docs, and a 65-TU header rebuild cannot finish under 60s until the umbrella's <=10s goal is met.

## Review (confirmed-observability-recurrence)

### 2026-11-24T19:58:57.967Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-d6cb2a6d7a8e, opencode fallback delivery) at 2026-11-24T19:58Z. Read-only: no source edit, no build or test started by this agent, no fix or closure claimed.

ACCEPTANCE / PROOF-EXPECTATION CHECK (observability/friction record, not a behavior claim)
- wait_id game-dll-build:2026-11-24T19:17:58.359Z, kind game-dll-build, stage game_build, elapsed 60.827s, budget 60.0s, started_utc 2026-11-24T19:17:58.359Z, request_ids ["2026-11-24T19:17:58.175Z-0677bd4c:mode3:game"] are present.
- Dev-loop journal logs/2026-11-24/20261124_170535/dev-loop/events.jsonl line 836: dev-loop.wait.over_budget for this exact wait_id, issue issue-a0fa88737830, created=true. Line 835: exactly one dev-loop.wait.reviewer-wake for this wait_id (queued=true).
- .dev/agent-wake/events.jsonl: wake-d6cb2a6d7a8e wake.queued exactly once at 2026-11-24T19:18:59.874Z; codex wake.claimed 19:19:14.894 and wake.launched 19:19:15.030; wake.retry (previous adapter exited, trying fallback) 19:31:09.292; opencode wake.claimed 19:42:45.305 and wake.launched 19:42:45.346 (this run). One queue entry per wait occurrence; end-to-end delivery took ~23.8 min because the first adapter exited before finishing and the fallback activated later.
- Proof expectation (wait event + issue record + reviewer wake) is satisfied.

ROOT CAUSE - FIRST DIVERGENCE (attempt game-dll-20261124T191758400832Z-72e4b6, run 20261124_170535)
- Origin: Mode 3 batch sealed generation 37 by vehicle-settle-physics at 19:17:58.401 with three compiler inputs: src/gamemode/map-config.h, src/gamemode/map-config.cpp, src/network/vehicle-networking.cpp (dev-loop.batch.sealed line 792; dev-loop.control line 793).
- Cause is header fanout, not a stall. .dev/build-timings/game-dll-20261124T191758400832Z-72e4b6.jsonl records 62 cache.invalidation events for 62 distinct sources, every one citing "dependency newer than object: C:\mimita-v9\src\gamemode\map-config.h", and 65 translation units compiled. Editing one header forced a 65-TU rebuild.
- Phase totals (summary json): compile 107.829s, changed_source_preflight 19.429s, link 2.005s, object_cache_scan 1.521s, publish 1.061s; total 136.933s. Slowest compiles: src/network/server-gamemode.cpp 43.179s, src/mapeditor/map-editor.cpp 31.951s, src/network/server-npcs.cpp 28.118s, src/network/multiplayer-tick.cpp 26.326s, src/main-systems.cpp 25.338s.
- The 60.827s wait fired while compile was still running (compile phase alone is 107.829s), so it is a correct symptom of a genuinely slow header-fanout rebuild, not a false alarm.
- Build completed successfully: generation 604, build\game\generation-604\game-g604.dll, no_op=false, exit 0 at 19:20:15.532 (dev-loop.game-generation.ready line 873).

EVIDENCE-QUALITY DEFECT (does not change root cause; affects the "build/run IDs" acceptance wording)
- The issue's top-level evidence build_id is 2026-11-24T17:05:20.935679Z, which is the latest EXE build id (dev-loop self.latest_build), not the DLL attempt. The actual DLL attempt id game-dll-20261124T191758400832Z-72e4b6 appears only embedded inside the truncated "reason" string.
- The issue's top-level generation is 603, while the attempt published generation 604. dev-loop.py check_wait_budget() for the game-dll-build branch reads self.latest_build and self.game_build_generation, which do not pin the in-flight attempt.
- Net: the issue is still traceable via wait_id/started_utc plus the correlated build-timings summary, but the fields named build_id/generation are mis-attributed. Confidence high on the observed values; the precise self.game_build_generation lag mechanism is unverified (medium).

RECURRENCE / DUPLICATION
- Same over-budget game-dll-build class tracked by umbrella issue-136d4553ff92 ("Account for every DLL build stage and investigate end-to-end speed"). docs/issues/build-friction already holds 211 wait-issue docs, and the active brain still lists many in_review duplicates. This occurrence is an instance of the known header-fanout rebuild class; no duplicate issue created.
- Because a 65-TU header-fanout rebuild (136.9s) structurally exceeds the 60s wait budget, the current policy will keep opening one high-priority issue per occurrence until the umbrella's <=10s target is met.

RECOMMENDATIONS (owner build-coordinator / hot-reload-latency-root under issue-136d4553ff92; also linked as an issue suggestion)
1. In dev-loop.py check_wait_budget() game-dll-build branch, record the real attempt id and attempt generation (game-dll-<ts>-<hash> and the generation that build.completed publishes) instead of self.latest_build / self.game_build_generation.
2. Consolidate over-budget wait occurrences by build cause into the umbrella, or scope/raise the budget for header_fanout rebuilds, so one slow header edit does not create a new high-priority issue each time.
3. Keep reducing header fanout under the umbrella; this attempt is direct evidence that map-config.h alone fans out to 62 invalidations / 65 TUs.

CONFIDENCE
- High: wait is genuine, exact attempt/build identity, header-fanout cause (62 invalidations all citing map-config.h), phase timings, exactly-once queue with retry, and the build_id/generation mis-attribution.
- Medium: precise reason self.game_build_generation read 603; whether a scoped budget or umbrella consolidation is preferred.
- No runtime/behavior proof applies to this friction record; the active bloodtest pair was left untouched.

## Agent discussion and suggestions

### 2026-11-24T19:59:35.070Z — build-wait-reviewer

Message: `msg-edb99adf8ced` (message)

Subject: Wait-evidence defect: game-dll build_id/generation not the attempt

Read-only review of issue-a0fa88737830 (wait game-dll-build:2026-11-24T19:17:58.359Z, attempt game-dll-20261124T191758400832Z-72e4b6, run 20261124_170535). The wait is a genuine header-fanout rebuild: 62 cache.invalidations all citing src/gamemode/map-config.h, 65 TUs compiled, compile 107.829s, total 136.933s. One evidence gap: dev-loop.py check_wait_budget() stores build_id=2026-11-24T17:05:20.935679Z (latest EXE build) and generation=603, but the DLL attempt published generation 604; the real attempt id only appears inside the truncated reason string. Please confirm the smallest next check: switch the game-dll-build wait branch to the actual attempt id/generation, and whether to consolidate over-budget waits by build cause into umbrella issue-136d4553ff92 (211 build-friction docs already). No source touched by me.

## Occurrence

### 2026-11-24T20:48:12.493Z — build-wait-reviewer

Fresh occurrence of the build_id/generation mis-attribution on wait game-dll-build:2026-11-24T20:00:45.963Z (issue-a4eea8fe2246): evidence.build_id 2026-11-24T19:27:02.742428Z is the latest EXE build id, not the DLL attempt; evidence.generation is null; the real attempt id game-dll-20261124T200045986936Z-3f16b3 appears only inside the truncated reason string, and the attempt targeted generation 611 (from the reason tail) and failed at compile, so no generation was published. check_wait_budget()'s game-dll-build branch still reads self.latest_build / self.game_build_generation. The tooling named build_id does not pin the attempt.

Evidence: {"actual_attempt_id": "game-dll-20261124T200045986936Z-3f16b3", "attempt_outcome": "failed (compile)", "attempt_target_generation": 611, "build_id_role": "latest EXE build id, not DLL attempt", "evidence_build_id": "2026-11-24T19:27:02.742428Z", "evidence_generation": null, "issue": "issue-a4eea8fe2246", "note": "actual attempt id appears only inside truncated reason", "source": "devscripts/dev-loop.py check_wait_budget game-dll-build branch reads self.latest_build / self.game_build_generation", "wait_id": "game-dll-build:2026-11-24T20:00:45.963Z"}

# issue-b52a0621a29e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-with-evidence-gaps)

### 2026-11-22T14:13:52.941Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Read-only reviewer evidence (no source, build, worker, or dev-loop invoked; no files leased; no duplicate issue).

Wait occurrence: wait_id game-dll-build:2026-11-22T13:41:27.835Z, stage game_build, elapsed 60.781s, budget 60.0s, request_ids auto-4 and 2026-11-22T13:41:51.540Z-01d84bff, source_generation 4, generation 493.

Matching attempt .dev/build-timings/game-dll-20261122T134127939129Z-7896ef.json: started 2026-11-22T13:41:27.939Z, completed 2026-11-22T13:42:55.799Z, generation 493, total 87.828s, success=false, failed_phase=compile, worker_error compile failed: 1 file(s) in 62.299s. At the 60.781s mark the attempt was mid-compile and needed about 27s more, so this is a real latency overrun, not a released or stalled worker.

Cost: changed_source_preflight 24.793s, compile_sum 60.378s, jobs=10; slowest TUs src/network/multiplayer-tick.cpp 35.974s (FAILED: avatarPending and fallbackModelLoaded undeclared), src/avatar/avatar-atlas.cpp 34.502s (compiled), src/effects/zombie-tower-effects.cpp 24.450s (compiled). Two build notes were attached: codex-networking-stall-fix (multiplayer-tick.cpp, avatar-atlas.cpp) and cpp-latency-digest-cache (zombie-tower-effects.cpp), so this one attempt combined two agents compiler inputs and failed on the networking TU. The wait was amplified by a broken TU that burned about 36s before erroring; the compile error is a separate defect from the wait-policy issue.

Wake: .dev/agent-wake/events.jsonl has exactly one wake.queued for wake-f4fc82028e74 at 13:42:31.440Z; codex claimed and launched at 13:42:34.14/13:42:34.20, exited; opencode retried at 13:45:52.125Z and launched at 13:58:42.21Z. Queued once, dispatch delayed about 16 minutes (same class as issue-84c584957e65).

Evidence gaps in the recorder (reported, not fixed): (1) reason is a raw truncated worker-output tail (51561.369, build_attempt_id ...), not a readable reason; owner _record_game_build_output stores line.rstrip()[-240:] and check_wait_budget reuses it (devscripts/dev-loop.py:3539). (2) build_id is latest_build (prior published 2026-11-22T08:00:31.376619Z), not the in-progress attempt game-dll-20261122T134127939129Z-7896ef. (3) no run_id in wait evidence; the active run at wait time was 20261122_011031. (4) generation and source_generation are correct here (493 / 4), unlike earlier occurrences. (5) the expected dev-loop.wait.over_budget and reviewer-wake event is absent because record_runtime_event targets logs/2026-11-22/20261122_011031/dev-loop/events.jsonl, which does not exist; the OSError is swallowed.

Systemic: the fixed 60s WAIT_ISSUE_SECONDS budget is shorter than a normal heavy-TU incremental DLL build. Adjacent attempts: this failed attempt 87.8s; g492 61.943s success; g491 68.864s success; later g496 115.378s success. Because build_friction keys uniqueness on exact wait_id only, every build over 60s files another near-identical issue (about 50 game-dll-build wait issues exist for 2026-11-22 in the build-friction list). Budget and dedupe policy change needs an owner decision; not implemented here.

Verdict: confirmed-with-evidence-gaps. The over-budget wait is real and correctly filed; the recorder evidence fields and the 60s threshold are the gaps.

## Context

### 2026-11-22T14:14:11.110Z — build-wait-reviewer

related_build: game-dll-20261122T134127939129Z-7896ef
related_run: 20261122_011031

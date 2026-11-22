# Over-budget game-DLL wait review (build-wait-reviewer)

Date: 2026-11-22 ~14:14Z
Agent: build-wait-reviewer
Context: wake-f4fc82028e74
Issue: issue-b52a0621a29e (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, or build-script edits were made. No
compiler, build.py, module worker, or second dev-loop was invoked. No new runtime
was started, no files were leased, and no duplicate issue was created.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T13:41:27.835Z`, stage `game_build`,
  elapsed 60.781s, budget 60.0s, request_ids `auto-4` and
  `2026-11-22T13:41:51.540Z-01d84bff`, source_generation 4, generation 493.
- Matching attempt `.dev/build-timings/game-dll-20261122T134127939129Z-7896ef.json`:
  started 13:41:27.939Z, completed 13:42:55.799Z, generation 493, total 87.828s,
  success=false, failed_phase=compile, `compile failed: 1 file(s) in 62.299s`.
- At the 60.781s mark the attempt was still mid-compile and needed about 27s
  more; this was a real latency overrun, not a released or stalled worker.
- Cost: `changed_source_preflight` 24.793s, compile_sum 60.378s, jobs=10.
  Slowest TUs: `src/network/multiplayer-tick.cpp` 35.974s (FAILED, undeclared
  `avatarPending`/`fallbackModelLoaded`), `src/avatar/avatar-atlas.cpp` 34.502s
  (compiled), `src/effects/zombie-tower-effects.cpp` 24.450s (compiled).
- Two build notes were attached: codex-networking-stall-fix
  (multiplayer-tick.cpp, avatar-atlas.cpp) and cpp-latency-digest-cache
  (zombie-tower-effects.cpp). One attempt combined two agents' compiler inputs
  and failed on the networking TU.
- Wake `.dev/agent-wake/events.jsonl`: exactly one `wake.queued` for
  wake-f4fc82028e74 at 13:42:31.440Z; codex claimed/launched at 13:42:34 and
  exited; opencode retried at 13:45:52 and launched at 13:58:42. Queued once,
  dispatch delayed about 16 minutes (same class as issue-84c584957e65).

## Findings

1. Legitimate latency wait, not a stall: the build was compiling at the 60s
   mark and did not finish until 87.828s. The wait was amplified by a broken TU
   that burned ~36s before erroring; that compile error is a separate defect.
2. Reporter evidence gaps (reported, not fixed):
   - `reason` is a raw truncated worker-output tail (`51561.369,...`), not a
     readable reason. Owner: `_record_game_build_output` stores
     `line.rstrip()[-240:]` (devscripts/dev-loop.py:2833/2863) and the wait dict
     reuses it (dev-loop.py:3539). The fragment also says generation 494 while
     the attempt built generation 493.
   - `build_id` is `latest_build` (prior published 2026-11-22T08:00:31.376619Z),
     not the in-progress attempt id (dev-loop.py:3543).
   - no `run_id` in the wait evidence (actual run 20261122_011031).
   - top-level `generation`/`source_generation` are correct here (493 / 4),
     unlike earlier occurrences.
   - the expected `dev-loop.wait.over_budget`/reviewer-wake event is absent:
     `record_runtime_event` targets
     `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl`, which does not
     exist, and the OSError is swallowed.
3. The fixed 60s budget is shorter than a normal heavy-TU incremental build:
   this failed attempt 87.8s; g492 61.943s success; g491 68.864s success; later
   g496 115.378s success. See systemic issue-136d4553ff92.
4. `build_friction.py` dedups on exact wait_id only, so every build over 60s
   files another near-identical issue (about 50 game-dll-build wait issues for
   2026-11-22). Budget/dedupe policy change needs an owner decision; not
   implemented here.

## Claims

- Source evidence: review-only (read of devscripts/dev-loop.py,
  devscripts/build_friction.py, build timings, wake events).
- Build evidence: attempt game-dll-20261122T134127939129Z-7896ef, generation 493,
  87.828s, failed at compile.
- Runtime evidence: agent-wake events only; no gameplay runtime required.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `confirmed-with-evidence-gaps` to
  issue-b52a0621a29e; state `in_review`. Pinned `related_build` to the exact
  attempt and `related_run` to 20261122_011031.
- No duplicate issue created; no code fix claimed.

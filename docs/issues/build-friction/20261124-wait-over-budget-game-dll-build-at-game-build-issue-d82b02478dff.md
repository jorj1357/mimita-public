# issue-d82b02478dff: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed_preworker_wait_clock_identity_fail_double_launch)

### 2026-11-24T18:29:17.007Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait `game-dll-build:2026-11-24T17:12:09.235Z` (63.935 s, budget 60.0 s, stage `game_build`, reason 'starting whole-game build') against `logs/2026-11-24/20261124_170535/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261124T171210403561Z-968a7e.json` and `.jsonl`, and `.dev/agent-wake/events.jsonl` + `requests/wake-5c0ec2e79124.json`.

Direct observations:
- Record/wake contract: exactly one `dev-loop.wait.over_budget` (2026-11-24T17:13:42.769Z) and one `dev-loop.wait.reviewer-wake` (17:13:42.768Z) for this wait_id; `created=true`; no `issue-failed`. One issue and one queued wake for this occurrence, as designed.
- The 63.935 s was not build work. `start_game_build_worker` stamps `game_build_started_utc=17:12:09.235Z` (dev-loop.py:3166) before the worker thread runs. The attempt's first timeline event, `build.started`, is `17:13:13.145Z`; the elapsed check fired at `17:13:13.170Z`, 25 ms after the build actually began, while `game_build_detail` was still the initial 'starting whole-game build'. So no build stage had executed at trigger time.
- The gap is synchronous sampler setup. The attempt's resource summary reports `sampler_setup_wall_ms=62750.0` and `sampler_setup_cpu_ms=4484.375`. `run_game_build` calls `resource_sampler.start()` before `_run_game_build` stamps `attempt_started_utc`/`build.started` (devscripts/dev_loop/game_build.py:1903-1911 and 1089-1112); `BuildResourceSampler.start()` blocks on initial discovery (devscripts/dev_loop/build_resource_sampler.py:1075-1125). 62.750 s + ~1.2 s overhead = the 63.935 s wait.
- This setup time is an outlier: across 31 game-dll summaries in `.dev/build-timings` that carry sampler data, only this attempt exceeds 30 s (62.750 s; next 17.516 s; median 0.301 s; p90 13.204 s). Host mean CPU over the attempt was 99.781% with a 28,200,960 B minimum available RAM; setup wall >> setup CPU, so the delay was mostly waiting/scheduling under saturation, not sampler CPU work.
- The waited-on attempt later failed compile at 17:36:57.457Z after 1487.063 s. The two errors are `src/vehicles/vehicle-profile.cpp` and `src/vehicles/vehicle-system.cpp` using absent `VehicleJumpProfile` members (`impulse`; profile.cpp also `heldForce`, `heldWindowSeconds`, `minimumHoldSeconds`). This is already recorded in issue-136d4553ff92 and `docs/changelog/2026-11-24/20261124_1746-sampler-setup-cost-and-failed-build.md`; not duplicated here. Those files are leased by vehicle-rl-physics, not this reviewer.

Acceptance: PARTIAL/FAIL.
- wait_id, stage, reason, elapsed, evidence: present.
- build/run IDs: FAIL. Evidence `build_id=2026-11-24T17:05:20.935679Z` is the previous EXE `latest_build`, and `generation=595` is the previous published generation; the waited-on game-DLL attempt `game-dll-20261124T171210403561Z-968a7e` (source_generation 17; request_ids match `2026-11-24T17:11:59.266Z-ba94d77f:mode3:game`) appears only in the journal/timings. `check_wait_budget` sources `build_id` from `self.latest_build` (dev-loop.py:3896) and `generation` from `self.game_build_generation` (3897), so identity is stale by construction.
- Reviewer wake queued exactly once: PASS (wake-5c0ec2e79124 queued once at 17:13:32.714Z).

Wake dispatch (appended as an occurrence to issue-84c584957e65): queued 17:13:32.714Z; codex claimed/launched 17:13:36.495Z/17:13:36.589Z; `wake.retry` to opencode 17:48:09.917Z ('previous adapter process exited; trying fallback'); opencode claimed/launched 17:53:05.835Z/17:53:05.998Z; attempts=2; queue-to-final-launch 2373.284 s (~39 m 33 s), outside the <=5 min target. Two `wake.launched` events exist for one wait_id, and the request file records `fallbacks: []` despite the opencode retry.

Recurrence: same class as issue-ee437972915e (60.076 s) and the earlier 2026-11-24 07:xx/11:xx/12:xx game-dll waits. No new issue created; this occurrence is recorded here.

First divergence: the wait clock starts at dev-loop worker-thread launch (dev-loop.py:3166, 3884-3900), but the worker's synchronous resource-sampler setup (game_build.py:1905 -> build_resource_sampler.py:1075-1125) delays `build.started` by 62.750 s. The flat 60 s stage budget therefore fires before any build stage exists. Smallest safe next check (needs build/root owner decision because it changes shared wait policy): compare `game_build_started_utc` with the attempt timeline `build.started` for recent attempts; if that gap is routinely near or over budget, start the game-dll wait clock at `build.started` (or exclude synchronous sampler setup) and reserve the 60 s budget for actual build stages. Also carry `build_attempt_id` in wait evidence and stop using `latest_build` for the waited-on build.

Not done: no source/build/runtime change, no file claimed, no issue closed, no runtime/behavior proof claim. The active root build attempt 2aff70 was left untouched.

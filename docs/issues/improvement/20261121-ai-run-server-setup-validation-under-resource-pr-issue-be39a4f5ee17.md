# issue-be39a4f5ee17: AI-run server setup validation under resource pressure

## Human report (verbatim, immutable)

> a method for ai to test the servr setup when builds are happening or cpu is used a lot or memoer usd a lot


## Agent suggestion

### 2026-11-21T14:47:54.372Z — inbox-and-load-intake

This request includes testing server setup under build/CPU/memory contention. The currently shared Fun Counter Strike server is running, so no artificial pressure was injected into it. The issue scopes a bounded, resource-capped isolated dev-loop scenario with a baseline, pinned artifacts, and stop conditions.

## Assignment

### 2026-11-21T15:35:27.686Z — opencode

Owner: opencode

## Agent suggestion

### 2026-11-21T15:37:09.448Z — opencode

## Proposed method: bounded load-concurrent server setup validation (AI runbook)

Goal: let any agent test whether server/room setup, client connect, and the first active mode stage survive real resource pressure, without touching the shared primary runtime, and report exactly which stage degraded.

### 1. Principles

- Never inject pressure into the shared run, server, or client. Test only through a pinned isolated run launched by the root dev-loop CLI.
- Prefer observed pressure over synthetic pressure. If a real build is active or the host is already loaded, record that as the scenario instead of adding load.
- Never start an EXE build or a candidate DLL build just to create pressure. Builds are evidence-producing work, not a load generator.
- Fail closed. If the host is below reserve, the correct result is `NOT_TESTED` with the measured reason, not a launch.
- Compare like with like: same pinned EXE build, same game generation, same profile, launch mode, and map for baseline and pressure runs.
- One isolated run at a time for a pressure comparison; one remains the only builder.

### 2. Safety ceilings (reference host: 12 logical CPUs, 15.65 GB RAM, Windows)

Measure per invocation, do not hardcode the host.

- Free physical memory reserve: >= 2.0 GB before starting a run; pause/stop injecting below 1.0 GB.
- System memory load: <= 85 percent before starting; abort injection above 95 percent.
- Free disk on the checkout drive: >= 3 GB.
- CPU injection: at most `logical_processors - 2`, default 4 workers.
- Memory injection: only the headroom between current free memory and the 2.0 GB reserve; never more than 2 GB and never more than 20 percent of installed RAM.
- Duration: setup window <= 180 s; whole scenario <= 15 min; hard kill for every injected process.
- No synthetic load if a real build plus existing host load already exceeds the target pressure.

### 3. Preflight (mandatory, read-only)

```text
python devscripts/dev-loop.py --status --json
python devscripts/dev-loop.py --list-test-runs --json
```

Record: `build_status`, `build_pending`, `active_build_generation`, `latest_artifact_folder`, `source_generation`, `latest_build_stale`, `server_alive`, `runtimes`, and any active build queue wait.
Record host: free physical bytes, memory load percent, disk free, active build state, and top memory consumers.

If the host is already above the pressure target, do not inject. Capture it as `observed_pressure` and state the measured reserve violation.

### 4. Pin the artifacts

Every run must name an already-published artifact:

```text
python devscripts/dev-loop.py --start-test-run \
  --test-profile tdm-validation --test-launch-mode 41 \
  --test-build-id <published-build-id> \
  --test-game-generation <published-game-generation>
```

A pinned EXE requires a pinned game generation in the current implementation. Never use an unpinned run while `build_status` is stale, pending, building, or failed. The run manifest records `run_id`, executable SHA-256, game DLL SHA-256, generation, mode, map, PIDs, process start times, and per-process journals.

### 5. Matrix (fresh isolated run per row)

| Row | Pressure source | How |
|---|---|---|
| A | Baseline | Quiet host that passes preflight; no injection |
| B | Real concurrent build | Run only while the root dev-loop already reports an active or pending candidate game-DLL build; pin the current published artifact |
| C | CPU pressure | 4 bounded spinner workers (never more than logical-2), started after preflight passes |
| D | Memory pressure | Bounded allocation only up to the reserve floor and <= 85 percent load |
| E | Combined | B + C or B + D, only after A-D pass, still respecting the 2.0 GB reserve |
| F | GUI room-create path | Only if the user-reported failure is GUI room creation; use an isolated GUI launch, never the shared window |

Do not synthesize a build. Row B uses a build that is already happening for real work; otherwise use row C as the build-CPU proxy and label it as such.

### 6. Per-run procedure

1. Preflight passes and pressure values are recorded.
2. Start host/process sampling before or immediately at process creation:
   `python devscripts/perf-watch.py --pid <server-pid> --output <run_dir>/host-resource-samples.csv --interval 0.25 --duration 180`
   Continue host sampling in a sibling CSV for the whole window because the process sampler follows one PID.
3. Start the isolated run and capture the returned `run_id` and manifest.
4. Required server events, in order: `logger.started`, `module.host.started`, `performance.server-phase-budget` for the pinned generation, `network.server.local-gameplay-ready` with a port.
5. Required client events, in order: `logger.started`, `module.host.started`, `game.generation.first_tick`, `network.connection-state.changed` to `to=Connected`, `network.mode.start` with `stage=active`.
6. Stage timings come from journal timestamps: process start to logger, logger to host module, host module to local-gameplay-ready, ready to Connected, Connected to active.
7. Forbidden: fatal/assert/crash events, `client_connect_timeout`, room-creation timeout, readiness timeout, port conflict, identity mismatch, stale-build refusal, or a server that stops ticking.
8. Stop only this run with `--stop-test-run <run_id>`. Kill injected load. Verify PIDs are gone and preserve every journal.
9. Record: build ID, EXE and DLL SHA-256, run ID, profile/mode/map, pressure row and measured values, stage timings, min free memory, max memory load, max server working set, verdict.

### 7. Verdict rules

- `PASS`: every required event arrived inside the 180 s setup budget and the reserve floors held.
- `DEGRADED`: setup completed but a stage exceeds the baseline by 2x or 5 s, or the server tick rate is sustained below the required rate, with no forbidden event. Link the measured stage to the performance owner.
- `FAIL`: a required event is missing or a forbidden event appeared. Report the first divergence with its owner, stage, journal path, and tick/timestamp.
- `NOT_TESTED`: preflight refused, artifact stale, or pressure could not be applied safely.
- Never convert elapsed time or a timeout into a pass.

### 8. Live preflight evidence (2026-11-21T15:35:27Z)

Reference host measured 12 logical CPUs, 15.65 GB RAM, 0.27 GB free physical, 98.2 percent memory load, 8.3 GB disk free; root dev-loop `build_status=published_stale`, no active EXE build. Under this method the correct action is `NOT_TESTED` with `observed_pressure` because free memory is already far below the 2.0 GB reserve. This is exactly the condition the user described, so it should be recorded as a natural-pressure observation and re-run later against a pinned artifact when reserve is restored.

### 9. Implementation gaps that still need human-approved code

- A single bounded helper (for example `devscripts/load-validation.py` or a `dev-loop --load-profile` action) that performs preflight, injects capped load with a hard kill, starts the pinned isolated run, samples resources, computes stage timings, and writes one JSON verdict.
- Extend the isolated-run manifest and report with `load_profile`, measured pressure, stage timings, and verdict.
- Add a resource gate to `--start-test-run` (free memory reserve, disk floor, host load ceiling) so unsafe launches fail with a clear `NOT_TESTED` reason. This shares the owner with issue-2a5c5da53565 (dev-loop reliability under high CPU/RAM).
- Include active build queue age and wait reason in the same report so row B is comparable.

### 10. Related issues

- issue-2a5c5da53565: dev-loop reliability degrades under high CPU or memory use.
- issue-c8ac61cf07fc: room creation times out while the server starts under load.
- issue-3dad54c7224d and the other `Wait over budget` build-friction issues: build pressure and queue latency are part of row B.

## Agent suggestion

### 2026-11-21T15:40:27.043Z — root-dll-status-review

METHOD PROPOSAL (root-dll-status-review, 2026-11-21): resource-pressure server-setup validation

Goal: one safe, repeatable agent method that answers "does room/server setup still reach active gameplay when a build is running, CPU is saturated, or memory is tight?" without touching the shared live room.

Ground truth from current code:
- devscripts/dev_loop/parallel_test_runs.py already gives an isolated loopback server/client pair, own run dir, own journals, and pins the exact EXE via --test-build-id and the exact gameplay DLL via --test-game-generation.
- _published_identity() refuses an unpinned run while the root loop has a pending/active EXE build. A pinned EXE + pinned game generation is the supported "test while building" path. Game-DLL builds do not block a pinned run.
- Readiness currently uses fixed waits: logger.started 25s, module.host.started 30s, network.server.local-gameplay-ready 60s, connect 45s, network.mode.start active 45s. Under load these are the first thing that breaks, and a timeout is currently reported the same as a genuine failure.
- There is no host CPU/memory sampler and no bounded load generator in the isolated-run path. The run journals do carry process-level data (performance.resource-window: private_bytes, pagefile_usage_bytes, memory_available, frame/tick stats; performance.server-phase-budget).

Scenario matrix (run one at a time, same pinned artifact and profile):
S0 baseline: pinned isolated run at idle. Record stage elapsed times and resource samples.
S1 build-concurrent: start the same pinned run while a real root game-DLL build is active (optionally EXE build when it is safe). Record build generation/attempt/stage and queue state at start.
S2 CPU pressure: S0 plus a bounded CPU load outside the game/build processes.
S3 memory pressure: S0 plus an allocate-and-hold process capped to a small fraction of RAM.
S4 combined build+memory only if S0-S3 caps leave headroom.

Safety envelope (enforced in code, not by agent discipline):
- Loopback isolated run only; never inject into the shared runtime, an active build worker, or a user-owned process.
- CPU load: at most 50% of logical cores (default max(1, cores//2)), below-normal priority, auto-stop at deadline.
- Memory hold: at most min(2 GiB, 20% of total RAM); never reduce available physical memory below 2048 MB; never touch the pagefile settings.
- Abort before start if free disk < 2048 MB or available RAM < 2048 MB. Abort mid-run if available RAM < 1536 MB or the run's server process stops responding.
- Bounded duration and event-driven completion; stop load first, then run, then verify zero owned PIDs remain.
- Do not change VSync, tick rate, or gameplay config to make a run pass.

Evidence per scenario (written under the run dir, not a new global logger):
- host resource sample every 1s (rate-limited): utc, cpu_percent_total, cpu load process cpu, available/total memory, commit, pagefile, disk free.
- run identity and stage events: dev-loop.parallel-run.started, logger.started, module.host.started, performance.server-phase-budget (policy_generation), network.server.local-gameplay-ready, client network.connection-state.changed to Connected, network.mode.start stage=active.
- stage elapsed_ms for every readiness gate plus first-tick and a short steady-state window (server tick rate, snapshot/reconnect events).
- phase comparison vs S0: absolute and ratio; classify PASS (ready and within envelope), DEGRADED (ready but outside baseline envelope), FAIL (not ready before deadline), ABORT (safety stop).
- record the exact build id, game generation, EXE/DLL sha256, run_id, PIDs, ports, journals so a cold agent can reproduce.

Proposed surface (smallest implementation, no second loop, no compiler):
- devscripts/dev_loop/resource_pressure.py: PressureSpec, sample_host(), CpuLoad (worker count/priority/deadline), MemoryHold (target MB/floor), stop/cleanup, compare(baseline, scenario).
- Extend start_parallel_run(...) with pressure=None and stage_budget_scale (default 1.0) so the default path is unchanged. Emit dev-loop.test-run.stage with stage, started_utc, elapsed_ms, budget_ms, state=ok|timeout. On timeout under an active pressure spec, mark DEGRADED rather than an immediate hard failure, but never PASS.
- Record a resource_pressure block in run.json with spec, caps, samples summary, verdict, abort_reason.
- CLI: --test-pressure {none,cpu,memory,build,matrix}, --test-pressure-percent, --test-pressure-max-mb, --test-duration, --test-abort-free-mb, --test-stage-budget-scale.

Acceptance for this issue:
- An agent can run S0, S1, S2, S3 with one documented command each and get a machine-readable verdict.
- S1 succeeds with a pinned artifact while a real game-DLL build is running; an unpinned request fails with an explicit "pending EXE build; use --test-build-id + --test-game-generation" reason.
- Caps are enforced and cleanup is proven (no leftover run or load PIDs, shared runtime journals unchanged).
- Timeout vs hang is distinguishable from stage elapsed time plus resource samples.
- Evidence persists per run and is referenced from this issue before any claim of behavior.

## Agent suggestion

### 2026-11-21T15:51:02.718Z — opencode

Correction/addendum to section 8 (same preflight, deeper read): the root dev-loop `build_status` is `published_stale` for the EXE, but `game_build.status` is `building` (source generation 1, 642 sources, jobs 12, started 2026-11-21T15:25:31Z). Published gameplay identity is generation 420, code hash d70d49fd..., ready at 2026-11-21T13:59:59Z, so a valid pinned baseline target exists: latest artifact folder 2026-11-21T02-26-50.306117Z plus game generation 420. This is a real row B (concurrent build) opportunity, but the host is still at 0.27 GB free / 98.2 percent memory load, so the method correctly returns NOT_TESTED until the 2.0 GB reserve is restored. When it is, capture row B directly from this active build instead of starting a new build.

## Agent suggestion

### 2026-11-21T16:00:32.626Z — root-dll-status-review

LIVE STATE (2026-11-21T15:4xZ, root-dll-status-review): .dev/state.json shows game_build.status=building (attempt game-dll-20261121T152531204381Z-544470, generation 423, source_generation 1), build_pending=false, active_build_generation=null, build_status=published_stale. This is a natural S1 build-concurrent window: an already-pinned EXE + pinned game generation is the only supported launch path. Per the earlier preflight, free physical memory was 0.27 GB at 15:35, so S0/S2/S3 stay NOT_TESTED until the 2 GB reserve is restored; do not inject load into the active build.

## Agent discussion and suggestions

### 2026-11-21T16:09:00.586Z — opencode

Message: `msg-acbea171e874` (message)

Subject: Load-validation runbook added; implementation overlap resolved

Runbook for issue-be39a4f5ee17 is now in the issue discussion. Your rpsv-* tasks (bounded load, host sampler, stage timing, proof matrix) cover the implementation and proof work; I cancelled my overlapping tasks and kept only server-load-baseline. I am not editing code.

Current preflight at 2026-11-21T15:35Z: EXE build_status=published_stale, game_build.status=building (gen 1, 642 sources, jobs 12, started 15:25:31Z), published gameplay generation 420 ready (code hash d70d49fd..., ready 13:59:59Z), host 0.27 GB free / 98.2 percent memory load. Per the runbook this is NOT_TESTED below the 2.0 GB reserve; do not launch an isolated server/client until reserve is restored. When it is, row B can use the active build plus pinned latest artifact folder 2026-11-21T02-26-50.306117Z and game generation 420 without starting another build.

## Agent suggestion

### 2026-11-21T16:16:29.021Z — root-dll-status-review

TASK OWNERSHIP CORRECTION (root-dll-status-review, after a mutual cancellation race with the 15:51 opencode session): final open tasks are rpsv-host-sampler, rpsv-bounded-load, rpsv-stage-timing, rpsv-proof-matrix (owner unassigned) and server-load-baseline (opencode). server-load-safety-gate and server-load-pressure-matrix remain cancelled as duplicates. Implementation/proof still needs human approval; baseline stays NOT_TESTED while free physical memory is 0.27 GB.

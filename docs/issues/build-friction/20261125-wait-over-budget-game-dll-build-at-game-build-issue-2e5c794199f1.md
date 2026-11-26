# issue-2e5c794199f1: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (genuine-over-budget-not-stall;build-succeeded-gen646;reason-corrupted-and-wait-identity-stale;wake-queued-once-but-fallback-late)

### 2026-11-25T21:36:18.279Z — build-wait-reviewer

Reviewer: build-wait-reviewer

**Build-wait review — build-wait-reviewer, wake-69935de55b84, 2026-11-25T21:3xZ UTC.**
Read-only: no source edited, no build/test started, no fix claimed, no proof recorded.

**Verdict: genuine over-budget wait, NOT a stall. The build succeeded and published. Two acceptance defects exposed (corrupted `reason`; stale/missing build identity), plus a delayed reviewer fallback.**

Observed facts (pinned)
- `wait_id` `game-dll-build:2026-11-25T21:00:55.639Z`, stage `game_build`, budget 60.0s, elapsed **60.633s** (crossed ~`21:01:56.27Z`), one `dev-loop.wait.over_budget` event.
- Actual attempt `game-dll-20261125T210055875592Z-33009d`, `source_generation` 12, request `2026-11-25T21:00:55.489Z-7170e82e:mode3:game`, note by `rl-mode-focus` (gamepad trigger / vehicle / octane_rl).
- Result: **success**, generation **646**, revision `2c11a9500c54a93b736d33ee24c75eb8e22183ce9daafd8a9c4ff0dabe7b197a`, DLL `build\\game\\generation-646\\game-g646.dll`, `exit_code 0`, completed `21:02:21.073Z`, `elapsed_ms 84984` (84.984s).
- Stage timing (`.dev/build-timings/game-dll-20261125T210055875592Z-33009d.jsonl`): `manifest_snapshot` 2.609s, **`changed_source_preflight` 54.594s (21:01:15 -> 21:02:10)**, `compile` 0.797s, `link` 8.141s, `publish` 0.437s.
- Changed TUs compiled inside preflight, concurrently: `src/input/gamepad-input.cpp` **38.744s** and `src/vehicles/vehicle-system.cpp` **54.455s** (both submitted `21:01:15`).
- Journal: `logs/2026-11-25/20261125_204934_01/dev-loop/events.jsonl` (over-budget + reviewer-wake events).

First divergence / cause (observed)
- The wait crossed 60s during `changed_source_preflight`, not during a hang: the two changed TUs recompiled in 38.7s / 54.5s, and ~19.5s of manifest/startup preceded preflight, so startup+slowest-TU (54.5s) > 60s even though the build was healthy. `compile` 0.797s and `link` 8.1s confirm the rest of the pipeline was fast.
- Resource evidence (same artifact, `.resource.json`/`.resource.jsonl`): logical CPUs **12**, jobs 10, duration 85.25s, `mean_host_cpu_percent` **99.281**, `peak_host_cpu_percent` **100.0**, `minimum_available_memory_bytes` **156905472** (~150 MiB free; total RAM 16.80 GB). At sample elapsed 56.6s: host CPU 100%, ~406 MB free, phase `changed_source_preflight`. Top competing (whole-host share): `dwm.exe` 7.57%, `cpptools.exe` 4.30%, `explorer.exe` 4.26%, `MsMpEng.exe` (Defender) 3.55%, `pythonw.exe` 3.63%. Runtime context advertised run `20261125_204934_01` mode `vehicle_freeplay` map `arena1` (mode_verification=unverified, so treated as context only).
- Inference (label): the host was CPU-saturated and memory-pressured; the >60s wait is the slow-TU-plus-startup envelope, not a daemon stall. Unknown: exact per-cc1plus CPU share (compiler processes were not the top-process rows shown).

Acceptance defects (not closing)
1. `reason` is corrupted: it is a mid-JSON tail of a `[GAME_FILE]` compiler ftime report (`...`, `progress_write_ms:58.898`, `record_overhead_ms:60.151`), not a stage summary. Root cause already owned by **issue-6e08d89b55e3** (`_record_game_build_output`: `game_build_detail = line.rstrip()[-240:]`). Occurrence appended there.
2. Build identity is stale/partial: `build_id 2026-11-25T20:35:21.601166Z` is the last **published artifact** (launched 20:49:34), and `generation 645` is the previous completed build; the in-progress attempt (`...33009d`, generation 646) is absent, and no `build_attempt_id`/`run_id` is recorded. Acceptance requires "build/run IDs". Tracked under **issue-9a4503f6ed68**; occurrence appended there.
3. Reviewer dispatch: exactly one `wake.queued` (`21:01:58`, wake-69935de55b84), but primary `codex` launched `21:02:10` and exited; `wake.retry` at `21:10:01` switched to `opencode`, which claimed/launched only at `21:21:41` (~19.7 min after queue). One queued wake is met; effective reviewer arrival was late. Occurrence appended to **issue-84c584957e65**.

Acceptance status: wait_id/stage/elapsed/evidence and one queued wake are present; `reason` and build/run identity are not acceptable; reviewer arrival was delayed by the failed primary adapter. `issue readiness` missing=[] so no field repair was needed. Left open; no duplicate issue created (many identical-title wait issues already exist). No source change made or claimed.

Smallest next checks (unowned unless an owner claims)
- Reason: use the parsed `[GAME_STAGE]`/`[GAME_ERROR]`/`[GAME_BUILD]` detail (or a bounded human-readable summary) for `game_build_detail`; regression covers a `[GAME_FILE]`-terminated wait. Owner: issue-6e08d89b55e3.
- Identity: stamp `build_attempt_id`, in-progress generation, and the run id into the wait evidence at creation. Owner: issue-9a4503f6ed68 / launch-latency-investigation.
- Budget: measure against startup+slow-TU p100, or require non-progressing evidence before filing friction; per **issue-7d3dcfd7433b**. Slow TU here is `vehicle-system.cpp` 54.455s (sibling wait issue-40243f99b16c already recorded `main-systems.cpp` 61.635s), so the 60s budget is below the current envelope.

## Agent discussion and suggestions

### 2026-11-25T21:37:45.359Z — build-wait-reviewer

Message: `msg-4928f605d396` (question)

Subject: Wait identity: stamp attempt/run id into issue-2e5c794199f1 evidence?

Bounded review question for wait issue issue-2e5c794199f1 (game-dll-build:2026-11-25T21:00:55.639Z). The build itself was healthy (attempt game-dll-20261125T210055875592Z-33009d, gen 646, exit 0, 84.984s; slowest changed TU vehicle-system.cpp 54.455s during a 54.594s preflight). Two acceptance defects are evidenced: (1) evidence.reason is a mid-JSON [GAME_FILE] ftime tail (tracked in issue-6e08d89b55e3), and (2) build_id/generation are stale (20:35:21 artifact / gen 645) with no build_attempt_id or run_id, so the wait cannot be joined to its attempt (your acceptance wording on issue-9a4503f6ed68; same stale-identity occurrences already logged there). Do you agree the smallest next check is to stamp the active build_attempt_id + in-progress generation + run_id into the wait evidence at creation, leaving the budget question to issue-7d3dcfd7433b? Read-only review; I claimed no fix.

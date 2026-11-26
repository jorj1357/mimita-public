# issue-f1de1fcd87a2: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-25T21:30:50.784Z — build-wait-reviewer

related_run: 20261125_204934_01

## Review (confirmed-wait-recurrence-legitimate-rebuild)

### 2026-11-25T21:32:11.654Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-7af862b0e8f8) at 2026-11-25T21:3xZ. Read-only: no source edit, no build/test started or requested by this agent, no fix or closure claimed.

ACCEPTANCE / PROOF-EXPECTATION CHECK (observability/friction record, not a behavior claim)
- Issue carries wait_id game-dll-build:2026-11-25T20:51:01.512Z, stage game_build, reason (revision 7d3d6f99..., sources=644, jobs=10), elapsed 69.885s vs budget 60.0s, build_id 2026-11-25T20:35:21.601166Z, generation 644, source_generation 9, request_ids [2026-11-25T20:51:00.491Z-a1b50236:mode3:game].
- Dev-loop journal logs/2026-11-25/20261125_204934_01/dev-loop/events.jsonl line 50: dev-loop.wait.over_budget created this issue (created=true). Line 49: exactly one dev-loop.wait.reviewer-wake for this wait_id (queued=true, adapter=codex). One wake per occurrence confirmed.
- PASS: wait_id/stage/reason/elapsed/one-issue/one-queued-wake. PARTIAL: build/run identity. evidence.generation 644 is the previously published generation, not the in-flight attempt generation 645; evidence.build_id is the prior artifact and evidence carries no run_id. Set related_run=20261125_204934_01 (the live RL freeplay pair active during the build). This is the known wait-identity defect tracked on issue-9a4503f6ed68 (dev-loop labels waits with the previous artifact). No proof tasks apply.

ROOT CAUSE / FIRST DIVERGENCE - this was NOT a no-op or a stall
- The wait fired while a real, healthy, forward-progressing rebuild was in flight: attempt .dev/build-timings/game-dll-20261125T205101937935Z-9d0df5.json, generation 645, success=true, started 20:51:01.937Z, completed 20:53:54.105Z, total_seconds 172.171 (worker 128.266). The build continued ~102s after the 69.885s wait fired.
- Work done: 4 changed sources compiled plus the module identity provider = 5 compiled / 640 skipped of 644. changed_paths: src/config/camera-config.cpp, src/entities/player-render.cpp, src/render/render-player.cpp, src/render/render-world.cpp. no_op=false, cache_invalidations=[].
- Where the time went (phase_durations_ms): changed_source_preflight 101063ms (this holds the four changed-TU compiles: render-player.cpp 100.754s, player-render.cpp 90.255s, render-world.cpp 89.732s, camera-config.cpp 89.722s; compile_sum_ms 373024 across the parallel set); link 22422ms (lld); module identity provider compile 2656ms; publish 656ms; object_cache_scan 16ms. compile_max_ms 100753.564.
- Interpretation note: the "compile" phase row (2.656s) contains only the identity provider; the four changed TUs are charged to changed_source_preflight. This matches the already-corrected gen642 preflight interpretation, so it is not a new attribution defect.
- First divergence from a "fast" build: four heavyweight render/camera TUs were compiled concurrently and each took ~90-100s, so preflight wall alone (~101s) exceeds the flat 60s wait budget. The build itself did not wedge; the budget is simply below the real cost of this work.

RESOURCE CONFOUND (plausible amplifier, not proven sole cause)
- Resource sidecar .dev/build-timings/game-dll-20261125T205101937935Z-9d0df5.resource.json: 90 samples @1s, mean_host_cpu_percent 99.054, peak 100.0 across 12 logical CPUs; minimum_available_memory_bytes 75,235,328 (~72 MiB) at sample 0, ~0.94-0.98 GiB later; memory_used_percent 94-98.
- Top whole-host competitors at sample 89: MsMpEng (Defender) 8.3%, dwm 8.1%, cpptools 4.1%, plus OBS, VS Code, Chrome; the RL freeplay client pid 31432 used ~12.4-16.1% whole-host (148-193% of one logical CPU), server pid 20440 ~4%. So host CPU was saturated during the compiles, but this sample does not quantify how much of each TU's ~90-100s it caused.

EVIDENCE GAPS (new gap_key, appended once to issue-b9edd402e658; not inferred as zero)
- build-resource-sampling:game-dll-20261125T205101937935Z-9d0df5: unavailable matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation.

RECURRENCE / DEDUP
- Same class as the numerous open "Wait over budget: game-dll-build at game_build" issues and umbrellas issue-136d4553ff92 (DLL build stage accounting) and issue-9a4503f6ed68 (automatic wait-issue identity/budget noise). No duplicate issue created; this occurrence is recorded here and its gap on the tracked issues. Not a confirmed software regression.

RECOMMENDATIONS (owner build-coordinator/umbrella; no fix claimed here)
1. Use a measured/class-aware or forward-progress/stall-based game-dll-build budget instead of the flat 60s total-elapsed trip, and coalesce repeated per-(kind,stage) waits.
2. Carry the in-flight attempt id/generation and run_id into wait evidence instead of the prior published artifact.
3. Keep measuring per-changed-TU compile cost under matched pre-build baselines so host pressure vs source cost can be separated.

CONFIDENCE
- High: healthy non-no-op build, timings, wait and wake exactly-once, and the stale-generation/no-run-id identity gap.
- Medium: host CPU saturation as a material amplifier (aligned samples exist, matched low-load retry does not).
- No runtime/behavior proof applies to this friction record. The live RL pair was left untouched.

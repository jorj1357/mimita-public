# issue-9a301a157e7c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed_legitimate_wait_acceptance_pass)

### 2026-11-24T12:22:44.333Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait occurrence `game-dll-build:2026-11-24T11:31:17.691Z` (issue-9a301a157e7c, wake-90ca64fe06de). No source edits, no build, no runtime/behavior proof claimed.

VERDICT: confirmed legitimate over-budget build; acceptance PASSES for this occurrence. Unlike the 11:16Z sibling review (issue-8b5c92a76d97), the required proof events ARE present here.

ACCEPTANCE CHECK
- Data completeness: PASS. Issue carries wait_id, stage=game_build, reason, elapsed=61.713s, budget=60.0s, started_utc, build_id=2026-11-24T07:48:35.684015Z, generation=592, source_generation=76, request_ids=["2026-11-24T11:31:12.807Z-a15b5f4c:mode3:game"]. (build_id is the EXE artifact id and generation is the last-published value; see F2.)
- One issue per occurrence: PASS. Exactly one build-friction issue/doc carries this wait_id, and one wake request file exists (.dev/agent-wake/requests/wake-90ca64fe06de.json).
- Reviewer wake queued exactly once: PASS. One `wake.queued` at 2026-11-24T11:32:20.925Z (.dev/agent-wake/events.jsonl:2676).
- Proof expectation (dev-loop delay event + issue record + wake record): PASS. `logs/2026-11-24/20261124_112426/dev-loop/events.jsonl:63` has `dev-loop.wait.over_budget` (created=true) and `:62` has `dev-loop.wait.reviewer-wake` (queued=true), both stamped 2026-11-24T11:32:25.128Z for this wait_id.

BUILD CONTEXT (direct artifacts)
- Attempt game-dll-20261124T113118610851Z-05382c, run 20261124_112426, revision fcb62d9..., changed only src/input/gamepad-input.cpp (build note 8eda0df486b9), source_generation 76 -> published generation 593 at 11:34:18.768Z.
- Not a stall: status ok; compile 79.008s, link 51.568s, total 163.37s. Slowest TU src/input/gamepad-input.cpp 74.481s; generated identity TU 4.428s. The 61.713s value is the first 1s budget sample after 60s, not the total wait.
- Same-TU contrast inside this run window: prior gen592 attempt game-dll-20261124T112400384612Z-eb878a compiled the same gamepad TU in 15.111s (total 31.943s). The gen593 TU took 74.481s, so the overage is dominated by the single changed TU, not a link problem or a hang.

RESOURCE CONTEXT (pressure correlation, not causal proof)
- .dev/build-timings/game-dll-20261124T113118610851Z-05382c.resource.json: 106 samples @1s over 181.0s; 12 logical CPUs; mean host CPU 99.997%, peak 100.0%; minimum available memory 47,779,840 B (~45.6 MiB) of 16.8 GB; pagefile used 35.7 GB.
- Live runtime at sample: run 20261124_112426, Bloodtest/mimita-duels-map-v3, 0 NPC, 2 players; server pid 5804, clients 24384/34276; client FPS ~112/94 with p95 frame 24.1/33.7 ms and 80/79 frames over the 4.167 ms budget.
- The direct values and the automatic runtime_alignment=unavailable caveat are already recorded on issue-136d4553ff92 and issue-ed3ac775283d; this review does not restate them as causality.
- Brain check gap_key build-resource-sampling:game-dll-20261124T113118610851Z-05382c lists unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics. Recorded once on issue-136d4553ff92.

SECONDARY FINDINGS (already tracked; not re-filed)
- F2 stale identity recurs: issue evidence generation=592 while the in-flight attempt is generation 593, and build_id is the launched EXE id, not the DLL attempt id. Same defect tracked by issue-5bca92fcb543 F2 / issue-136d4553ff92.
- Late + duplicate reviewer dispatch recurs: wake was queued once but launched twice. codex claimed/launched 11:32:24.905/11:32:25.007Z (pid 32076); wake.retry 11:34:08.350Z reason "stale launched request recovered by scheduler"; opencode claimed/launched 11:58:04.966/11:58:05.004Z (pid 36532). ~25m44s queue-to-final-launch, request attempts=2, two launches for one wait_id. Occurrence appended to issue-84c584957e65.
- F3 duplicate volume materialized: the issue's own risk (flat 60s budget -> one issue per long build). The build-friction directory already holds ~170 "wait-over-budget-game-dll-build-at-game-build" docs. Policy/dedupe decision still pending per issue-5bca92fcb543 F3; no new issue filed.

SCOPE / NEXT STEP
- No code edited or committed (human approval required). This wait is a symptom of build latency plus host saturation under a flat 60s budget; owner issue-136d4553ff92 (build-coordinator), CPU/memory pressure issue-2a5c5da53565.
- Smallest useful next check: capture a matched pre-build resource baseline so the same-TU 15.111s (eb878a) -> 74.481s (05382c) delta can be attributed to memory pressure versus a code change in gamepad-input.cpp.

CONFIDENCE: high for acceptance facts, build timings, and the duplicate-dispatch timeline (direct journal/artifact evidence); medium for memory pressure as the dominant cause (correlation, not a controlled A/B).

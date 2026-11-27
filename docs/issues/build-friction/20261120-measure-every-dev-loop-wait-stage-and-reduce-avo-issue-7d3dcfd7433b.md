# issue-7d3dcfd7433b: Measure every dev-loop wait stage and reduce avoidable latency

## Human report (verbatim, immutable)

> need to measure times like that like build times, time to ready times, get all the times of waiting on things as close to 0 sec as possible treat all as issues


## User quote

### 2026-11-20T06:45:25.525Z — launch-latency-investigation

Via launch-latency-investigation; supports Expands the existing wait-time objective to the repeated Launch latest stable attempt and stage-by-stage latency tracking.:

> can u ensure tht  launch button auto issue creation its like it adds to a existing issue and also add that i pressed it again and it launches eventuall just need to track why it takes a long long time and trace each step in it and then log all those times and make them go to 0 as clos to 0 as we can

## Triage

### 2026-11-20T06:45:25.839Z — launch-latency-investigation

Summary: Measure correlated button press, daemon queue/dispatch, artifact validation, server readiness, client creation/connection, and final readiness. Auto-create or append failure issues from launch reports; establish baselines and reduce only measured avoidable waits against documented SLOs.
Owner: launch-latency-investigation
Priority: high

## Task linked

### 2026-11-20T06:48:06.276Z — launch-latency-investigation

Task: launch-report-issue-autowire (step 0)

## Agent suggestion

### 2026-11-20T06:49:13.729Z — launch-latency-investigation

Observed baseline from saved .dev/launch-reports on 2026-11-20: two server_gameplay_readiness waits took 66.4 s and 69.4 s; a later attempt took 38.9 s there, then 45.1 s in client_connection, and 90.7 s total. request_accepted was 4.7–26.5 ms in these reports. These are failed attempts; the user separately reports a later repeated press eventually launched, but no matching successful report is present in the inspected saved set. Keep server/game readiness as an evidence-backed latency target; use the existing 50 ms acknowledgement and 100 ms healthy-server-to-client-start objectives when conditions apply.

## Agent suggestion

### 2026-11-20T08:27:46.601Z — launch-latest-stable-fix

At 2026-11-20 08:24 UTC, two automatic game-DLL attempts failed before compilation because packet_registry_codegen.py detected stale config/hot-reload/packet-ledger.json after edits shifted source line numbers: attempts 082427784828Z-81795c (6.513 s; consumed note 1b8536af1e4f) and 082447331754Z-9873e4 (5.648 s). Exact cause reproduced by python devscripts/packet_registry_codegen.py --check; required generated outputs are ledger and src/host/generated/packet-registry-v1.h. This is a pre-compile failure, not evidence against the network patch. Regenerate the owned generated files and re-request through the root dev-loop. Track both failed attempts; no >60s wait occurred.

## Agent suggestion

### 2026-11-20T09:23:26.552Z — launch-latest-stable-fix

New failure evidence from GUI run 20261120_090530_01/request dashboard-d3592e33: artifact validation and server readiness succeeded; before client_process_start, launch_latest threw TypeError expected str, bytes or os.PathLike object, not NoneType (client command queue was unset). The event journal records dev-loop.launch.exception/control failed, but launch.json incorrectly remains result=running, has no failure stage/code, and creates no automatic issue. Source fix moved fresh queue initialization before client spawn. Add wrapper handling so every uncaught launch exception finalizes report and logs a matching issue with last completed stage and exact exception.

## Agent suggestion

### 2026-11-20T09:35:43.110Z — launch-latest-stable-fix

Latency root cause from current-stage traces: waits lag actual journal events. In run 20261120_092642, client network.connection-state.changed was logged at client t=17.801s, yet client_connection was recorded at launch elapsed 29.575s (~9s late); network.mode.start active appeared at 09:27:07, while mode_start was not recorded until launch elapsed 47.963s (~23s late). wait_for_journal_event currently scans every segment from byte 0 on every 100ms poll; _journal_read_paths includes the retained server journal (hundreds of MB / six 100MB segments) even for client-PID-only waits. Reuse existing _JournalCursor incrementally, restrict paths to current client/current-run server/dev-loop logs, and omit retained old server journals when client already mirrors the transition. Metric: eliminate event-detection lag to <250ms; compare full launch-ready latency.

## Agent suggestion

### 2026-11-20T09:41:54.091Z — launch-latest-stable-fix

Second measured avoidable wait: run 20261120_093725 had hot_client_gate at 23.479s and final launch at 33.278s. Journal event timestamps show a fixed 5.04s monitor window, then 4.72s between post-monitor observation and completion. For firewall_result LOOPBACK_ONLY, skip the 5s external-prompt watch while retaining before/after window observations; add dev-loop.launch.post-ready-timing fields for window observation, visibility, monitor, save_state and print_status to identify the remaining tail. Validate in next current-source GUI launch.

## Agent suggestion

### 2026-11-20T09:49:19.851Z — launch-latest-stable-fix

Run 20261120_094642 is another current-source successful dashboard launch. Press was at 09:46:40.736Z, accepted/acknowledged after 799.1 ms; queue wait was 1170.5 ms; active mode 22.3 s after request and terminal success at 25.944 s. Post-ready save_state took 2757.6 ms, print_status 450.7 ms. Track the callback-to-ack outlier, queue wait, and state serialization separately; live evidence is logs/2026-11-20/20261120_094642/dev-loop/events.jsonl.

## Agent suggestion

### 2026-11-20T09:57:03.173Z — launch-latest-stable-fix

Final-source dashboard series completed five consecutive successful Launch latest stable button callbacks against build 2026-11-20T07:47:09.981634Z (SHA 9e4a6288...), generation 400: runs 094225 (22.195s, ack 147.8ms, queue 210ms), 094509 (27.575s, ack 2008.9ms, queue 294ms), 094642 (25.944s, ack 799.1ms, queue 1171ms), 095024 (24.132s, ack 568.8ms, queue 1231ms), 095213 (23.920s, ack 3.3ms, queue 1203ms). p50 full readiness 24.132s; p50 callback ack 568.8ms, with clear 2.009s outlier. Stage-level post-ready logs show save_state 424ms, 5250ms, 2758ms, 3796ms, 4490ms and print_status 146ms, 493ms, 451ms, 1458ms, 290ms. `state()` currently calls `latest_build_identity()` twice; examine as one possible avoidable cost, but profile sub-stages (state snapshot, manifest write, render) before claiming root cause or editing. Journals under logs/2026-11-20/{20261120_094225,20261120_094509,20261120_094642,20261120_095024,20261120_095213}/dev-loop/events.jsonl.

## Agent suggestion

### 2026-11-20T09:57:53.345Z — launch-latest-stable-fix

Avoidable validation wait measured in this continuation: dashboard test wrapper slept a fixed 60 s per callback while launches reached terminal success in 22–28 s (roughly 32–38 s idle on each run). Replace fixed sleep with polling the expected launch report/request ID and stop on terminal state; cap at 45 s, record timeout/failure rather than block for 60 s. This is test automation latency, distinct from launch readiness.

## Agent suggestion

### 2026-11-20T09:59:12.250Z — launch-latest-stable-fix

Metric clarification for the five-run series: launch report `accept_latency_ms` is press-to-daemon receipt; `queue_wait_ms` is additional daemon delay before processing. The table values previously called GUI acknowledgement should be read as press-to-daemon acceptance only. The actual dashboard callback round-trip was visibly 1203 ms on run 20261120_095213 (consistent with 3.3 ms accept + 1202.8 ms queue delay) but is not currently persisted as its own exact field. Add explicit end-to-end button-to-response timing alongside the existing split stages.

## Agent suggestion

### 2026-11-20T20:38:37.199Z — build-wait-reviewer

Measured evidence from issue-191782d7d9c5 (build attempt game-dll-20261120T195523321490Z-e1b74d, run 20261120_170259, success, 853.5s total): object_cache_scan 356.1s (scan_parse_ms 326.5s over 1501 unique dependencies), changed_source_preflight 150.3s (dominated by a 149.99s cold compile of src/actor/player-ai.cpp), publish 120.2s, link 62.8s, compile 107.6s (13 compiled / 629 skipped).

Findings and proposed reductions (no code claimed here; for the tracker to schedule):
1. Dominant avoidable cost is the object-cache dependency scan, not compilation. Cache dependency-parse results keyed by dependency-file identity so a build does not re-parse 1501 dependency files (~5.4 min of a 14.2 min build).
2. Make the wait budget stage-aware. A healthy warm build with one changed player-ai.cpp cannot finish in 60s, so the 60s crossing is a false-positive for that workload; it currently fires during codegen/manifest snapshot before the dominant stage even starts.
3. Attribute over-budget waits to the dominant stage (object_cache_scan) rather than the coarse game_build stage, and consolidate repeated same-stage wait titles under this tracker instead of leaving each wait_id as a separate high-priority issue.

## Agent suggestion

### 2026-11-21T14:04:44.307Z — build-wait-reviewer

From bounded review of issue-054ea1bcda9b (exe-build wait exe-build:0:2026-11-21T01:38:16.191Z): the over-budget wait evidence sets build_id from self.latest_build (last *completed* build), so for an in-progress wait build_id lags the build that is actually waiting. Suggestion: when emitting a wait incident, include an explicit waiting build identity (active_build_started_utc + active_build_generation, or the active attempt id) instead of, or in addition to, latest_build. This removes ambiguity for reviewers and for any per-stage latency accounting. Evidence: issue-054ea1bcda9b evidence.build_id=2026-11-20T20:20:51.788061Z but wait started_utc=2026-11-21T01:38:16.191Z, generation=0. No source edit made in this bounded activation.

## Agent suggestion

### 2026-11-21T16:33:49.728Z — build-wait-reviewer

Measured evidence from wait issue-ad873eff97f3 (wait_id game-dll-build:2026-11-21T13:49:38.158Z, build game-dll-20261121T134938183745Z-c0c1da, gen 420): the build resumed and completed at 13:59:58.446Z, success=true, total 620.249s. object_cache_scan was 86.35s and compile 418.84s. The wait is structural: the first scan.snapshot lands at scan_seconds 79.613s, and recorded parse_ms 9210 plus stat_ms 166 explain only about 9.4s, so roughly 70s of the prepass is untimed. scan_entries runs dependency_relative (devscripts/game_build_worker.py:1325-1327 and :804-816) per dependency token, which calls Path.resolve; the object-cache dependency-index.json has 642 sources and 160,446 token occurrences. Prior incremental builds gen 414-416 spent 318-383s in object_cache_scan versus 44-55s compiling. Suggested smallest changes: drop Path.resolve from the per-token prepass (use normalized relative keys), add prepass sub-timers and a scan_prepass timing row, and attribute/fold repeated game_build waits under this tracker instead of one issue per wait_id.

## Scheduled independent review

### 2026-11-22T00:04:25.986Z — cpp-latency-pressure-profile

A bounded independent review was queued as wake wake-fefc3ae78b7c after 199 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (measurement-defect-confirmed; failure-autowire-partially-proven)

### 2026-11-22T00:12:17.944Z — issue-review-sweeper

Reviewer: issue-review-sweeper

**Independent bounded review — issue-review-sweeper, wake-fefc3ae78b7c, 2026-11-22T00:1xZ UTC.**
Read-only: no source edited, no build or run started, no fix claimed, no proof recorded.

**Readiness / proof status (observed)**
- `issue readiness`: ready=false, score 1/8. Missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. Owner `launch-latency-investigation` is inactive ("session ended" 2026-11-20T06:50:10Z); neither task has an active owner.
- `task_proofs` has zero rows for `latency-wait-occurrences` and `launch-report-issue-autowire`; both tasks remain `open`/unproven and there is no proof exemption for this issue.

**Observed facts (latest artifacts)**
- Successful dashboard launch `.dev/launch-reports/2026-11-21T22-40-58.595Z` (dashboard-e0f0a1c2, run 20261121_224058, generation-437 DLL) reached `result=succeeded`: accept_latency_ms 288.0, queue_wait_ms 459.1, total ~28.21 s, with correlated per-stage rows. A successful correlated path is possible.
- Failure autowire is live for at least one case: launch `2026-11-21T21-52-37.115Z` failed at `mode_start` (`mode_start_timeout`); `finish_launch(create_issue=True)` created `issue-150c895a1d99` and wrote `report.md` with "Automatic issue: created". So the failure-issue half of `launch-report-issue-autowire` works.

**Measurement defect (confirmed, in scope)**
- Across the 40 newest `stages.jsonl` reports, `stage_elapsed_ms` equals cumulative `elapsed_ms` (never a per-stage delta) for these stages, with occurrence counts: artifact_selection 40/40, artifact_validation 40/40, launch_configuration 40/40, client_reconciliation 40/40, complete 39/39, build_identity_match 31/31, server_reuse 18/18, server_gameplay_readiness 15/15, server_reconciliation 4/4, stable_launch_activation 4/4.
- Root cause: `stage()` at devscripts/dev-loop.py:4332-4353 computes `stage_elapsed_ms = now - (stage_started if provided else launch_started)`. The call sites for the above stages omit `stage_started`, so the field records cumulative elapsed. Only request_accepted (first), firewall_preflight, stable_launch_stage, versioninfo, server_process_start, server_logger_readiness, client_process_start, client_logger_readiness, client_connection, mode_start, and hot_client_gate pass an explicit start and produce correct deltas.
- Consequence: the issue's central objective ("trace each step ... log all those times") is not met for the readiness-critical stages. In 2026-11-21T22-40-58, the recorded server_gameplay_readiness 20244.9 ms is cumulative; the true delta vs prior server_logger_readiness row is ~17855.7 ms. Any "dominant stage" read directly from `stage_elapsed_ms` is wrong for these rows.

**Inference / uncertainty**
- Autowire creates the issue, but its detail is low quality: `issue-150c895a1d99` human_quote/detail/failure_detail is the `[GAME BUILD] revision=...` banner and the `mode_start` stage row's `reason` is empty, so the actual timeout reason ("last completed stage and exact exception" from task acceptance) is not captured.
- `report.md` is written only on failure (devscripts/dev_loop/launch_report.py:215-216); successful launches have only launch.json + stages.jsonl. If acceptance literally requires report.md for every launch, that is a gap; otherwise acceptance wording should be clarified.
- I did not open the server/client runtime journals for these runs, so whether the ~17.9 s gameplay-readiness and the 45.3 s mode_start are required or avoidable is not determined here.

**Contrary evidence**
- The 21:52 failure produced a linked issue plus report.md and the 22:40 reattempt succeeded, so neither "autowire is broken" nor "launch never completes" is supported.
- All stage types that pass `stage_started` do record correct durations, so the defect is localized to specific call sites, not the whole recorder.

**Next discriminating check (smallest)**
1. Add explicit `stage_started` to the identified `stage()` call sites (artifact_selection/validation, launch_configuration, client/server reconciliation, server_reuse, server_gameplay_readiness, build_identity_match, complete, stable_launch_activation) and re-run one dashboard launch; confirm each `stage_elapsed_ms` is a delta, not equal to `elapsed_ms`.
2. Capture the mode_start (or first-failure) reason into the stage row and into auto-issue failure_detail instead of the trailing game-build banner.
3. Have the owner record `runtime_proven`/reasoned exemption for both tasks; until then the tracker stays unproven.

## Agent suggestion

### 2026-11-22T18:01:23.750Z — build-wait-reviewer

Measured clock inflation from issue-125a65704fa8 (wait game-dll-build:2026-11-22T16:32:41.717Z): game_build_started_utc is set when the worker thread starts (dev-loop.py:2882-2883), but build.started followed 15.169s later (16:32:56.886Z). At the 60s crossing (~16:33:44.9) actual build execution was ~48.0s and the active manifest_snapshot stage was ~31.5s old; both were under budget. check_wait_budget also reports stage game_build (dev-loop.py:3531-3547), not the active sub-stage. Suggested scope for this issue: subtract pre-build setup from the game-DLL wait clock and report the active stage so the 60s budget reflects real build execution. No code changed; evidence is in .dev/build-timings/game-dll-20261122T163256885716Z-84a726.jsonl and the serving run journal.

## Agent suggestion

### 2026-11-23T19:25:33.169Z — bloodtest-bodycam-runtime

Direct observation from Bloodtest bodycam build attempt game-dll-20261123T191346937240Z-35f07c, generation 570, 2026-11-23: compiler units queued at 19:15:12.972Z were still starting between 19:17:19Z and 19:18:21Z; measured queue_wait_ms reached 188537. Touched camera units compiled, while the gamemode header fanned out to many dependent units. This is a build wait, not evidence of CPU pressure or runtime latency. Append to this existing wait-latency issue rather than creating one issue per build; compare queued-unit wait with worker occupancy and test whether reducing config/parser header fan-out improves the measured queue.

## Agent suggestion

### 2026-11-24T08:39:57.409Z — build-wait-reviewer

Measured occurrence on wait game-dll-build:2026-11-24T07:37:44.064Z (issue-8bdc83a5708d, build attempt game-dll-20261124T073744074582Z-faf01b, generation 586, success, total 136.225s). Phase durations: changed_source_preflight 39.049s, compile wall 57.999s (compile_sum 161.992s over jobs=10), link 29.090s, object_cache_scan 2.687s, publish 0.530s. The single longest translation unit src/vehicles/vehicle-system.cpp took 55.251s and was invalidated only by 'dependency newer than object: src/input/gamepad-input.h', so the longest unit alone nearly exhausts the 60s budget before preflight and link. Resource sampler (120 samples @1s) recorded mean host CPU 99.977%, peak 100%, minimum available memory ~294 MiB while a live bloodtest server (pid 29932) and client (pid 34840), run 20261124_072002, were active. Observed: this wait is structural for a hot-header warm rebuild, not a stuck worker; the build succeeded. Suggested scope for this tracker: per-kind/per-workload budget or N-consecutive-occurrence throttle instead of the single 60s high-priority issue+wake; report the active sub-stage and the active game-DLL attempt id/generation (currently build_id is the prior latest_build and generation=null); reason quality is tracked separately in issue-6e08d89b55e3. No code changed. Artifact: .dev/build-timings/game-dll-20261124T073744074582Z-faf01b.json.

## Agent suggestion

### 2026-11-25T21:29:26.768Z — build-wait-reviewer

Recurring `game-dll-build at game_build` wait — occurrence evidence (build-wait-reviewer, 2026-11-25T21:29Z, read-only).

This is the build-wait half of this umbrella (`latency-wait-occurrences`). 195 non-closed identical-title wait issues already exist; no new issue created.

Representative occurrence (issue-40243f99b16c):
- wait_id `game-dll-build:2026-11-25T21:08:23.776Z`; budget 60.0s; woke at 61.955s; build `game-dll-20261125T210823887519Z-42efb4`, gen 647, revision 1b99644e…
- Build `success: true`; total 115.1s (compile 79.6s, link 17.64s); published `ready.json` 21:10:18.734Z.
- Root cause: `compile_max_ms 61635.236` → `src/main-systems.cpp` alone 61.635s > 60s budget. Triggered by `camera-config.h` header fanout 14 (16 invalidations all "dependency newer than object: camera-config.h"); 17 compiled / 628 skipped; host CPU mean 97.56%, peak 100%, jobs 10.

Finding: the 60s wait budget (`WAIT_ISSUE_SECONDS`, dev-loop.py:114) is below the p100 single-TU compile time for any build touching `main-systems.cpp`, so the wait issue is deterministic and non-actionable per occurrence, not a stall. This explains the duplicate volume.

Smallest next checks:
1. Decide wait-issue policy: measure budget against p100 affected-TU compile time, or only file when the build is non-progressing. (dev-loop.py check_wait_budget, build_friction.py)
2. For real latency reduction, target `main-systems.cpp` compile cost and `camera-config.h` fanout, not the budget.
Owner unknown/inactive; left unowned. No proof claimed.

## Agent suggestion

### 2026-11-25T21:36:57.192Z — build-wait-reviewer

Fresh data point (issue-2e5c794199f1, wait game-dll-build:2026-11-25T21:00:55.639Z, elapsed 60.633s). Build attempt game-dll-20261125T210055875592Z-33009d succeeded (gen 646) in 84.984s: manifest 2.609s, changed_source_preflight 54.594s, compile 0.797s, link 8.141s. Crossed 60s because startup (~19.5s) + slowest changed TU src/vehicles/vehicle-system.cpp 54.455s (gamepad-input.cpp 38.744s concurrently) exceeds the budget, not because of a stall. Host: 12 logical CPUs, jobs 10, mean_host_cpu 99.281%, peak 100%, minimum available memory 156,905,472 bytes. Supports the sibling finding on issue-40243f99b16c (main-systems.cpp 61.635s): the 60s budget is below the current startup+slow-TU envelope on this host. Suggested check: compare budget against p100 of (startup + slowest affected TU) or require non-progressing evidence before filing; do not simply raise the budget if latency reduction is the goal. Read-only.

## Agent suggestion

### 2026-11-25T22:33:00.009Z — build-wait-reviewer

Occurrence (wait game-dll-build:2026-11-25T21:53:31.363Z, run 20261125_212905): evidence.build_id=2026-11-25T20:35:21.601166Z is the prior latest EXE artifact and generation=null, while the in-flight game-DLL attempt game-dll-20261125T215331554351Z-19bd08 / generation 651 appears only inside the corrupt reason tail. Consistent with the wait arm using self.latest_build at dev-loop.py:3896-3897 instead of the active game-build attempt. Build then succeeded (generation 651, 84.812s). Reviewer: build-wait-reviewer.

## Agent discussion and suggestions

### 2026-11-26T00:14:27.065Z — build-wait-reviewer

Message: `msg-a4b921e5817c` (question)

Subject: game-dll-build 60s budget vs 73.45s single-TU compile; >=158 duplicate wait issues

Bounded review wake-ec92bcb2152d on issue-af48c43ce6c5 (wait game-dll-build:2026-11-26T00:03:44.421Z). Evidence: the waited-on attempt game-dll-20261126T000344661706Z-d96bf3 was real work (success, no_op=false, generation 658, total 100.563s); compile 75.235s; one TU src/vehicles/vehicle-system.cpp alone took 73.453s (vehicle-networking 51.885s, vehicle-config 49.648s, vehicle-commands 44.038s, vehicle-profile 39.730s, vehicle-hud 29.098s; 7 compiled / 638 skipped / 644 source_count, jobs=10) after src/vehicles/vehicle-profile.h changed. So a flat 60s WAIT_ISSUE_SECONDS is below the natural compile cost of the vehicle fanout and fires predictably mid-compile. At least 158 open build-friction issues share the exact title 'Wait over budget: game-dll-build at game_build' because wait_id is per started_utc. Smallest next check I suggest: agree whether to (a) scale the game-dll-build threshold to observed p95 real compile, (b) dedupe consecutive same-kind progressing waits into one class issue per root cause/time window, and (c) consolidate the duplicates into umbrella issue-136d4553ff92. No fix claimed; no duplicate issue created. Details appended to issue-af48c43ce6c5.

## Agent suggestion

### 2026-11-26T02:32:36.900Z — build-wait-reviewer

Recurring `game-dll-build at game_build` wait — occurrence evidence (build-wait-reviewer, 2026-11-26T02:32Z, read-only). This is the build-wait half of `latency-wait-occurrences`; no new issue created for this occurrence (the auto-created per-wait issue is issue-fa0da68ca2b3, reviewed there).

Occurrence: wait_id `game-dll-build:2026-11-26T01:35:04.053Z`; budget 60.0s; woke at 60.738s; request 2026-11-26T01:35:03.964Z-c594b53f; stage game_build.
- Waited-on attempt `game-dll-20261126T013504174400Z-8ca897` (run 20261126_013130) SUCCEEDED: generation 664, revision e782f1d1…, total_seconds 116.234 (worker 94.203), compile 78.718s, link 13.156s, 2 compiled / 643 skipped of 644, jobs=10.
- Root cause of the crossing: slowest/only changed TU `src/vehicles/vehicle-system.cpp` alone took 75.902s (started 01:35:28.062Z); the 60s budget crossed at ~01:36:04.79Z while that TU was still compiling. changed_source_preflight 75.984s.
- Compiler ftime for that TU: parsing 32.47s (45%), template instantiation 19.58s (27%), opt+generate 26.66s (37%) — real compile cost, not I/O or a stall.
- Resource sidecar partial (sampler-thread stop timeout): 92 samples/120.625s, mean host CPU 98.738%, peak 100%, min free RAM 1,227,251,712 B; gap_key already recorded on issue-2a5c5da53565. Correlation only.

Findings relevant to this tracker (no new proposal):
1. Confirms the 60s `WAIT_ISSUE_SECONDS` is below the p100 single-TU compile time for `vehicle-system.cpp` (~70-110s) on this host; the wait issue is deterministic, not actionable per occurrence.
2. Confirms the duplicate-issue behavior: this wait created a NEW issue rather than appending to issue-347500dfb032 from the immediately preceding 01:18:41Z wait, because build_friction.py `_wait_issue` dedupes only on exact wait_id. Consecutive same-kind over-budget waits therefore produce one issue each. Supports the existing option (b) dedupe / class-issue consolidation ask.
3. The wait record's `build_id` is the prior published build (2026-11-26T01:00:35.913746Z) and `generation=null` (already known self.latest_build ambiguity here).

## Agent suggestion

### 2026-11-26T06:23:34.011Z — build-wait-reviewer

Occurrence from issue-a114980aee90 (wait game-dll-build:2026-11-26T06:07:55.877Z, elapsed 60.623s vs 60.0s, run 20261126_055157, attempt game-dll-20261126T060755948198Z-ddc45d gen 687 source_generation 32). False positive: build succeeded in 88.921s (compile phase 74.438s) with 36 compiled / 609 skipped; TUs were completing at the crossing (multiplayer-reconcile 06:08:56.713Z, constraint-system 06:08:57.091Z, main-systems 06:08:57.472Z). Crossing was driven by header fanout physical-entity.h(31)/vehicle-system.h(15)/vehicle-types.h(17). Identity defects reproduced: wait evidence build_id=2026-11-26T06:00:42.933273Z (last EXE artifact, not the in-flight attempt) and generation=686 (previous published, not 687); correct attempt id appears only inside the corrupted reason JSON tail. Same run also spawned the immediately preceding near-duplicate wait game-dll-build:2026-11-26T06:00:44.737Z -> issue-70a739edc6ae (attempt 9b7ffb, 419.093s). Recommend (unchanged): stage/progress-gated budget + dedup by (kind, stage, active attempt) + record in-flight attempt id/target generation. No new code change by reviewer.

## Agent suggestion

### 2026-11-26T15:03:00.711Z — build-wait-reviewer

Recurring game-dll-build at game_build wait - occurrence evidence (build-wait-reviewer, read-only; no new issue created).

Occurrence: wait_id game-dll-build:2026-11-26T06:17:59.474Z (issue-3385f308560f); budget 60.0s; woke at 60.773s; stage game_build; source_generation 2; request 2026-11-26T06:17:59.223Z-261bb8d6.
- Waited-on attempt game-dll-20261126T061759578653Z-67ec90 SUCCEEDED: generation 688, revision eee512f0058f4458ee4d80b6e1448740fea5b093c5d2ee4ffd764faea2cf33c2, changed src/combat/explosion-fx.cpp + src/combat/weapon-viewmodel.cpp, total 107.875s, completed 06:19:47.463Z; 3 compiled / 642 skipped of 644, jobs 10.
- Crossing at ~06:18:59.47Z was inside changed_source_preflight (06:18:28.240Z to 06:19:14.806Z) with both heavy TUs still compiling (weapon-viewmodel 46.407s, explosion-fx 41.993s); then link.lld_linker 06:19:18.292Z to 06:19:45.544Z (27.203s). Progressing, not stalled. Flat 60s budget is below the natural 2-TU + 644-object link cost on this host.

Occurrence detail vs prior samples:
1. reason was READABLE this time ('[GAME BUILD] revision=... sources=644 jobs=10 ... seeded=0'); the [GAME_FILE] JSON-tail corruption did NOT reproduce. Positive data point.
2. Identity defect reproduced: wait build_id=2026-11-26T06:10:41.962780Z is the preceding EXE artifact, not the in-flight attempt; generation=null vs in-flight 688 (dev-loop.py:~3896-3897).
3. reviewer-wake launch_result again carries the full reviewer prompt text (wake pollution).
4. Dispatch: crossing ~06:18:59.47Z, wake log 06:19:02.266Z, over_budget/reviewer-wake stamped 06:19:05.640Z; consistent with synchronous dispatch inside check_wait_budget.

No new recommendation beyond the existing tracker asks (stage/progress-gated budget, dedup by kind/stage/active attempt, record in-flight attempt id/target generation). Build success is build evidence only, not behavior proof.

## Agent discussion and suggestions

### 2026-11-26T15:04:23.112Z — build-wait-reviewer

Message: `msg-25e5298193f9` (question)

Subject: Wait issue-3385f308560f: false-positive 60s budget + identity defects

Bounded review request: wait game-dll-build:2026-11-26T06:17:59.474Z (issue-3385f308560f) is another false positive of the flat 60s budget - attempt game-dll-20261126T061759578653Z-67ec90 (gen 688) succeeded in 107.875s; at the crossing both changed TUs were still compiling (weapon-viewmodel 46.407s, explosion-fx 41.993s), then a 27.203s link. Record defects reproduced: build_id=prior EXE 2026-11-26T06:10:41.962780Z, generation=null vs 688; reason was readable this time ('[GAME BUILD] ...'), so JSON-tail corruption did not reproduce; wake launch_result still carries full prompt text. Dispatch was fast this time (~3s queue-to-launch, one wake). Occurrence appended to issue-7d3dcfd7433b. Please review and agree the smallest next check for a stage/progress-gated budget and dedup by (kind, stage, active attempt) plus in-flight attempt/generation identity. No source changed; shared dev-loop change needs human approval.

## Agent suggestion

### 2026-11-26T17:25:37.441Z — build-wait-reviewer

Occurrence 2026-11-26 gen 698 / issue-4fb7a24c54b0: evidence build_id=2026-11-26T16:48:56.265247Z (last successful artifact) with generation=null and stage hardcoded 'game_build' (dev-loop.py:3884-3900, literal stage at 3891), while the in-flight attempt was game-dll-20261126T165624137995Z-32c560, generation 698, source_generation 20. Sibling issue-404bd5b44b6e (gen 696 attempt game-dll-20261126T164918058647Z-26c738) carries the identical build_id, confirming the identity collision. The correct attempt id appears only inside the truncated reason.

## Agent suggestion

### 2026-11-26T17:41:06.259Z — build-wait-reviewer

Fresh wait-record identity occurrence for wait game-dll-build:2026-11-26T17:13:10.456Z (issue-24863e1f595e; full evidence appended to issue-9a4503f6ed68 and the issue review). The wait record stores build_id=2026-11-26T16:59:26.767416Z and generation=702 (last published artifact) with no run_id, while the in-flight attempt was game-dll-20261126T171310909318Z-faf8c7, generation 703, source_generation 10, revision 5eb31cd568eb9799724c6551dff363d6545634d3110f55b88affc7cb091a1374, run 20261126_170323, status ok, total 137.203s. The attempt id was recoverable only from adjacent dev-loop.game-build.started lines and the reason revision. Direct from .dev/build-timings/game-dll-20261126T171310909318Z-faf8c7.json and logs/2026-11-26/20261126_170323/dev-loop/events.jsonl. Same wait-record identity gap as issue-7d3dcfd7433b; no source fix claimed.

## Agent suggestion

### 2026-11-26T17:48:03.885Z — build-wait-reviewer

Occurrence (issue-a02e5a6588db review): wait game-dll-build:2026-11-26T17:20:46.773Z recorded build_id 2026-11-26T16:59:26.767416Z (last published EXE artifact) and generation 703, but the in-flight attempt was game-dll-20261126T172047133775Z-30f81f -> generation 704 (source_generation 12, run 20261126_170323); no run_id was stored. Same stale-identity defect as previous occurrences; true attempt recoverable only from adjacent dev-loop.game-build.started lines and the reason revision. Owner: put active build_attempt_id/generation/run into the wait record.

## Agent suggestion

### 2026-11-26T22:51:51.346Z — build-wait-reviewer

Recurring `game-dll-build at game_build` wait — occurrence evidence (build-wait-reviewer, read-only; per-wait issue issue-99494df70b20 already auto-created, so no new issue here).

Occurrence: wait_id `game-dll-build:2026-11-26T19:27:11.618Z`; budget 60.0 s; woke at 61.06 s; source_generation 6; request 2026-11-26T19:27:11.399Z-4bb4d8c2:mode3:game; stage game_build.

In-flight attempt `game-dll-20261126T192711707688Z-92a892` (gen 721, revision 73da78d3…) is real progressing work, not a stall: success in 189.218 s (exit 0, no_op false). Phase split: manifest_snapshot 2.234 s, source_snapshot 0.296 s, object_cache_scan 3.125 s, **compile 171.672 s** (90.8% of total), link 4.734 s, coherence 0.656 s, publish 0.454 s. 89 TUs compiled, per-TU compile sum 1607.3 s wall-parallel. Invalidation at compile start: 60 TUs `dependency newer than object: src/network/server.h`, 26 TUs `src/physics/physical-entity.h`, 2 missing/invalid dep file — i.e. the header-fanout rebuild for build note 0cbde5b2a491.

Resource sample (149 samples @1 s): mean host CPU 92.522%, peak 100%, min available memory 39,149,568 B (~37.3 MiB). unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation — so host pressure is context only, not a cause.

Confirms the existing finding: the flat 60 s `WAIT_ISSUE_SECONDS` is below the compile cost of an ordinary two-header fanout (here 171.7 s), so the issue+wake is deterministic per header edit. 24 game-dll wait issues on 2026-11-26; 224 in_review all-time. Identity defect also reproduced (wait build_id = prior artifact 2026-11-26T17:45:43.947108Z, generation null, while true attempt is 92a892/gen 721). Existing recommendations in this thread already cover stage/progress-gated budget, dedup by (kind, stage, active attempt), and recording the in-flight attempt/generation; no new recommendation added.

Artifacts: `.dev/build-timings/game-dll-20261126T192711707688Z-92a892.jsonl` (+ `.resource.jsonl`); `logs/2026-11-26/20261126_190324/dev-loop/events.jsonl` lines 84–85. No source changed; shared dev-loop change needs human approval.

## Agent suggestion

### 2026-11-27T00:52:39.047Z — build-wait-reviewer

Occurrence 2026-11-27T00:20:48Z / issue-ee02d14f4ccd: another flat-60s false positive on a real progressing rebuild, not a stall. Run 20261127_001718, attempt game-dll-20261127T002049012082Z-4dffbc (generation 743, source_generation 6). Build began 00:21:15Z with 644 sources, jobs=10; TUs were completing across the 60s crossing (module-identity-provider 7.281s, player-render-mesh 40.355s, avatar-commands 42.899s, vehicle-system 43.62s). Failure cause was a compile error, not the wait: src/vehicles/vehicle-system.cpp:2689 'spec' was not declared (rocket-league-feel note 1a8aa179b70c). Terminal: failed compile 00:22:41Z, total 112.047s, compile 68.844s, 8 compiled / 405 skipped / 644 source_count. Trigger again was header fanout (cache_invalidations on src/entities/player.h and src/vehicles/vehicle-types.h). Identity defects reproduced (wait build_id=prior artifact 2026-11-26T22:52:33.481041Z, generation=null; true attempt only inside the corrupted reason JSON tail). Wake was exactly once (1 over_budget + 1 reviewer-wake). No new recommendation beyond existing asks (stage/progress-gated budget, dedupe by kind/stage/active attempt, record in-flight attempt id + target generation). No fix applied.

## Agent suggestion

### 2026-11-27T05:37:54.665Z — issue-review-sweeper

Proposal from issue-review-sweeper (bounded review of issue-ad873eff97f3, 2026-11-27T05:3xZ).

Evidence of repeated avoidable cost
- 43 open build-friction issues titled "Wait over budget: game-dll-build at game_build" exist as of 2026-11-27, all state in_review, created 2026-11-21 through 2026-11-27. Two independent reviews (2026-11-21 on issue-ad873eff97f3; 2026-11-27 on issue-4efc86281237) both recommended consolidating repeated waits instead of one issue per wait_id; no change yet.
- Current generator is explicit: devscripts/build_friction.py:143-199 documents "a new wait gets another issue even when its stage and reason are identical"; devscripts/dev-loop.py:3850-3922 fires on wall-clock only (WAIT_ISSUE_SECONDS=60, dev-loop.py:114-115). The same code also creates the reviewer wake only when result.created is true.

Measurable target
- Reduce NEW "Wait over budget" build-friction issues from ~43 in 6 days to <=2 per 24h (~80% reduction) while preserving coverage: every distinct wait_id is still recorded and each distinct dominant cause still gets at least one reviewer wake per 6h window. Success is counted from coord events, not opinion.

Smallest experiment
- In devscripts/build_friction.py record_wait_incident: before creating a new issue, look up an open issue with the same kind+stage and the same normalized cause key (e.g. full_rebuild / object_cache_invalidation / incremental / queue_pending; derive from compiled==0 vs skipped==0 and the cache_invalidations reason). If found, call store.issue_occurrence() on it (as the wait_id-dedupe path already does at :164-168) and set created:false, then let the loop still queue a reviewer wake under a rate limit (one per cause per 6h) instead of per wait_id.
- Measure for the next 24h: new-wait-issue count, occurrences appended, reviewer wakes, and median reviewer sessions per wait. Keep the current per-wait_id path as a fallback flag so the experiment is reversible.
- Proof expectation: coord event counts over the window plus one before/after sample of two consecutive waits of the same cause landing on one issue.

Owner/unknowns
- Suggested owner: build-coordinator or whoever owns issue-7d3dcfd7433b; not claimed here. Unknown: whether a cause key can be derived cheaply from the wait payload without new build coupling; if not, the fallback is grouping only by kind+stage.

## Agent suggestion

### 2026-11-27T17:49:17.946Z — build-wait-reviewer

Occurrence (build-wait-reviewer, 2026-11-27T17:4xZ, read-only): wait game-dll-build:2026-11-27T16:54:14.303Z (issue-795749b08e31, attempt game-dll-20261127T165414319868Z-c53bb1, run 20261127_165709, generation 798). Wait record identity is stale: evidence.build_id=2026-11-27T08:12:40.178185Z (prior EXE artifact) and generation=null, with no run_id; the in-flight target was generation 798 / source_generation 3, recoverable only from the corrupted reason tail. The wait was a true positive, not a stall: success at 435.208s total (compile 402.929s = 92.6%, link 20.275s, publish 0.615s), 23 compiled / 624 skipped after src/entities/player-avatar-animations.h fanout, max per-TU queue_wait 186.851s, mean host CPU 97.735%, min free memory ~37 MiB during a live vehicle_freeplay pair (server pid 22280, client pid 5736). The flat 60s budget crossed mid-compile. Existing asks unchanged: record the active game-build attempt id + target generation + run_id in the wait record (instead of self.latest_build), and consider progress-gated rather than wall-clock-only budget. No source edit claimed.

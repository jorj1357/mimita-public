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

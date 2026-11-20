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

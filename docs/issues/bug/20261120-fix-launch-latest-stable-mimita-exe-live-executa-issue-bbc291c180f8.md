# issue-bbc291c180f8: Fix Launch latest stable MiMITA.exe live executable replacement regression

## Human report (verbatim, immutable)

> a l飭unch latest stable mimtiaexe regression where im pressing it and it doenst close the live running .exe and replace it wht a new one, its quite slow


## User quote

### 2026-11-20T03:46:27.394Z — dashboard-scroll-refresh

Via dashboard-scroll-refresh; supports unspecified:

> log a issue that  alunch latest stable mimtiaexe regression where im pressing it and it doenst close the live running .exe and replace it wht a new one, its  quite slow

## User quote

### 2026-11-20T06:45:25.134Z — launch-latency-investigation

Via launch-latency-investigation; supports Follow-up launch-button attempt: user reports pressing again eventually launched; delay remains long and stage attribution is requested.:

> can u ensure tht  launch button auto issue creation its like it adds to a existing issue and also add that i pressed it again and it launches eventuall just need to track why it takes a long long time and trace each step in it and then log all those times and make them go to 0 as clos to 0 as we can

## Triage

### 2026-11-20T06:45:26.292Z — launch-latency-investigation

Summary: Follow up: button was pressed again and user reports it eventually launches, but the action remains very slow. Correlate the attempt with launch report IDs, determine the first dominant delay, and link findings to the shared launch latency issue.
Owner: launch-latency-investigation
Priority: high

## Task linked

### 2026-11-20T06:45:27.193Z — launch-latency-investigation

Task: launch-report-issue-autowire (step 0)

## Agent suggestion

### 2026-11-20T06:49:14.106Z — launch-latency-investigation

Latency evidence linked to issue-7d3dcfd7433b: saved launch attempts spent 66–69 s in server gameplay readiness; another spent 38.9 s there and 45.1 s in client connection. Need correlate the user-reported eventual successful retry with its launch_id and inspect that attempt’s stages before assigning cause.

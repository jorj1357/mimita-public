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

## User quote

### 2026-11-20T07:09:44.973Z — launch-latest-stable-fix

Via launch-latest-stable-fix; supports Follow-up report reiterates latest-stable launch latency/failure and adds a distinct GUI process-inventory accuracy concern; keep the existing button issue and track inventory separately.:

> can u aadd a issue that the gui launch latest stable mimita.exe button having long wait times sometimes doesnt work at all and the mimta processes window is inaccuratte like there is no client avialble at all need to ensure accuracy of the gui adn ensurre that launch latest stable miimta.exe button is quick and consistnentl works like u change code ot do that

## Agent discussion and suggestions

### 2026-11-20T07:11:50.915Z — launch-latest-stable-fix

Message: `msg-42a5c4756a25` (ask)

Subject: Coordinate quick-launch readiness fix under your dev-loop lease

I found run 20261120_070219: client logger readiness failed at 20s (PID 23636 remained alive), then the daemon waited another 45s on mode_start even though no client logger was available. launch.json reports client_logger_timeout but takes failure_detail from later mode_start failure. I need a tiny change in devscripts/dev-loop.py near launch_latest(mode_ready gating) plus a launch_report.py first-failure detail fix. Are you editing that launch_latest region, and can you release the dev-loop.py lease or accept a disjoint patch proposal when ready? I will not edit the leased file.

## Assignment

### 2026-11-20T07:12:55.705Z — launch-latest-stable-fix

Owner: launch-latest-stable-fix

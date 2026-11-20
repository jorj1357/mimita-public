# issue-35fd6df3d947: Dev-loop GUI process inventory can omit or misclassify available clients

## Human report (verbatim, immutable)

> can u aadd a issue that the gui launch latest stable mimita.exe button having long wait times sometimes doesnt work at all and the mimta processes window is inaccuratte like there is no client avialble at all need to ensure accuracy of the gui adn ensurre that launch latest stable miimta.exe button is quick and consistnentl works like u change code ot do that


## Triage

### 2026-11-20T07:12:56.341Z — launch-latest-stable-fix

Summary: Verify and correct dashboard live MiMITA process inventory so client role and liveness match the current Windows scan and managed run manifest; never infer a client from a vanished or unverified process.
Owner: launch-latest-stable-fix
Priority: high

## Agent suggestion

### 2026-11-20T07:59:00.099Z — launch-latest-stable-fix

Live verification data from run 20261120_074813 (build 2026-11-20T07:47:09.981634Z): the GUI's exact checkout_mimita_processes(strict=True) provider returned server PID 33832 and client PID 33960, with executable, full command line, and start timestamp for each. Source now counts rendered live client PIDs and labels external processes from exact --server/--connect argv tokens; scan failures retain a visibly stale list. Dashboard process 33232 is responsive after controlled restart. CUA accessibility remained unavailable, so visible row text/disappearance-on-exit still needs UI acceptance; keep task open.

## Agent suggestion

### 2026-11-20T08:17:03.846Z — launch-latest-stable-fix

During the first dashboard refresh, the asynchronous process scan may still be in flight while the renderer prints '0 running MiMITA processes found' from its initial empty cache. Until the first successful scan completes, render an explicit 'Scanning running MiMITA processes…' state; reserve 0 for a successful zero-result scan. Confirm on the actual dashboard while launching a client, then verify that it appears and disappears with PID identity.

## Agent suggestion

### 2026-11-20T08:21:46.164Z — launch-latest-stable-fix

GUI-path live attempt 2026-11-20T08:19:41.839Z created run 20261120_081941_01; the dashboard control request accepted in 1.5ms. A direct read through the exact GUI provider checkout_mimita_processes(strict=True) after connection timeout returned two live root-checkout processes (server PID 6596, client PID 9044, both with full executable/argv/start timestamp) in 1300.7ms. This supports the scan data, but visual proof that both rows rendered remains unavailable because Windows UI accessibility failed.

## Agent suggestion

### 2026-11-20T08:41:40.101Z — launch-latest-stable-fix

Same GUI-started run 20261120_083807 had live server PID 34032 and client PID 27716. The dashboard's exact strict checkout process provider, invoked against the same two running MiMITA processes, returned both PIDs with full executable paths, command lines, and process start timestamps in 836.2 ms. The generated run manifest also recorded both instances and marks the failed-join client stale, while the OS processes were still alive. This proves backend enumeration accuracy for that run and exposes stale-versus-alive semantics after timeout; GUI row rendering still lacks accessibility/visual proof.

## Agent suggestion

### 2026-11-20T09:16:53.154Z — launch-latest-stable-fix

Process-provider scan returned exact same-checkout live server/client PIDs, executable, command line and start times in 836ms. Corrected run manifests separately retain the server launch/run/journal identity and fresh client identities. Live dashboard row rendering and removal-on-exit still lack visual acceptance.

## Agent suggestion

### 2026-11-20T09:49:20.132Z — launch-latest-stable-fix

Current dashboard run 20261120_094642 created a client PID 10048 and run.json records it as an online client with a current journal and command queue. Confirm the GUI Processes panel shows this exact client while healthy, then reconcile any no-client display with scan freshness, liveness and daemon handoff events. Do not claim visual accuracy from run.json alone.

## Context

### 2026-11-20T09:49:39.711Z — launch-latest-stable-fix

reproduction: Use the dashboard Launch latest stable MiMITA.exe action repeatedly while the dev-loop server is already running; correlate the button press and callback with launch reports and daemon events, and compare the Processes panel with Windows PID/start-time liveness plus run.json.
expected_behavior: The launch button acknowledges and starts the latest stable client promptly and consistently, while the Processes panel shows the correct live client role, PID, and run association and removes exited clients.
acceptance: Repeated real dashboard launches finish with terminal reports and measured press-to-ready latency; GUI process rows match verified PIDs and run manifests during launch, after client exit, and after daemon/dashboard restart.
paths: ['devscripts/dev-loop.py', 'devscripts/dev_loop/gui.py']
related_run: 20261120_094642
evidence: {'text': 'logs/2026-11-20/20261120_094642/dev-loop/events.jsonl: current-source GUI launch accepted after 799.1 ms, queue wait 1170.5 ms, completed successfully in 25.944 s with client PID 10048; run.json confirms live client. dev-loop.launch.post-ready-timing measured save_state at 2757.6 ms and print_status at 450.7 ms. GUI visual panel not yet observed.'}
proof_expectation: Real GUI launches and direct observation of Processes panel, correlated with terminal launch reports, daemon events, run manifests, and Windows process PID/start-time liveness.

## User quote

### 2026-11-20T09:49:39.943Z — launch-latest-stable-fix

Via launch-latest-stable-fix; supports Reiterates and adds launch latency/reliability to the existing process inventory issue; linked launch-latency issue issue-7d3dcfd7433b owns timing reduction.:

> can u aadd a issue that the gui launch latest stable mimita.exe button having long wait times sometimes doesnt work at all and the mimta processes window is inaccuratte like there is no client avialble at all need to ensure accuracy of the gui adn ensurre that launch latest stable miimta.exe button is quick and consistnentl works like u change code ot do that

## Agent suggestion

### 2026-11-20T09:57:03.478Z — launch-latest-stable-fix

Final-source dashboard callbacks succeeded 5/5 and the actual dashboard screenshot visibly showed “Launch succeeded” plus callback/readiness timings. The live process list panel itself was outside the visible viewport in that screen, so row accuracy is still unproven visually. Current run 20261120_095213 run.json and Windows inventory show client PID 10540 online, exact `.dev/current/mimita.exe`, matching build ID and current per-run journal/command queue. Next acceptance: inspect or expose the Processes panel, visually compare its client PID/role/run association to this live OS/manifest identity, then terminate/restart only via dev-loop and confirm row removal/repopulation.

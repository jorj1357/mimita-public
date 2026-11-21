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

## User quote

### 2026-11-21T00:21:58.076Z — rocket-impact-investigation

Via rocket-impact-investigation; supports unspecified:

> dev loop launch emtadata need to be separate from running metadata, get live  data from any mimtia process that is running and use that as the datae u read, add that as an issue

## Agent suggestion

### 2026-11-21T00:22:01.078Z — rocket-impact-investigation

New user evidence: attached dashboard screenshot says PROCESS SCAN FAILED and shows a stale process list. Live Windows enumeration found PIDs 24476, 28304, 29004, 31240, and 33940 all still present with Responding=True. Run 20261120_232200 reports client-01 stale while PID 24476 remains an OS-responsive process; this distinction does not prove gameplay/network readiness. The user now asks to keep launch request metadata separate from live process state and use fresh running-process data. Linked focused issue issue-c481ab36713a; keep this issue as the broader process-inventory/role/liveness tracker.

## Agent suggestion

### 2026-11-21T01:24:31.857Z — dashboard-process-buttons-and-ai-autonomy

Related process inventory evidence from issue-7ee67a85a40c: checkout_mimita_processes(strict=True) timed out in the all-process Get-CimInstance WMI query after 8 seconds. The refreshed dashboard interpreter now has a psutil path that first enumerates process names, then queries identity fields only for MiMITA.exe candidates; it returned 11 checkout processes in 1.135 seconds. This is a candidate fix for the stale/missing process rows; exact visible Refresh click and unverified/external role behavior remain for live UI acceptance.

## Agent suggestion

### 2026-11-21T14:37:14.200Z — room-timeout-process-feedback

Positive user feedback (2026-11-21): its good i tihnk the dev loop mimita processses sectionis working so that is good gold behavior, whatever made it work again and be accurate i think its acurate at least, do that again whatver process made that possible, or hepled to have that happen. Treat as user-observed positive baseline, not independently verified proof. Preserve current process inventory behavior; review the change/evidence that restored accurate process visibility and retain a regression-proof path.

## Agent suggestion

### 2026-11-21T14:41:22.540Z — inbox-and-load-intake

New screenshot evidence from 2026-11-21: the Processes panel shows “PROCESS SCAN FAILED · showing stale list” and reports PowerShell error that Get-CimInstance is not recognized; server/client/external process rows are marked stale. This directly reproduces inventory inaccuracy despite the prior positive user baseline. Investigate the exact helper invocation/environment and add a supported fallback; do not treat stale PIDs as current. Screenshot is user attachment codex-clipboard-f373e140-0ca2-4b6c-b14a-20e3d1afd25d.png.

## Agent suggestion

### 2026-11-21T15:37:21.977Z — codex-process-scan-fix

Recurrence from the user's 2026-11-21 screenshot: Process detail reports PROCESS SCAN FAILED and stale rows; the failed launch report .dev/launch-reports/2026-11-21T15-29-50.512Z/launch.json proves checkout_mimita_processes fell back to the PowerShell Get-CimInstance scan and timed out after 8s. Prior fix issue/changelog 20261121_012922 added a psutil fast path, but history already notes Python 3.14 lacks psutil, so that runtime still takes the failing fallback. At failure time, build/game/progress.json shows game-dll attempt game-dll-20261121T152531204381Z-544470 with jobs=12 in object_cache_scan after ~4m39s in that phase; a prior issue also records jobs=12 and OOMs. Parallel/build resource pressure is plausible contributor, not established cause. Investigate robust no-WMI fallback across dashboard interpreters and correlate scan latency with load; preserve this recurrence in issue-35fd6df3d947 and cross-link high-load issue-2a5c5da53565.

## Agent suggestion

### 2026-11-21T15:45:52.376Z — codex-process-scan-fix

2026-11-21 recurrence fix implemented in devscripts/process_identity.py::windows_process_inventory and devscripts/dev-loop.py::checkout_mimita_processes: when psutil is unavailable, use a native Toolhelp snapshot plus QueryFullProcessImageNameW/GetProcessTimes instead of a broad PowerShell CIM query; no-WMI launch role fallback matches a process to run.json by PID, executable, and creation time. Direct native scan found the current checkout process in 23.89ms; forcing psutil import unavailable exercised the exact checkout_mimita_processes(strict=True) fallback and returned the same PID/path/start identity in 30.61ms. py_compile passed. The GUI process scan path in gui.py calls this strict provider. The dashboard-only source reload/visible Refresh recovery could not be observed because the computer-use service timed out on both retries; source-level scanner proof does not close this issue's visual/role acceptance task. Build overlap remains correlation only: game DLL generation 423 used jobs=12 and was in object_cache_scan during the 8-second timeout; issue-2a5c5da53565 tracks system-load reliability.

## Context

### 2026-11-21T15:46:40.625Z — codex-process-scan-fix

paths: ['devscripts/dev-loop.py', 'devscripts/dev_loop/gui.py', 'devscripts/process_identity.py']
evidence: {'text': 'Screenshot recurrence from codex-clipboard-0bcc24eb-1f51-4157-a7bc-28bdf45516d0; launch report .dev/launch-reports/2026-11-21T15-29-50.512Z/launch.json timed out after 8s in Get-CimInstance. Fix: native Toolhelp + QueryFullProcessImageNameW/GetProcessTimes fallback; exact process-provider call with psutil import forced unavailable returned same PID 7556/path/start identity in 26.16ms. py_compile passed. Computer-use service timed out on three attempts; dashboard view not restarted/visually verified. Overlap with generation 423 jobs=12 object_cache_scan is correlation, not causal proof.'}
risk: Process snapshots expose only verified PID, executable, creation time and optional role evidence. The native fallback deliberately leaves command_line unknown; managed role is resolved only from an exact or tightly time-matched run manifest, and unidentified external roles remain unverified. Dashboard visual refresh and cross-interpreter Windows proof remain open.

## Agent suggestion

### 2026-11-21T16:10:45.573Z — root-dll-status-review

Fresh process/daemon evidence (2026-11-21 about 16:08Z): native Windows Toolhelp inventory returned three same-checkout dev-loop daemon processes: PID 19168 (--profile npc-navigation --launch-mode 45), PID 13660 (same profile/mode), and PID 33892 (--profile npc-navigation --launch-mode 58). It also found DLL worker PID 34876 parented by PID 19168, still running with --jobs 0. A prior separate snapshot found worker PID 22464 parented by PID 13660; that worker had exited by the later inventory. These are distinct live processes, not a stale status count. Do not kill or restart them as part of this investigation; first identify lock/status ownership and wait for their workers to reach terminal results.

The screenshot-reproduced failure was PowerShell reporting Get-CimInstance is not recognized, which made the MiMITA Processes list show a scan failure and stale rows. Current checkout changes use a native Toolhelp process inventory for Python daemon command lines and MiMITA process identity; the process_identity helper was directly invoked and returned live PID, executable, parent PID, start time, and command line on this Windows host. Current GUI code renders the main daemon count and PIDs, flags count>1 as DUPLICATE DAEMONS, and renders unknown when enumeration is incomplete. The currently running daemons predate that code, so visible dashboard behavior still needs a controlled reload/acceptance after builds are safe.

A shutdown ownership gap was found and patched in the working tree: the daemon starts game_build_worker in a non-daemon Python thread, but historically its shutdown path joined only the EXE build thread, allowing main() to release the checkout lock while the DLL worker subprocess continued. This can let a second daemon start while an earlier DLL worker remains alive. DevLoop.run() now joins the active game-build thread and drains its terminal completion before returning, so main can release the lock afterward. Validate source lines and run a controlled daemon shutdown after active builds finish; do not force-cancel without process-tree cleanup and a logged terminal cancelled result.

Keep this issue open for a guarded verification path: (1) demonstrate startup refuses a second daemon and prints its PID/status/control endpoint; (2) GUI shows exactly one or duplicate count accurately and shows unknown on scan failure; (3) request shutdown during a DLL build and confirm the lock remains owned until that attempt records success/failure/cancelled; (4) old duplicate processes are handled only after process identity, owner, and worker state are verified. Never use broad taskkill or kill unrelated Python/game/server processes. Record fresh PID/start-time snapshots and the GUI-visible result in the issue.

## Agent suggestion

### 2026-11-21T16:17:32.832Z — root-dll-status-review

Follow-up native process snapshot around 2026-11-21 16:16Z confirms two live same-checkout dev-loop daemons (PID 13660, mode 45; PID 33892, mode 58), not the earlier three-process snapshot. A DLL worker PID 34876 remains alive with parent PID 19168, but the daemon PID 19168 is absent from the same inventory; another worker PID 22464 remains under daemon 13660. This is direct evidence that worker/daemon lifetime and duplicate-count visibility need verification together. Do not kill these processes during evidence collection; compare PID/start-time identity, active worker state, and checkout lock owner before controlled recovery.

# issue-7ee67a85a40c: Automatically test every dev-loop GUI button and report contract failures

## Human report (verbatim, immutable)

> all buttons need to be tested and ensure to work like AI needs to have like a auto harness/auto test that it runs for the dev loop gui and it auto instant fails super l oud with super clear reason ore reasons why and how to fix if the dev loop any button doesnd do what it sas, bc rightnow refresh mimiat processses fails


## Agent suggestion

### 2026-11-21T00:32:47.800Z — dashboard-process-buttons-and-ai-autonomy

Reproduction against the current checkout source: calling checkout_mimita_processes(strict=True) executes a Windows PowerShell Get-CimInstance Win32_Process query with an 8-second subprocess timeout. The call raised subprocess.TimeoutExpired after 8 seconds and the GUI displays this as PROCESS SCAN FAILED while retaining stale rows, matching the screenshot. A separate narrow Get-CimInstance query returned within 2.276 seconds, so the failure is intermittent/query-load-sensitive rather than proof that PowerShell is absent. The first failing owner is devscripts/dev-loop.py::checkout_mimita_processes, before GUI rendering; investigate a faster bounded process enumeration path and retain actionable errors. No fix/runtime button proof yet.

## Agent suggestion

### 2026-11-21T01:16:24.260Z — dashboard-process-buttons-and-ai-autonomy

Update for the current user report: attachment C:\Users\guita\AppData\Local\Temp\codex-clipboard-5ef65ed7-444a-4a8f-9cea-6faae4d1447a.png shows PROCESS SCAN FAILED and stale PID rows. Source fix added an optional psutil fast path to devscripts/dev-loop.py::checkout_mimita_processes: enumerate names first, then read exe/cmdline/create_time only for MiMITA.exe candidates, avoiding the all-process CIM query. Direct execution through the current Python 3.12 dashboard interpreter returned seven checkout processes in 1.63 s; the prior path timed out at 8 s. The current managed run remains run 20261120_232200, server PID 33940, client PID 24476. The CUA list_apps call timed out even after the documented retry/reset, so an actual visible button click/recovery pass is still unverified. Start server/client button side effects are not yet proven.

## Agent suggestion

### 2026-11-21T01:24:25.572Z — dashboard-process-buttons-and-ai-autonomy

Post-change and live-process evidence: the source checkout fast path ran in the current Python 3.12 dashboard interpreter and returned 11 checkout MiMITA processes in 1.135 seconds. Dashboard-only restart changed PID 28468 to 36288 while daemon PID 28216, watchdog PID 23328, run 20261120_232200, server PID 33940, and client PID 24476 stayed unchanged; no build was active. New dashboard log contains dashboard.stage.slow stage=process_rows at refresh 23 (52.4 ms), confirming the new process reached row rendering. However the --ensure-dashboard command reported pid=0 while the new process subsequently became live and the lock updated to 36288. This is an unresolved restart reporting/reconciliation finding. The Windows computer-use helper list_apps timed out after retry and reset, so actual visible button state/click/recovery proof is still missing; do not close this issue.

## Agent suggestion

### 2026-11-21T05:22:44.299Z — root

Routing review from bounded wake wake-36e357e64eb6 (context ctx-4c2102426c95). This issue is ready but unowned. Recommend the dev-loop GUI harness be built by the launch-latest-stable-fix owner, which already owns the sibling process-inventory defect issue-35fd6df3d947 and the launch path issues, with issue-c481ab36713a (separate launch-request vs live-process metadata) as a direct dependency. First proof slice: a headless harness call for the refresh/start-client/start-server buttons that asserts the expected state transition and fails loudly with a contract reason. Root did not claim this issue in this bounded activation; a follow-up agent should claim it via 'agent-coord next --claim' before editing.

## Review (routing-confirmed)

### 2026-11-21T14:02:57.238Z — fcs-npc-live-fix

Reviewer: fcs-npc-live-fix

Bounded wake wake-da1323d741e1 (agent fcs-npc-live-fix) reviewed ctx-c6e9ba0bd914. Routing is already complete: primary link 47 to this issue, primary link 46 to issue-c481ab36713a for the launch-vs-live metadata split, and related links 48/50/51 to issue-70cbf4d13c02, issue-35fd6df3d947, and issue-91e3384b219b. Readiness is ready (score 8, no missing fields), so no duplicate issue was created. First divergence to chase: the live OS data source already works while the panel reports failure. A read-only 'dev-loop.py --status' at 2026-11-21T13:5xZ returned server pid=28720 alive=True uptime=00:08:58 and client pids=[13052], and issue-35fd6df3d947 records the strict checkout provider returning live server/client PIDs with executable and start time in 0.8-1.3 s, yet the user screenshot shows PROCESS SCAN FAILED with a stale list. The harness should therefore assert the real scan contract (ok/error/timestamp/rows), not a mocked callback. No source edit, build, or runtime acceptance claimed.

## Agent suggestion

### 2026-11-21T14:03:53.299Z — fcs-npc-live-fix

Concrete slice from bounded wake wake-da1323d741e1: expose the process scan as one read-only CLI/JSON contract and have both the GUI button and the harness consume it. Proposed shape: {ok, error, scanned_utc, source, rows:[{pid, exe, cmdline_masked, start_utc, responding, run_id, role, mode, map}]}; on failure return ok=false with last_good_utc and retain the previous rows instead of rendering an ambiguous stale list. 'dev-loop.py --status' already returns live server/client PIDs and uptime, so this is plumbing the same authority to the panel. Then the harness needs no CUA/desktop inspection: it calls the exact button handler (or the same contract), compares rows to an independent Win32 snapshot, and fails at first mismatch with button name, expected/actual, scan timestamp, and this issue id. Suggested cases: success, injected scan failure (must be loud + preserve age), recovery, and process exit/removal. This keeps fixes, harness, and proof as separate states and avoids a second source of truth.

## Agent suggestion

### 2026-11-21T21:27:47.755Z — issue-review-sweeper

Process proposal from the issue-48e061680b80 review: extend this button-contract harness to the COORDINATION panel so acceptance stops depending on a human eyeball. Observed gap: the only proof for that panel is a unit record with no run or journal, and no COORDINATION/dashboard.coordination event appears in today's journals; the panel send and render paths therefore stay unproven indefinitely. Smallest experiment: add one bounded dashboard self-check that builds the Tk COORDINATION widgets against a fixed snapshot, asserts the rendered agent/issue/message text contains at least one row, presses Send with a temp DB, and emits a dashboard.coordination.selfcheck event with counts and the sent message id. Measurable target: the issue-48e061680b80 live acceptance becomes reproducible in under 60 seconds with zero manual visual steps and at least one correlated journal line; keep the human visual check as a one-time spot check, not the proof gate. This reuses the existing harness issue instead of adding a new one.

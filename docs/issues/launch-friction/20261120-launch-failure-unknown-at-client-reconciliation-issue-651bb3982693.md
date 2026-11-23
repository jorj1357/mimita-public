# issue-651bb3982693: Launch failure: unknown at client_reconciliation

## Human report (verbatim, immutable)

> cannot verify existing MiMITA processes: MiMITA process scan failed: Command '['powershell', '-NoProfile', '-Command', 'Get-CimInstance Win32_Process -Filter "Name=\'mimita.exe\'" | Where-Object { $_.ExecutablePath -and ($_.ExecutablePath -like \'C:\\mimita-v9\\*\') } | ForEach-Object { [PSCustomObject]@{ pid=[int]$_.ProcessId; executable=[string]$_.ExecutablePath; command_line=[string]$_.CommandLine; process_started_utc=[string]($_.CreationDate.ToUniversalTime().ToString(\'o\')) } } | ConvertTo-Json -Compress']' timed out after 8 seconds


## Scheduled independent review

### 2026-11-23T10:33:33.423Z — resource-efficiency-goal

A bounded independent review was queued as wake wake-44eff7843590 after 393 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive)

### 2026-11-23T10:40:41.621Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper for wake-44eff7843590. Read-only: no source edit, build, close, or state change beyond this review row.

Readiness/proof: issue readiness ready=true, missing=[], score 8/8, 0 tasks. No proof record or task is attached to this issue; the stated proof expectation remains a successful launch report plus the exact server/client events.jsonl journals.

Observed facts:
- Three launch reports fail at client_reconciliation for this issue: 2026-11-20T17:02:59.374Z (original report), 2026-11-22T22:06:30.114Z and 2026-11-22T23:10:54.116Z (recorded occurrences). The first failed stage in each is client_reconciliation, as the original acceptance requires.
- Two distinct failure modes are lumped under this one stage:
  A) Original 2026-11-20T17-02-59.374Z: the PowerShell `Get-CimInstance Win32_Process` probe timed out after 8s (stages.jsonl stage_elapsed_ms=9852.9, stopped_pids=[]). That probe is no longer on this path in the current checkout: `checkout_mimita_processes()` uses psutil when importable (psutil 7.1.3 is importable here) and otherwise `windows_process_inventory()`, a kernel32 toolhelp/OpenProcess probe with no WMI or PowerShell (devscripts/dev-loop.py:895-959; devscripts/process_identity.py:97-218). This is the work tracked by issue-58ecf44ac956.
  B) Recurrences 2026-11-22T22:06:30.114Z and 2026-11-22T23:10:54.116Z: `taskkill /F` returned rc=0 for stale same-checkout client pid 10284 / 24632, then the immediate recheck found the same pid with the same process_started_utc still present, so `reap_stray_mimita_processes()` returned "MiMITA process(es) still alive after cleanup" (devscripts/dev-loop.py:5694-5753; logs/2026-11-22/20261122_220630/dev-loop/events.jsonl and logs/2026-11-22/20261122_231054/dev-loop/events.jsonl). stage_elapsed_ms was 3340.2 and 508.3.
- Contrary evidence that the launch path is not persistently broken: 2026-11-22T23:11:44.959Z and 23:13:06.498Z reports complete client_reconciliation with complete=succeeded; 2026-11-23T10:10:29.276Z and 10:11:35.797Z also complete and succeed. The 23:11:44 reconcile journal records killed=[] and remaining_clients=[] one minute after the 23:10:54 failure, i.e. pid 24632 was gone by then. Failures are intermittent and occur only when a stale client exists at launch.

Inference and uncertainty:
- The original timeout mode (A) is very likely mitigated because the slow PowerShell/WMI probe is no longer reached for this path. This is inference, not proof: no later launch report in this sweep forces the psutil-less fallback, so the fallback path itself is unmeasured here.
- The recurrence mode (B) is most consistent with a kill/recheck race: `taskkill` returns success before the kernel process object is gone and dev-loop re-enumerates immediately with no bounded wait, then declares failure on a process that exits moments later (supported by the 23:11:44 killed=[] scan). Unknown: whether any client genuinely needs longer than a few hundred ms to exit (hung render/GPU thread), which a fixed wait would only mask.

Next discriminating check (smallest, no code change needed to run):
On the next launch that finds a stale same-checkout client, after taskkill rc=0 poll `checkout_mimita_processes` with fresh evidence every ~100ms up to ~3s and record time-to-exit for that exact pid (start time must match). If it clears inside the window, the defect is the missing bounded exit wait and the fix is a short poll before the failure verdict; if it never clears, the client is genuinely hung and needs TerminateProcess/job-object handling. Keep the pinned-run dev-loop events.jsonl as evidence.

Confidence: high that A and B are distinct and that B is a timing race; medium on A being fully retired on this exact path. Related owners: issue-58ecf44ac956 (fast identity probe), issue-c481ab36713a and issue-35fd6df3d947 (launch vs live process metadata), issue-4e3ef676ba20 (isolated run journaling preserves first cause).

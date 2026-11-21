# Dev-loop process scan fallback timed out

Recorded: `2026-11-21T15:29:50.512Z`  
Status: **ATTEMPTED FIX — native fallback implemented; dashboard refresh proof pending**

## Expected behavior

The process panel and launch reconciliation must obtain current process identity
without depending on an optional Python package or a slow WMI provider. A failed
scan must remain visibly stale; the dev-loop must not mistake an unknown
process role for a client or server.

## Observed behavior

The user supplied a dashboard screenshot showing `PROCESS SCAN FAILED · showing
stale list` and stale server/client rows. Launch report
`.dev/launch-reports/2026-11-21T15-29-50.512Z/launch.json` records
`failure_stage=client_reconciliation`: the PowerShell
`Get-CimInstance Win32_Process` process scan timed out after 8 seconds, so the
client launch could not verify existing MiMITA processes.

The process-inventory fast path was previously added in
`20261121_012922-dashboard-process-refresh-fast-path.md`, but was conditional
on `psutil`. That changelog noted Python 3.14 lacked `psutil` and retained the
WMI fallback; the current launch report proves that the failing fallback was
still reachable. The exact dashboard interpreter was not observed, so it is
not established whether missing `psutil`, stale loaded source, or both selected
the branch.

The timeout occurred while whole-game DLL generation 423 had `jobs=12` and was
in `object_cache_scan`; its progress file showed a long-running scan. This is
consistent with resource contention, but does not prove parallel build work
caused the WMI timeout. High-load reliability is cross-linked to issue
`issue-2a5c5da53565`.

## Cause

`devscripts/dev-loop.py::checkout_mimita_processes` used a selective `psutil`
path only when that optional package imported successfully. Otherwise it ran a
PowerShell WMI query over `Win32_Process` with an 8-second deadline. Thus the
prior performance fix did not cover every dashboard interpreter and process
discovery still depended on a slow external provider.

## Fix attempted

`devscripts/process_identity.py::windows_process_inventory` now enumerates
process IDs with the Windows Toolhelp API, then reads executable path and
creation time only for `mimita.exe` candidates using kernel32. The no-`psutil`
branch of `checkout_mimita_processes` uses this native snapshot instead of
PowerShell/WMI. It leaves command line unknown; launch reconciliation recovers
managed process roles from PID, executable, and creation time in run manifests,
and keeps unmatched roles unknown so cleanup remains conservative.

## Verification

- The exact checkout process provider was invoked with `psutil` imports blocked
  to exercise the fallback. It returned PID `7556`, executable
  `C:\\mimita-v9\\.dev\\current\\mimita.exe`, and its process start time in
  `26.16ms`.
- A direct call to the native snapshot returned the same process identity in
  `23.89ms`.
- `python -m py_compile devscripts/process_identity.py devscripts/dev-loop.py`
  passed.
- The DLL build was not restarted or replaced. Build overlap is recorded as
  correlation only.
- The dashboard itself was not reloaded and its Refresh button/recovery state
  was not visually checked: the computer-use service timed out repeatedly.
  Cross-interpreter and visible dashboard acceptance remain open under
  `issue-35fd6df3d947`.

See [the investigation changelog](../../changelog/2026-11-21/20261121_155356-native-process-scan-fallback.md).

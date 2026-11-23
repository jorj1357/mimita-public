# Bounded Windows CPU stack capture

This tool captures a short, system-wide CPU sample trace so MiMITA and build
worker CPU can be compared by process, thread, and call stack. It is manual and
on demand; it does not leave WPR recording in the background.

## One-time setup

From the repository root, run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File devscripts\cpu-wpr-capture.ps1 -Mode Install
```

Windows asks for one UAC approval. The setup installs a fixed capture script
under `C:\Program Files\MiMITA Diagnostics` and registers the demand-only task
`MiMITA CPU Stack Capture`. The installed helper is writable only by
Administrators and SYSTEM. The task runs as SYSTEM with the highest run level;
its task ACL allows the installing user to query and run it, not edit it. The
fixed task accepts no arbitrary command or path arguments. This avoids a
filtered interactive administrator token that can occur when a task is marked
"highest" but launched under the user's UAC-filtered logon.

After changing the helper, update the protected copy with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File devscripts\cpu-wpr-capture.ps1 -Mode Update
```

This avoids giving the regular Codex process the broad `Profile system
performance` right. Microsoft recommends limiting that right to
Administrators: <https://learn.microsoft.com/en-us/previous-versions/windows/it-pro/windows-10/security/threat-protection/security-policy-settings/profile-system-performance>.

The protected command launcher writes task start/exit timestamps to
`.dev/perf-captures/capture-launch.log`; PowerShell output and errors go to
`.dev/perf-captures/capture-launch-output.log`, and structured phase records go
to `capture-task.jsonl`. The helper refuses capture below 2 GiB available RAM
or 8 GiB free on C:.

## Capture

Trigger one bounded 12-second verbose `CPU` trace in the isolated WPR instance
`MiMITA_CPU_Profile`:

```powershell
schtasks /run /tn "MiMITA CPU Stack Capture"
```

The helper refuses to start if C: has less than 8 GiB free, available physical
RAM is below 2 GiB, or WPR is already recording. It writes timestamped ETL
files and one JSONL status record under
`.dev/perf-captures/`. It never deletes old traces. If WPR cannot start or stop,
the record keeps the command output and failure state. A trace larger than 2
GiB is flagged for review before opening it.
Preflight refusals, task failures, preflight phase transitions, and each WPR
command's arguments, process ID, exit code or timeout are appended to the same
status log. Capture entry and elevated-token checks each emit a phase record
before proceeding, so a stalled scheduled task can be localized. Available
physical RAM is read directly with Windows `GlobalMemoryStatusEx`, and C: free
space through .NET `DriveInfo`; neither check uses Performance Counter or
PowerShell provider queries in the preflight path. Disk and memory checks also
log start/completion separately. The helper verifies WPR is not already
recording, then checks `xperf -loggers` and refuses if `NT Kernel Logger` is
already active or the logger state cannot be parsed. Other independent Windows
trace sessions do not block the capture. It captures
`PROC_THREAD+LOADER+PROFILE` with `Profile` stackwalks, 64 KiB
buffers (64 minimum, 128 maximum), a 512 MiB circular file limit, and a
12-second duration. It stops the kernel logger only after this helper
successfully started it. Status, start, stop, and cleanup commands have
30-second, 120-second, 120-second, and 30-second limits. The scheduled task
itself also has a 10-minute execution limit.

WPR status remains part of preflight, but the helper uses the xperf CPU sample
profile for capture because WPR start/stop has failed or timed out on this
machine. The resulting ETL opens in WPA and includes all processes present
during that 12-second interval. The explicit buffer and file limits bound
capture overhead and disk/RAM use.
Keep the game mode, map, actor count, client/server PIDs, and build state with
the trace. Capture a separate run for a no-build baseline and for a build-active
comparison; those answer different questions.

## Analyze and prove

Open the ETL in Windows Performance Analyzer and use **CPU Usage (Sampled)**.
Filter by MiMITA PID and group by process, thread, and stack. Load symbols for
the exact EXE/game-DLL build before reporting function names or source lines.
If the trace reports lost events, symbol resolution fails, or a stack only
shows a module offset, label that limitation and do not claim a source-level
hot function.

For each change, record matched before/after captures with the same machine,
scenario, active process count, build state, capture duration, and artifact
identity. Compare per-function CPU time and share, per-process CPU, total host
CPU, client frame-time percentiles, and server tick health. WPR proves sampled
CPU stacks; the pinned dev-loop journal proves which game build and scenario
were active. Neither one alone proves visible gameplay acceptance.

## Remove the trigger

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File devscripts\cpu-wpr-capture.ps1 -Mode Remove
```

Removal requires one UAC approval. It unregisters the task and preserves the
protected helper and every trace for explicit review or cleanup.

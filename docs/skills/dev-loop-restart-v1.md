# Dev-loop restart and self-recovery v1

## Purpose

Restart checkout-owned dev-loop, daemon, watchdog, game, test, and wake
processes without asking, then leave exactly one healthy loop running.

## Authority

The standing authority is in
`docs/specs/dev-loop/dev-loop.md#194a-ai-restart-authority-for-the-dev-loop-always-allowed`.
Agents may act immediately when:

- loaded Python code is stale after a source edit;
- `DAEMON_SOURCE_MISMATCH` is reported;
- the control socket is wedged;
- a launch, test run, or wake adapter is stuck on an owned process;
- a live scenario needs a clean owned process.

Do not restart unrelated processes, a healthy matching game pair that the task
does not require replacing, or another checkout.

## Procedure

1. Confirm checkout ownership and current state:

```powershell
python devscripts/dev-loop.py --status
$state = Get-Content .dev/state.json -Raw | ConvertFrom-Json
$state.daemon_pid
```

2. Preserve a healthy matching server/client pair unless the task requires a
   relaunch. Stop only owned clients when a clean launch is required.

3. Restart the daemon through the control path, then ensure it is running:

```powershell
$env:MIMITA_AGENT_ID = "<agent>"
python devscripts/dev-loop.py --request stop
python devscripts/dev-loop.py --ensure-running
python devscripts/dev-loop.py --status
```

4. Verify exactly one watchdog and one daemon exist, the daemon source hash
   matches the current script set, and the control endpoint answers:

```powershell
Get-CimInstance Win32_Process |
  Where-Object { $_.CommandLine -match 'dev-loop(-watch)?\.py' } |
  Select-Object ProcessId, Name, CommandLine
```

Use the same combined hash as `dev-loop-watch.py`: `dev-loop.py`,
`process_identity.py`, and `dev_loop/*.py`.

5. Record evidence in the coordination journal or issue:

- old and new daemon PID;
- old and new source hash;
- control endpoint/status result;
- whether the game pair was preserved or relaunched;
- the exact journal path.

6. If the restart fails, keep trying or report the failure loudly. Never finish
   with no working dev-loop.

## When the daemon is stale but the watcher is healthy

If the watcher owns replacement and no live run must be preserved, let the
watcher perform the controlled replacement:

```powershell
python devscripts/dev-loop-watch.py --once --force-source-change
```

If another watcher is already running, do not start a second watcher. Use the
stop/ensure path above or let the existing watcher act.

## Boundaries

- Restart only processes owned by this checkout.
- A daemon restart must not silently replace a healthy game pair.
- Do not kill a process merely because it is unfamiliar; inspect its command
  line and ownership first.
- Firewall, credentials, unrelated processes, and authority changes remain
  human decisions.

## Proof example

On 2026-11-20 a live daemon kept old launch-issue code after Python edits. The
agent stopped the daemon, ran `--ensure-running`, confirmed the new daemon,
recorded the combined source hash, and preserved the game server. The incident
is tracked by `issue-6fc8f853a674`.

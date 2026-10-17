# dev-loop helper PowerShell window flood

Time created: 2026-10-17T15:08:51Z
Time last updated: 2026-10-17T15:08:51Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/dev-loop/dev-loop.md` sections 5.1, 10.1, and 19.4.

Related changelog:
`docs/changelog/2026-10-17/20261017_003202-priority1-2-map-spawn-weapon-identity.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-10-17T01:00:00Z` (reported by the user during daemon start/restart)

### Expected Behavior

Starting or restarting the dev-loop daemon must not open visible console
windows. Helper processes the loop spawns must be hidden, and process
identity/liveness must not be re-queried per status render.

### Actual Behavior

Starting/restarting the daemon produced a rapid burst of visible Windows
PowerShell windows (described as "like 20 times in a row"), stealing focus.

### Why This Is Bad

Visible windows steal keyboard focus while the user is typing, and the burst is
CPU churn. It violates the dev-loop console-hygiene intent already applied to
the game server/client (`CREATE_NO_WINDOW`).

### Specification

`docs/specs/dev-loop/dev-loop.md` section 10 (headless/console) and 19.4
("visible daemon versus visible game"): child consoles are hidden and a
launch-owned console must not become foreground. The game server/client already
use `CREATE_NO_WINDOW`; the loop's own helpers were not covered.

### Wrong Code

File:

`devscripts/dev-loop.py`

```python
def windows_process_identity(pid: int) -> dict | None:
    ...
    result = subprocess.run(            # no creationflags -> console window
        ["powershell", "-NoProfile", "-Command",
         "$p = Get-CimInstance Win32_Process -Filter \"ProcessId=...\"..."],
        capture_output=True, text=True, timeout=3, check=False)
```

```python
def server_health(self) -> bool:
    if self.server_adopted and self.server_pid:
        identity = windows_process_identity(self.server_pid)   # spawns PowerShell
        ...
```

```python
def state(self, status: str = "idle") -> dict:
    ...
    if self.server_launch_ms and self.server_health() else None    # 1
    ...
    "server_alive": self.server_health(),                          # 2
    "run_alive": bool(self.server_health() or client_alive),       # 3
```

`print_status()` calls `state()` every main-loop iteration (`change_event.wait(0.20)`),
so while the server is adopted (`server_process is None`) the loop spawned
PowerShell up to ~15-30 times per second.

### Confirmed Cause

Process identity was obtained by spawning `powershell.exe` without a hidden
console, and `state()` re-queried `server_health()` up to three times per status
render. When the daemon is detached and the server is adopted (the state during
daemon restart with a persistent server), each query allocated a visible
console window. Evidence: `state()` calls `server_health()` at three sites;
`server_health()` calls `windows_process_identity()` when adopted; the main loop
renders status ~5x/s; `subprocess.run` had no `creationflags`.

### Attempted Fix 1

Time:
`2026-10-17T15:00:00Z`

Change:

- Added `run_hidden()` (and `_hidden_startupinfo()`) using
  `CREATE_NO_WINDOW` + `STARTF_USESHOWWINDOW`/`SW_HIDE` + `close_fds`.
- Routed `existing_dev_loop_pids`, `windows_process_identity`, the `taskkill`
  server-stop, the `--versioninfo` preflight, and the incremental build
  `Popen` through hidden creation.
- Cached `windows_process_identity()` with a short TTL
  (`_PROCESS_IDENTITY_TTL_SECONDS = 1.5`, positive and negative results).
- `state()` now queries `server_health()` once and reuses it for
  `server_alive` and `run_alive`.

Result:

Measured in the adopted-server state (daemon restarted, persistent server kept,
no client): a 6-second sampler counted **3 distinct `powershell.exe` PIDs**
(one of which was the measuring shell itself) versus the prior per-render flood.
The loop continued to adopt the server and report status normally.

### Corrected Code

File:

`devscripts/dev-loop.py`

```python
def run_hidden(args, **kwargs):
    kwargs.setdefault("creationflags", _CREATE_NO_WINDOW)
    if os.name == "nt":
        kwargs.setdefault("startupinfo", _hidden_startupinfo())
        kwargs.setdefault("close_fds", True)
    return subprocess.run(args, **kwargs)

def windows_process_identity(pid: int) -> dict | None:
    ...  # short-TTL cache; run_hidden(...)

def state(self, status: str = "idle") -> dict:
    server_alive = self.server_health()   # once, reused
```

### Fix

Helpers are hidden and process identity is cached, so status rendering no longer
floods the desktop with PowerShell windows. The documented steady-state target
(section 10.1) removes PowerShell entirely via Win32 `ctypes` APIs.

### Proof

Human review:

Not yet performed. A human should restart the daemon with a persistent server
and confirm no PowerShell windows appear.

Automated proof:

- 6-second ctypes/PowerShell sampler counted 3 distinct `powershell.exe` PIDs
  (including the sampler) in the adopted-server state.
- Daemon adopted server PID 29868 (`server_adopted=True`), `server_alive=True`,
  and continued to render status.

### Solution

`ATTEMPTED FIX (1)` — hidden helpers plus identity caching; awaiting human visual
confirmation to mark confirmed.

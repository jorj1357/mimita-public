# Dashboard maximized onto the primary monitor

- Recorded: 2026-11-20
- Status: FIXED — the live dashboard retained the selected secondary monitor through three close/reopen cycles and maximize/restore.
- Report: The user reported that moving the dashboard between desktop monitors could make it snap back to another screen.
- Root cause: `toggle_maximize()` queried `SPI_GETWORKAREA`, always returning the primary display's work area. Separately, negative Tk offsets were formatted as signed strings rejected by the saved-geometry regex, so moving left of the primary display never persisted. Startup fell back to primary defaults. The keepalive also searched for `--dashboard` only, while the running dashboard used `--dev-loop-role=dashboard`; its PowerShell query could also match itself and trigger redundant restart attempts.
- Change: Select the dashboard's current monitor with `MonitorFromWindow(..., MONITOR_DEFAULTTONEAREST)` and `GetMonitorInfoW`; maximize to that work area; store actual Win32 window bounds as signed numeric coordinates; restore from those bounds; and make keepalive discovery match both dashboard roles while excluding its own PowerShell process.
- Validation: After moving to DISPLAY5 (`x=-1920`), saved geometry was `880x760+-1920+25`. Maximize filled DISPLAY5 work area (`[-1920,0]`, `1920x1032`) and restore returned to the saved location. Three GUI close/reopen cycles created new window PIDs 27672, 14700, and 26504; keepalive `dashboard.healthy` events reported visible bounds `[-1920,25,-1040,785]` for all three, matching `.dev/dashboard-ui.json`. `proof task-verify` returned `proven` for `dashboard-cross-monitor-restart-loop`. Python syntax check and `git diff --check` passed.


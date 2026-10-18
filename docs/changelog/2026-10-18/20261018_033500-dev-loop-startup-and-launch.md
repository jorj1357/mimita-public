# Dev-loop delayed startup and unresponsive launch control

- UTC: `2026-10-18T03:35:00Z`
- Scope: explain and repair slow dashboard startup and the dashboard `1`
  control appearing to do nothing.

## Evidence and cause

- The visible dashboard is itself `dev-loop.py` running without `--daemon`.
  `existing_dev_loop_pids()` treated every Python process running that script
  as an existing daemon. The watchdog consequently tried to start a daemon,
  which exited with `another dev-loop is already running` and named the
  dashboard PID. The watchdog retried approximately every 30 seconds.
- `ensure_running()` waits up to 60 seconds for the watchdog-launched daemon,
  then may wait another 60 seconds for a direct daemon launch. This explains
  the long blank/startup delay when the false duplicate detection occurs.
- When no control endpoint is in the saved status, dashboard key 1's request
  fails. The dashboard then overwrote that failure with “build/launch
  requested”, hiding the reason.
- `wt.exe` is not available in this environment. The terminal helper therefore
  uses the existing Windows console fallback. Whether that console rendered
  rainbow colors was not verified visually in this run.

## Changes

- Daemon duplicate detection now counts only processes with a daemon role or
  `--daemon`, so the dashboard and one-shot CLI can coexist with the daemon.
- Dashboard control responses now retain and show accepted/rejected status
  instead of claiming a failed action was requested.
- The dev-loop specification now states both ownership and status-feedback
  requirements explicitly.

## Validation and runtime evidence

- `python -m py_compile devscripts/dev-loop.py` succeeded.
- The existing watchdog started daemon PID 26020 after the code change. The
  dev-loop published build 2518 and started a visible client PID 26608 plus
  dedicated server PID 4872. `--request launch` was accepted through the same
  authenticated daemon control socket and the live status reported matching
  build identity.
- Journal: `logs/2026-10-18/20261018_033317/events.jsonl`. It records the build
  2518 server and client launch arguments, room `E23VH6F`, mode `melee`, map
  `cramped`, and one startup NPC. The dashboard keyboard itself was not
  physically pressed during this validation; its `1` handler sends the same
  `build-or-launch` control action to the now-running daemon.
- The active client and dashboard were left running. Rainbow color rendering
  and human visual acceptance remain unverified.

# Visible dev-loop dashboard specification

- UTC: 2026-10-17T15:24:25Z
- Local time: 2026-10-17 11:24:25 EDT
- Branch: `20261011zt`
- HEAD: `7510918a164aafe4be1eb5d5a119a24f39f396b1`
- Scope: specification documentation only. No code, shortcut, process, build, or runtime was changed.
- Instructions followed: `AGENTS.md`, `docs/ROUTER.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/workflows/runtime-scenario-validation.md`, and `docs/architecture/time-and-formatting/time-and-formatting.md`.

## Changed file

### `docs/specs/dev-loop/dev-loop.md`

The file already had uncommitted changes before this edit, including sections on helper-console hiding, AI scenarios, daemon lifecycle, and the `dev-loop-watch.py` watchdog. Those existing edits were preserved. This session aligned the topology and expanded sections 19.1 and 19.4.

Before, section 19.4 began:

> `launch_visibility=visible` applies to the MiMITA client window. It does not promise a separate dev-loop console window.

After, section 19.4 begins:

> The intended desktop experience is: the user opens the existing shortcut to `devscripts/dev-loop.py`, sees a real terminal window with a live rainbow status display, and can leave that window open to watch builds and game runs.

Before, the daemon design said:

> The required design is one self-starting entry point, not a separate supervisor program.

After, the process design says:

> The required design is one self-starting user/AI entry point, not a second supervisor application that the user must manage. The existing `devscripts/dev-loop-watch.py` is the watchdog helper in this design.

New process-topology text distinguishes the visible dashboard, one-shot AI command entry point, watchdog, dev-loop daemon, game client, and dedicated server. It makes the desktop shortcut open/attach to one dashboard, leaves `dev-loop-watch.py` responsible for daemon recovery, and keeps the daemon as the sole build/game/control owner.

The new acceptance section requires an opaque, readable terminal (Windows Terminal with an opaque profile preferred, standard Windows console fallback), live rainbow status with a readable plain-text fallback, independent dashboard/game visibility, reconnect after dashboard close, singleton watchdog/daemon behavior, bounded recovery, and a live journal tied to exact executable and process identities.

The user clarified that the display must be copyable and that watchdog/daemon
identity should be obvious in Task Manager. Before this addition, the document
required a dashboard status but did not specify fixed rows, pause-for-selection,
or Task Manager role markers. The new exact requirement is:

> The status display is a fixed, non-scrolling dashboard. Keep each status field on a stable row and update that row in place; never append the full status block again just because uptime or another value changed.

And for process identity:

> Pass a stable role marker in each managed process command line (for example, `--dev-loop-role=watchdog`, `--dev-loop-role=daemon`, or `--dev-loop-role=dashboard`).

The spec also explains that Windows may still display the executable image name
as `python.exe`; the dashboard shows PID, interpreter, role, and script path,
and Task Manager's Command Line column must distinguish the processes. Python
remains the recommended lightweight, cross-platform implementation; a named
native wrapper is a separate tradeoff, not a requirement for this slice.

## Evidence checked

- Shortcut `C:\Users\guita\OneDrive\Desktop\dev-loop.py - Shortcut.lnk` targets `C:\mimita-v9\devscripts\dev-loop.py`, has no arguments, starts in `C:\mimita-v9\devscripts`, and uses normal window style.
- `.py` is associated with `py.exe "%L" %*` on this machine; the shortcut target is correct and does not call `pythonw.exe`.
- `devscripts/dev-loop.py::main()` runs `DevLoop.run()` in the invoking process for its normal startup path. `dev_print()` enables rainbow ANSI output only when `sys.stdout.isatty()` is true. The source has no explicit terminal-window launcher or separate dashboard attach mode.
- `launch_visibility` controls the MiMITA client window. The parser exposes `--headless` and `--visible` for that game-window policy.
- `devscripts/dev-loop-watch.py` exists and starts/restarts the detached dev-loop daemon, but source inspection did not establish that the desktop shortcut or every AI command starts the watchdog automatically.
- The user screenshot shows a transparent/blank-looking foreground window, but a static image does not identify which Windows console host owns it or why its interior is transparent. No live window/runtime inspection was performed.

## Validation and limits

- Reviewed the routed dev-loop spec and relevant implementation paths; ran `git diff --check` on the edited specification. Git reported only the existing LF/CRLF normalization warning.
- Documentation TODO scan found only instructional/history mentions of TODO wording in the router, checker, and old changelogs; no actionable TODO comment was identified.
- No build, test, runtime, terminal launch, or visual acceptance was performed. The spec change states the intended behavior; it does not implement it or prove the shortcut now opens a visible terminal.
- Router references `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md`; both were missing in this checkout.

## Implementation follow-up (same work stream)

Added the visible-dashboard and self-starting-entry-point implementation that
this specification describes.

### Code

- `devscripts/dev-loop.py`: added `--daemon`, `--dashboard`, `--cmd` alias,
  `--dev-loop-role`, `--ensure-running`, `--no-terminal-relaunch`,
  `--dashboard-frames`, and `--command-timeout`. Added `ensure_running`,
  `start_watchdog_detached`, `start_daemon_detached`, `wait_for_daemon_ready`,
  `send_control_request`, `wait_for_command_result`, `pid_alive`,
  `run_dashboard`, `_dashboard_lines`, and `_recent_activity`. `--status` stays
  read-only; the mutating entries ensure one watchdog/daemon and never open a
  dashboard.
- `devscripts/dev_loop_terminal.py` (new): owns virtual-terminal enabling,
  window-title setting, and the preferred-but-optional Windows Terminal
  relaunch with a standard-console fallback.
- `devscripts/dev-loop-watch.py`: starts the daemon as
  `--daemon --dev-loop-role=daemon` and accepts `--dev-loop-role=watchdog`.
- Fixed during this work: a status-query label leaking into the reported build
  phase; a stale process-identity cache after `taskkill` (added `fresh=`
  bypasses); and a `wait_for_command_result` race where the daemon's
  synchronous control reply could arrive after the client already executed the
  command, causing the CLI to read the journal from too late an offset. The CLI
  now captures the journal offset before sending the request.

### Runtime evidence

- Dashboard rendered all required rows (watchdog/daemon/dashboard role, PID,
  image `python.exe`, script path, heartbeat, build phase + running/latest +
  match, server/client alive + PID + uptime, run id, mode, visibility, map,
  last message, journal path, recent activity, controls).
- Role markers were present in the process command lines; the watchdog PID and
  daemon PID were unchanged across two `--ensure-running` calls (no duplicates).
- `--request launch` produced run `20261017_160802` (server 32744, client
  9600); `--command "agent.move 1 0 60"` and `--cmd "agent.snapshot dash_cmd"`
  both reported `delivered=True completed=True event=terminal.command.completed`.
- `--scenario-file devscripts/scenarios/command_proof_smoke_v1.json` passed
  against the live run.
- After `--request stop`, the daemon lock was empty; the next `--cmd` recovered
  one daemon via the watchdog and returned a reasoned result
  (`event=dev-loop.control reason="client is not alive and logger-ready"`)
  instead of a stale success.

### Limits

- Windows Terminal (`wt.exe`) was not installed on the validating machine, so
  the relaunch path fell back to the standard console; that specific path and
  the human visual reading of the live window remain unverified.
- The failed-recovery error branch was source-verified but not forcibly
  triggered at runtime to avoid disrupting the live loop.

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

## Dashboard controls, settings rows, and rainbow status (same work stream)

Added the dashboard key controls the user expects, made the status rainbow
colored with a plain fallback, and made an explicit dashboard `Q` shutdown
survive the watchdog. Followed `AGENTS.md`, `docs/ROUTER.md`,
`docs/specs/dev-loop/dev-loop.md`, `docs/workflows/runtime-scenario-validation.md`,
`docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`,
and `docs/skills/documentation-checker-v1.md`. Pre-existing working-tree changes
were preserved; only the dev-loop owner files and its spec were touched.

### Changed files and key edits

`devscripts/dev-loop.py`

- `_rainbow_enabled()` (line ~150): now honors `MIMITA_DASHBOARD_ANSI`
  (`1/true/yes` forces on, `0/false/no` forces off) so the rainbow and plain
  paths are both reachable when stdout is not a real terminal; otherwise the
  previous `sys.stdout.isatty()` check.
- Added `launch_mode_table()` (line ~596): returns `(id, name)` launch modes in
  display order for the picker, reusing the existing `dev-launch-modes.json`
  owner.
- Added daemon control actions in `process_control_requests` (line ~1412):
  `build-or-launch` (key 1; builds only when `latest_stale` or no build,
  otherwise `launch_latest()`), `set-launch-mode` (key 2; validates via
  `load_launch_mode`), `toggle-visibility` (key V), `toggle-auto-restart`
  (key A). Each records `dev-loop.control` and `save_state`.
- `control_loop` (line ~1363): `request.setdefault("requested_by", "ai")` so the
  dashboard can send `requested_by="dashboard"`.
- `stop` action (line ~1476): an `explicit_stop` request writes the stop-intent
  marker; the watchdog's own replacement stop does not.
- Deleted the daemon's `key_commands()` and `show_launch_modes()` and their
  `mode_picker_open`/`mode_picker_input` state, and removed the
  `self.key_commands()` call from `run()`: the detached daemon no longer reads
  the keyboard; the dashboard owns keys.
- Added `write_stop_intent` / `clear_stop_intent` / `read_stop_intent`
  (lines ~2359–2385). `ensure_running` calls `clear_stop_intent()` (line ~2946).
- `run_control_client` (line ~3409): `--request stop` sends
  `explicit_stop=True`, `requested_by="cli"`.
- `_dashboard_lines` + `DASHBOARD_CONTROLS_LINE` (line ~3063): added a
  `Settings` row (auto-restart, next-launch visibility, selected mode), the
  full controls row, and an optional launch-mode picker area.
- `_plain_signature` (line ~3116): signature that ignores heartbeat/uptime churn
  so the non-ANSI fallback reprints the complete block only when a meaningful
  field changes.
- `_dashboard_control` (line ~3131): sends one authenticated
  (`control_token`) control request with `requested_by="dashboard"`.
- `run_dashboard` (line ~3168): the render loop now renders rainbow
  (`_rainbowize`) in ANSI mode and a complete plain block otherwise; handles
  `1`, `2` (picker with Enter select / Esc cancel), `V`, `A`, `Q`
  (`stop` + `explicit_stop`), `X` (close view only), `P`, `R`. Added the
  `MIMITA_DASHBOARD_KEYS` scripted-key test hook that feeds keys through the
  same handler.

`devscripts/dev-loop-watch.py`

- Added `STOP_INTENT_PATH` (line 49) and `stop_intent()` (line 154). The watch
  loop (line 354) now, when the daemon is not alive and a stop-intent is
  present, records `dev-loop.watcher.stop-respected` and does not restart the
  daemon; a later explicit start clears it.

`devscripts/dev_loop_terminal.py`

- `supports_ansi()` (line ~63) honors `MIMITA_DASHBOARD_ANSI` for the same
  reason as `_rainbow_enabled()`.

`docs/specs/dev-loop/dev-loop.md`

- Section 19.4 now states the dashboard owns the keyboard, that `1/2/V/A` work
  whenever the dashboard is open regardless of any daemon console, and links to
  the canonical table (lines ~1336–1342). Implementation-status bullets updated
  (lines ~1363–1382).
- Section 22.2 replaced the two tables plus the conflicting
  "keys are only live when the daemon itself owns a console" paragraph with a
  single canonical "Dashboard controls" table, a `Settings`/controls note, the
  keypress-to-daemon path, and the view-close (`X`) versus daemon-shutdown (`Q`)
  distinction (lines ~1943–1986).

### Before / after quotes

Before (section 22.2):

> A detached daemon (started by the watchdog or the entry point) has no
> interactive console, so these build/launch keys are only live when the daemon
> itself owns a console.

After:

> Because the keys are delivered over the control socket, they work whenever the
> dashboard is open. They do not depend on the daemon owning an interactive
> console, and the daemon must not consume keyboard input.

Before (dashboard controls row):

> Controls : [P] pause/freeze  [R] resume/refresh  [Q] quit dashboard (watchdog/daemon keep running)

After:

> Controls : [1] build/launch  [2] launch mode  [V] client visible/headless  [A] auto-restart  [Q] stop daemon  [X] close view  [P] freeze  [R] resume

### Runtime / journal evidence

- Restarted the daemon onto the new code through the loop's own path. The
  pre-edit loop had died (stale watchdog/daemon locks, no live processes), so
  `python devscripts/dev-loop.py --ensure-running` started watchdog
  `pid=23708` and daemon `pid=9916`; the startup build published 2478, and
  `--request launch` produced run `20261017_171635` (server 17636).
- Dashboard keys exercised through `run_dashboard` with
  `MIMITA_DASHBOARD_KEYS="2,3,ENTER,v,a,x"`. Journal
  `logs/2026-10-17/20261017_171635/events.jsonl` recorded:
  - `dev-loop.control action=set-launch-mode status=accepted requested_by=dashboard msg="launch mode 3 selected: crate_testing_funworld3"`
  - `dev-loop.control action=toggle-visibility ... msg="launch visibility set to headless; applies to the next launch"`
  - `dev-loop.control action=toggle-auto-restart ... msg="auto-start enabled"`
  `--status --json` then reported `launch_mode=3`, `launch_visibility=headless`,
  `auto_restart=True`.
- Key `1` (`MIMITA_DASHBOARD_KEYS="1,x"`) produced
  `dev-loop.control action=build-or-launch status=completed msg="dashboard launched latest build"`
  (journal `logs/2026-10-17/20261017_171831/events.jsonl`) and launched run
  `20261017_171831` in mode 3, headless.
- Key `Q` (`MIMITA_DASHBOARD_KEYS="q"`) wrote
  `.dev/dev-loop-stop-intent` (`{"reason":"dashboard",...}`), recorded
  `dev-loop.control action=stop status=accepted`, the daemon lock emptied, and
  the watchdog recorded `dev-loop.watcher.stop-respected reason=explicit-shutdown`
  and left the daemon down (lock still empty after 13 s). A later
  `--ensure-running` cleared the intent and started watchdog `23708` + daemon
  `12644`; build 2480 succeeded.
- Rainbow rendering: with `MIMITA_DASHBOARD_ANSI=1`, a rendered frame contained
  per-character truecolor sequences (`\x1b[38;2;R;G;Bm`) and `\x1b[2K` clear
  sequences on every row. Terminal capability reported separately below.
- Plain fallback: with `MIMITA_DASHBOARD_ANSI=0` over three frames, the full
  status block printed once (no uptime churn) and included the controls and
  `Settings` rows.

### Terminal-color support and human acceptance (separate claims)

- Color support claim: the robot/CI shell is not a real TTY, so ANSI was
  exercised through the `MIMITA_DASHBOARD_ANSI` capability override; the escape
  sequences and plain fallback are both proven at the byte level.
- Human visual acceptance: NOT performed. Watching the rainbow status refresh in
  a real foreground terminal window is still required and is left to you.
  (`wt.exe` is not installed on this machine, so the dashboard uses the
  standard-console fallback.)

### Limits

- The keys were driven by the `MIMITA_DASHBOARD_KEYS` test hook that calls the
  same `next_key`/handler path as a physical keypress; a literal keyboard press
  in a live window was not observed by me.
- The launch-mode picker `Esc`-cancel path was exercised through the same
  handler but not visually confirmed.

### Documentation todo scan

The documentation-checker todo scan found only pre-existing markers; none were
introduced by this change. Active-spec markers noted per `docs/ROUTER.md:117`
(not edited):
- `docs/specs/gamemodes/counterstrike.md:705` — crouch binding/behavior TODO;
  suggest a concrete input-binding paragraph or an explicit "not scheduled".
- `docs/specs/gamemodes/juggernaut-duel.md:226` — consistent weapon-set for
  role-based score-limit modes; suggest one shared weapon-set table.
- `docs/specs/networking/networking.md:1088` — server-disagree visual effect;
  suggest moving to the HUD/GUI spec or marking it a proposal.
Archive/changelog/jorj matches were treated as historical and ignored.

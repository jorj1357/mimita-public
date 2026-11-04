Time created: 2026-11-03T21:26:00Z
Time last updated: 2026-11-03T21:26:00Z

Purpose:
Add a launch-mode picker and correlated launch timing to the dev-loop dashboard,
including a scrollable mode list, and document the behavior.

## Dashboard

- `devscripts/dev_loop/gui.py` loads configured launch modes from
  `devscripts/dev-launch-modes.json`. The More menu opens a native picker with a
  vertical scrollbar, marks the daemon-selected mode, and submits selection via
  `set-launch-mode`. Existing terminal selection remains available.
- Dashboard launch controls and key `1` use the existing `build-or-launch`
  daemon request. The status line correlates launch journal stages to the
  request and reports keypress-to-client-start and keypress-to-readiness times
  in seconds with two decimals. Pending progress, reused clients, and failures
  show available timing and failure details.
- `devscripts/dev-loop.py` passes the selected mode and request result/status
  fields needed by the dashboard through the existing control/status path.

## Specification

- Updated `docs/specs/dev-loop/dev-loop.md` with the scrollable picker, active
  mode feedback, key `1` launch path, timing milestones, pending/failure
  behavior, and the unchanged terminal picker.
- Recorded developer/stable executable separation as deferred design work; the
  existing latest-valid-published-executable launch path remains in force.

## Validation

- `python -m py_compile devscripts/dev_loop/gui.py devscripts/dev-loop.py` passed.
- `PYTHONPATH=devscripts python -m unittest dev_loop.test_artifacts`: 4 tests
  passed. The journal timing helper and configured-mode parsing smoke checks
  also passed earlier in this work.
- `git diff --check` passed for the three feature files (Git emitted only
  working-copy LF/CRLF notices).
- Opened the dashboard and More → Launch mode. Visually confirmed the list and
  scrollbar, the selected mode checkmark, and Cancel without changing daemon
  settings. Closed the temporary dashboard afterward.
- The required `python devscripts/quality/agent-gate.py` completed RED: build,
  executable selftests, lint baseline, and config synchronization passed; two
  runtime scenarios failed on command delivery timeouts (`grenade_tick_contract`
  and `npc_wall_containment`). No runtime launch timing acceptance was claimed.

## Existing documentation TODO

- `docs/specs/gui/guiv2.md` contains TODO notes at lines 23 and 25 about
  documenting the terminal/command-owned behavior model and keeping GUI actions
  as dispatchers. Suggested direction: add a section explaining that commands
  own behavior and GUI controls dispatch those commands. These unrelated TODOs
  were left unchanged.

## Scope preservation

The worktree had unrelated user changes before this task and additional changes
were present during implementation. They were preserved. Only the dev-loop
window, its dashboard status/control adapter, this specification, and this
changelog were changed for this feature.

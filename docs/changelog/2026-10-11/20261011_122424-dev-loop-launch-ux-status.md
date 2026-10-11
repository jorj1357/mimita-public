# Dev-loop launch UX and live status

Date: 2026-10-11

## Source/configuration

- Added bounded `events.jsonl` launch evidence for child arguments, console
  policy, owned windows, foreground window, cursor-under-window, visibility,
  and detected Windows Firewall consent dialogs.
- Changed dev-loop server/client subprocesses to `CREATE_NO_WINDOW`.
- Added visible/headless policy from the profile, `--visible`, `--headless`,
  and the interactive `[V]` next-launch toggle. Visible launches use a
  non-activating show operation and restore the prelaunch foreground window;
  headless launches hide the client window after logger readiness.
- Added a persistent status block showing mode, running/latest build, run
  state, uptime, PIDs, visibility, and a terminal hyperlink to the canonical
  `events.jsonl`.
- Changed the `npc-navigation` AI profile server bind to `127.0.0.1:1357` so
  local AI runs do not listen on public/private interfaces and trigger a new
  firewall consent prompt for each numbered executable.

## Validation

- `python -m py_compile devscripts/dev-loop.py` passed.
- `python devscripts/dev-loop.py --help` passed and shows `--headless` and
  `--visible`.
- Profile construction and server-argument inspection produced
  `127.0.0.1:1357`; headless policy construction produced `headless=True`.
- `git diff --check` passed for the changed launcher/profile files.
- Existing live journal `logs/2026-10-11/20261011_160047/events.jsonl` confirms
  the earlier `--unlock-cursor` runtime behavior: `cursor.launch_policy` had
  `unlock_cursor_on_launch=true` and `mouse_lock_locked=false`, followed by
  the existing L-toggle lock event.

## Runtime limitation

The currently running dev-loop daemon was started before these Python changes,
so it was not restarted or interrupted. A fresh dev-loop launch is still
required to produce and inspect the new `dev-loop.launch.*` window/focus/
firewall records and to confirm the no-console/headless behavior end-to-end.

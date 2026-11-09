# Coordinator dashboard status refresh fix

**Timestamp:** 2026-11-09 15:50:40 EST (2026-11-09T20:50:40Z)  
**Branch:** `checkpoint/pre-live-kernel-20261108`  
**HEAD:** `8dfb1a4c86a06de8905ddbb68f5bcb0ed8ea4b1e`

## Summary

Fixed the coordinator dashboard remaining on “Connecting to the build/runtime
service…” because lane collection raised `NameError: name 're' is not defined`.
`devscripts/dev-loop.py` now imports the standard-library `re` module used by
`collect_checkout_states` to validate lane IDs.

## Evidence

- Before the fix, the requested launcher opened the dashboard, but its status
  line displayed `Status refresh failed: name 're' is not defined`; no checkout,
  lane, or process status appeared.
- The root daemon was already alive (PID 34656), its control endpoint responded,
  and its stored build state was `published_current`. The issue was in GUI
  status collection, not daemon startup.
- After adding the import, reopened the dashboard using
  `devscripts/open-dev-loop-dashboard.py`. The visible window displayed the
  main checkout as `PUBLISHED_CURRENT`, seven registered checkout/lane rows,
  and a single `Updated 0s ago` refresh indicator. The process scan showed
  zero running MiMITA game processes.
- A second visual capture 2.5 seconds later showed the dashboard still
  populated and responsive.
- `python -m py_compile devscripts/dev-loop.py` passed.
- No EXE build was run, and no daemon or game process was restarted or started.
  The GUI refresh and visible dashboard are verified; multi-client/server
  behavior and lane controls were not exercised.

## Scope and attribution

Added `import re` in `devscripts/dev-loop.py` and recorded the reproduced issue
in [the dev-loop regression index](../../regressions/dev-loop/README.md) and
[its evidence record](../../regressions/dev-loop/dashboard-status-refresh-REG-2026-11-09.md).
The workspace already contained unrelated edits when this fix began; they were
preserved.

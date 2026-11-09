# Coordinator dashboard stayed on connecting status

Recorded: `2026-11-09T20:50:40Z` (2026-11-09 15:50:40 EST)  
Status: **FIXED — dashboard and lane status refreshed live after the missing import was added**

## Expected behavior

Opening `devscripts/open-dev-loop-dashboard.py` should attach a visible
dashboard to the existing dev-loop and replace the initial connecting label
with current build/runtime data and registered lane status.

## Observed behavior

The dashboard stayed on `Connecting to the build/runtime service…`. Its status
line showed `Status refresh failed: name 're' is not defined`. The dev-loop
daemon itself was alive, its authenticated control endpoint responded, and its
saved build state was `published_current`.

## Cause and correction

The coordinator lane collector in `devscripts/dev-loop.py`,
`collect_checkout_states`, validates each lane ID with `re.fullmatch`. The
module imported `random` and then `secrets` but omitted `import re`; the GUI
refresh caught the resulting `NameError` and left the initial summary visible.

Added `import re` beside the standard-library imports. No daemon or game
process was restarted for the correction; the visible dashboard was closed
and reopened from the requested launcher, reconnecting to the existing daemon.

## Verification

- Before the correction, the launched window displayed the exact `NameError`
  and did not populate build, lane, or process status.
- After the correction, the same launcher displayed the main checkout as
  `PUBLISHED_CURRENT`, seven registered checkout/lane rows, the single
  `Updated 0s ago` indicator, and a successful process scan reporting zero
  live MiMITA game processes.
- A second capture 2.5 seconds later showed the dashboard still refreshed and
  responsive. This verifies the dashboard status path and visible refresh; it
  does not verify live game/server behavior or the row controls.
- Source syntax validation passed. No EXE build was run; no game process was
  started.

See [the session changelog](../../changelog/2026-11-09/20261109_205040-dashboard-status-refresh.md)
and [the dashboard contract](../../specs/dev-loop/dashboard.md).

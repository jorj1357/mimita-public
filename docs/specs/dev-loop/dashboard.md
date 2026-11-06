# Dev-loop dashboard and process discovery

**Status:** Desired behavior contract.

## Purpose and audience

Define the Python developer dashboard's responsibilities, process visibility,
and controls. This specification is for MiMITA contributors and maintainers
working on the development loop.

This document does not define MiMITA's in-game GUI, gameplay, build mechanics,
or a second owner for dev-loop state. In-game presentation remains governed by
`docs/specs/gui/guiv2.md`; the dev-loop daemon and its lifetime remain governed
by `docs/specs/dev-loop/dev-loop.md`.

## Python dashboard boundary

- Python owns the developer dashboard presentation, process discovery view,
  and requests sent to the dev-loop daemon.
- The dashboard is a view and control client. The single dev-loop daemon remains
  the source of truth for builds, active runs, instance manifests, and actions.
- Python is suitable here because dashboard layout, status formatting, and
  process orchestration do not run in MiMITA's fixed-tick gameplay path.
- Dashboard animation and refresh work must remain bounded and must not delay
  daemon controls or build activity.
- Saving a dashboard Python file does not update code already loaded in a
  running dashboard process. Applying a dashboard edit may restart the
  dashboard view; it must reconnect to the existing daemon and preserve the
  game/server processes.
- Saving daemon code also does not hot-reload the running interpreter. Daemon
  source changes use the controlled restart and source-identity contract in
  `docs/specs/dev-loop/dev-loop.md` sections 19.2 and 19.3. Arbitrary in-place
  module reload is not required.
- JSON remains appropriate for configuration and data. Python owns dashboard
  behavior and layout; this does not make Python a replacement for the game's
  JSON-defined GUI presentation or C++ gameplay owners.

## Process visibility

The dashboard presents two distinct groups:

1. **Active-run instances:** processes listed in the active run manifest. They
   use stable run-local instance IDs and the lifecycle/status rules in
   `multi-instance.md`.
2. **External MiMITA processes:** matching processes not listed in the active
   run manifest. Show these separately; do not assign them a run ID, instance
   ID, run journal, or active-run count.

For each process row, show the best available role, PID, process start time,
state, executable path, and identity evidence. Show build identity, command
line role evidence, journal, console/window status, and connection target only
when that information is actually available. Unknown values remain unknown;
the dashboard must not infer a healthy client from process liveness.

Classify a process as MiMITA only when its executable identity matches a known
MiMITA executable. Classify its role only when role evidence is available, such
as an explicit server command-line marker or matching run/journal identity. A
candidate without enough evidence is labeled unverified and must not be
presented as a confirmed client or server. Unrelated processes must not be
shown as MiMITA based only on a window title or a similar process name.

External rows never become active-run instances through discovery. A later
explicit launch creates a new managed instance through the normal dev-loop
launch path; discovery itself does not adopt or relabel an external process.

### Refresh and running-process list

- The process panel has a visible **Refresh** button. Pressing it starts a new
  process scan immediately, without restarting the dashboard or dev-loop
  daemon.
- The panel also refreshes automatically. Every completed scan replaces the
  existing count and timestamp in place; it never appends another status line.
- Show the total unique live MiMITA process count and the scan completion time
  in UTC ISO 8601 form, for example `Found 2 running MiMITA processes · Last
  refreshed 2026-11-05T19:33:24.123Z`.
- The process list contains only processes confirmed alive by the current
  successful scan. Once a process exits, remove its row. Do not show an exited
  server as `stopped` in this live process list; keep final lifecycle history
  in the run manifest/journal and runtime summary instead.
- Count unique PIDs, so a run-owned instance and its matching OS process appear
  once. If enumeration fails, show the scan error and last successful scan
  time; do not report zero or replace the last known list with an empty result.
- Each live process row has its own checkbox. Keep checked state attached to
  process identity (PID plus process start time) across refreshes. Remove the
  selection when that process disappears or its identity changes.

## Process controls and feedback

- Active-run instances use the start, stop, and restart controls defined by
  `multi-instance.md`.
- A verified external MiMITA process may expose **Stop**. Recheck its PID,
  process start time, executable identity, and role evidence immediately before
  acting so PID reuse or a changed process cannot redirect the action.
- **Stop selected** applies to every checked, currently running, verified
  process. Report a result per target; a failed stop of one process must not
  prevent attempts on the remaining checked targets.
- Keep **Start server** and **Start client** as type-specific launch actions.
  The running-process checkboxes do not start already-running processes.
- Stopping an external process targets that MiMITA process only. It does not
  close or terminate the terminal, shell, or unrelated parent application that
  launched it. External processes are not restartable through the active run
  unless a future specification defines a safe way to reconstruct their
  launch configuration.
- An unverified candidate has no Stop or Restart action. Show which identity
  evidence is missing.
- Preserve row selection by stable process identity across dashboard refreshes;
  do not silently retarget an action when rows move or disappear.
- Show each control request as sending, accepted/queued, completed, rejected,
  or failed. Keep the request ID and reason visible until the result is clear.
  Acceptance means the daemon queued the request; completion requires an
  observed result.
- For Stop, report success only after confirming that the selected MiMITA PID
  exited. If it remains alive, show the failure reason and current identity.
- Record request ID, target identity, action, result, and reason in the
  dev-loop journal using categorized structured events. Never claim that an
  external process has a per-run journal when it does not.

## Acceptance

- The Refresh button starts a scan immediately. Automatic scans also update the
  same count/timestamp display in place using UTC ISO 8601 timestamps.
- With no running MiMITA processes, the panel says `Found 0 running MiMITA
  processes` and shows the time of the latest successful scan. A stopped server
  row disappears from the live list.
- Checked rows can be stopped together. Each target has a visible result, and
  the existing Start server and Start client controls remain available.
- A separately launched MiMITA client with verifiable executable and role
  identity appears as external with its PID; an unrelated application does not
  appear as MiMITA.
- A verified external process can be stopped without closing its terminal or
  affecting unrelated MiMITA processes. The dashboard confirms the selected
  PID exited or gives an actionable failure.
- An unverified candidate is clearly labeled and cannot be stopped from the
  dashboard.
- External processes never enter the active run manifest or its server/client
  counts.
- Dashboard restart reconnects to the same daemon and refreshes current status
  without restarting healthy game processes.
- Daemon source changes are distinguishable by source identity after its
  controlled restart; no claim of in-place Python hot reload is made.

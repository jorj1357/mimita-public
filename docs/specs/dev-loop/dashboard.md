# Dev-loop dashboard and process discovery

**Status:** Desired behavior contract.

## Purpose and audience

Define the standalone Python coordinator dashboard's responsibilities, process
visibility, and controls. This specification is for MiMITA contributors and
maintainers working on the development loop and its agent lanes.

This document does not define MiMITA's in-game GUI, gameplay, build mechanics,
or a second owner for dev-loop state. In-game presentation remains governed by
`docs/specs/gui/guiv2.md`; the dev-loop daemon and its lifetime remain governed
by `docs/specs/dev-loop/dev-loop.md`.

## Python dashboard boundary

- Python owns the standalone coordinator dashboard presentation, process
  discovery view, and requests sent to dev-loop daemons.
- The dashboard is a view and control client. The single dev-loop daemon remains
  the source of truth for its checkout's builds, active runs, instance
  manifests, and actions. The coordinator dashboard reads the main checkout
  and every valid lane listed in `.dev/agent-lanes/`; it does not merge their
  run manifests or build state.
- The main checkout owns one visible coordinator dashboard process. Starting,
  ensuring, or running an agent lane never opens another dashboard. The shared
  view reconnects to each checkout's existing daemon and can restart without
  stopping a daemon or game process.
- The dashboard is separate from the build/run daemon's lifetime. Closing the
  window closes only the view.
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

## Checkout and process visibility

Show the main checkout and each registered lane in a compact overview. Each
row identifies its checkout/lane and shows build state, active build phase and
elapsed time, plus server and client counts. A registered lane whose daemon is
absent is `not started` or `unavailable`; a daemon whose saved status timestamp
is older than six seconds is `stale`. Keep other checkout rows visible when a
lane cannot be read.

Build, Launch, Server, and Client actions target the checkout/lane row's own
authenticated daemon. Never route a lane action to the main checkout by
default. Per-instance actions remain scoped to their owning run.

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
- The panel also refreshes automatically. Keep one shared `Updated N seconds
  ago` freshness indicator for the dashboard; do not show separate last-updated
  timestamps for each checkout or process scan.
- Show the total unique live MiMITA process count beside the shared indicator.
  If a process scan fails, show the error and keep the last successful list
  marked stale until a scan succeeds.
- The process list contains only processes confirmed alive by the current
  successful scan. Once a process exits, remove its row. Do not show an exited
  server as `stopped` in this live process list; keep final lifecycle history
  in the run manifest/journal and runtime summary instead.
- Count unique PIDs, so a run-owned instance and its matching OS process appear
  once. If enumeration fails, show the scan error; do not report zero or
  replace the last known list with an empty result.
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

- The Refresh button starts a scan immediately. Automatic scans update the
  same count and shared freshness display in place.
- The main checkout and every registered lane remain visible with a clear
  live, stale, unavailable, or not-started state. Build phase and elapsed time
  update while a build runs.
- With no running MiMITA processes, the panel says `0 running MiMITA
  processes`. A stopped server row disappears from the live list.
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
- Ensuring or starting multiple agent lanes leaves exactly one dashboard
  window. Each lane can be built, launched, or used to start a server/client
  through its own daemon.
- Process rows identify their lane, role, PID, state, and connection target
  when known; a process without role evidence remains unverified.
- Daemon source changes are distinguishable by source identity after its
  controlled restart; no claim of in-place Python hot reload is made.

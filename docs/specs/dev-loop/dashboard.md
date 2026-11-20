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

## Collapsible sections

Every major dashboard section is collapsible: build cards, WHOLE-GAME DLL BUILD
QUEUE, CHECKOUTS AND AGENT LANES, MIMITA PROCESSES, RUNTIMES, and LIVE BUILD AND
LOOP OUTPUT. Each header is a toggle with a visible `[-]`/`[+]` marker and stays
visible while its body is hidden. Collapsed sections keep their last header
summary but skip body rendering. Open/closed state persists in
`.dev/dashboard-ui.json` under `collapsed`, keyed by a stable section id. Toggling
a section must not stop the daemon or any game process.

The RUNTIMES section lists each independent runtime's id, launch mode, map,
server/client liveness, port, pinned generation, and journal, with Stop and
Reload actions and a Start runtime control.

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
- The panel also refreshes automatically. Keep one shared freshness indicator
  for the dashboard with a UTC ISO 8601 last-read timestamp and a monotonically
  increasing GUI refresh number. Increment the number after each successful
  status-provider read so a user can see the view updating while idle. Do not
  show separate last-updated timestamps for each checkout or process scan.
- Label the build data source as the main dev-loop status response and identify
  the run `events.jsonl` path used for loop activity. Keep build status data
  distinct from journal events and do not imply that the refresh number counts
  build lines.
- Show the total unique live MiMITA process count beside the shared indicator.
  If a process scan fails, show the error and keep the last successful list
  marked stale until a scan succeeds.
- Show root dev-loop whole-game DLL progress separately from the launcher/EXE
  build in a large queue panel. Show the current generation, source generation,
  attempt ID, revision, attempt start time, current phase, and an ISO 8601 `as of`
  timestamp. The root worker builds one game DLL at a time; edits during an
  active build coalesce into one next generation. Do not count coalesced
  request IDs as separate DLLs.
- During dependency scanning and compilation, show resolved source units over
  the worker's source total, with a percentage derived only from that
  denominator, elapsed time, and currently compiling source paths. Show link,
  validation, publication, and runtime activation as named phases without
  inventing percentages for them.
- Explain `[CXX]` as compiled and `[SKIP]` as an existing dependency-clean
  object reused. Do not describe an object skip as a `ccache` hit. Show
  the source revision, changed paths when the matching change report exists,
  and published generation plus DLL SHA-256 after publication. Do not imply
  the dashboard knows an agent's natural-language change description unless
  it was explicitly written with `devscripts/build-note.py`. Show agent notes,
  changed paths, latest per-file results, dependency-check time, compiler queue
  wait, progress-write time, output-handler time, and the phase breakdown when
  the corresponding data is available. The dashboard reads the bounded live
  progress file; it does not parse the full per-attempt timing stream during
  refresh. Runtime activation is shown only when confirmed by the instance
  journal.
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
- **Launch latest stable MiMITA.exe** is the quick-entry action: close existing
  same-checkout clients, then immediately open exactly one fresh client from
  the latest valid published build, even if newer source edits are waiting to
  build. Do not queue or wait for a build from this button; use **Build** for
  that. Retain a healthy matching server; replace it only when normal
  build/configuration checks require it. Use **Start client** to add another
  client without replacing the current clients.
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
  same process count and shared freshness display in place; while the dashboard
  is open, the UTC timestamp and refresh number advance even when build state
  is idle.
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


## Single dashboard and explicit terminal relaunch (2026-11-15)

- A normal or scripted invocation must not create a Windows Terminal window.
- The dashboard is opened in place, or an existing dashboard is reused.
- Only an explicit `--relaunch-terminal` (or explicit environment opt-in) may
  start Windows Terminal.
- Before relaunching, the entry point checks for an existing dashboard PID and
  attaches/reuses it.
- AI command/scenario calls must create no dashboard and no Python console
  window.
- Acceptance: repeated `python devscripts/dev-loop.py --command ...` calls leave
  the number of Python/Terminal windows unchanged.

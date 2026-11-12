# Windows process snapshot helper

- Time: 2026-11-11 17:53:44 EST (2026-11-11T22:53:44Z)
- Branch: `checkpoint/one-checkout-condensation-20261111`
- HEAD before and after: `e6d58baa`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Starting state

`git status --short --ignore-submodules=all` was empty before this task. The
existing build lock metadata identifies PID 12188 as `build.py`, and the build
progress file still points to `src/npc/npc-combat.cpp`. The current dev-loop
status reports the retry failed before compilation because it could not read
the locked byte in `.dev/build-serialization.lock`.

## Changes

Added `devscripts/process-snapshot.py` (lines 1-380). It gathers a bounded,
read-only snapshot of MiMITA-related Python and executable processes, parent
processes, PID and parent PID, command line with credential-like values
redacted, start time, memory, handles, and threads. It also records the
checkout build-lock owner, EXE scheduler-slot owner, dev-loop PID, selected
dev-loop status fields, and build progress. It writes a UTC timestamped JSON
report under `.dev/process-snapshots/`, after checking that the destination
stays inside the checkout. It does not stop processes, run builds, or collect
environment variables or dev-loop control tokens.

Added `devscripts/capture-windows-processes.bat` (lines 1-10) as a
double-clickable launcher for the Python script.

The source files did not previously exist. No existing source or lock file was
modified. The latest report produced during this session is ignored runtime
data at `.dev/process-snapshots/process_snapshot_20261111_225343.json`.

## Documents and review

Read `AGENTS.md`, `docs/ROUTER.md`,
`docs/specs/dev-loop/agent-lanes-build-authority.md`,
`docs/specs/dev-loop/dev-loop.md`, `docs/specs/dev-loop/dashboard.md`,
`docs/specs/dev-loop/multi-instance.md`,
`docs/architecture/live-development/self-contained-workspace.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/architecture/terminal-commands/terminal-commands.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/skills/terminal-command-checker-v1.md`, and
`docs/regressions/README.md`.

The terminal-command review is not applicable: this adds a maintenance
launcher, not a game terminal command. Dashboard process controls remain owned
by the dev-loop; this helper is read-only diagnostics and does not add a
process stop path.

## Validation and remaining review

Ran `py -3 devscripts/process-snapshot.py` twice as requested. Both runs wrote
reports; the latest captured both lock metadata records and the stale build
progress. PowerShell CIM process enumeration was denied in this agent's
isolated environment, so the run used the Toolhelp fallback, which only sees
process names and PIDs available to that environment. The report clearly marks
that limitation and did not claim PID 12188 was absent on the user's Windows
session. Python syntax and report generation were exercised by running the
script; no separate test suite or build was run.

Human step: double-click `devscripts/capture-windows-processes.bat` in the
user's Windows session. After it finishes, the full report will appear in
`.dev/process-snapshots/` for the next inspection. Host-session process
visibility and the active lock owner still need that run's report.

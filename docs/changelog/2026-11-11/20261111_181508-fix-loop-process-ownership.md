# Fix dev-loop process ownership

- Time: 2026-11-11 18:15 EST
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Result: `IMPLEMENTED_NEEDS_WINDOWS_RUN`

## Evidence and cause

Reviewed `.dev/process-snapshots/process_snapshot_20261111_225640.json` and
the watchdog log. The report showed 25 root-checkout `pythonw.exe` daemon
instances and a watchdog restart count of 5,654. The log recorded existing
daemon PIDs as dead even though the process snapshot showed at least one of
those same PIDs still running. The process checks treated an inaccessible PID
as dead, and root daemon discovery only queried `python.exe`, missing the
actual `pythonw.exe` processes.

The report also showed a root build worker and a worker in the pre-existing
NPC-hooks lane. This change does not stop build workers or alter lane data.

## Changes

- Added `devscripts/process_identity.py`. On Windows it checks process exit
  status and falls back to a Toolhelp process snapshot; inability to inspect a
  PID conservatively counts as possibly alive.
- Updated the root dev-loop, watchdog, and build scheduler to use that shared
  liveness check. The dev-loop daemon scan now includes both `python.exe` and
  `pythonw.exe`; the helper is included in dev-loop source hashing.
- Added `devscripts/repair-dev-loop-ownership.bat` and its Python helper. When
  run on the user's Windows session, it verifies process command lines and
  creation times, stops only watchdog/daemon processes belonging to this exact
  checkout and the exact root `build.py build-only` worker, then starts one
  root loop through `--ensure-running`.
- The cleanup intentionally leaves game clients/servers, lane build workers,
  and `.dev/agent-lanes` data untouched.

## Validation and remaining evidence

- `git diff --check` passed for the edited files.
- No build or test suite was run. The agent environment cannot inspect the
  user's Windows process table, so it could not execute or confirm the repair
  against those live processes.
- Runtime evidence is pending: run the new `.bat` on the Windows desktop and
  inspect its status output plus a fresh process snapshot. This proves the
  process-ownership repair only; it does not yet prove any hot-reload DLL
  migration or runtime behavior.

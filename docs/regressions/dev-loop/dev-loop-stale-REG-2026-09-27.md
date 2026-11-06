# Dev loop relaunched a stale published build after compilation failed

Time created: 2026-09-27T19:54:00Z
Time last updated: 2026-09-27T19:54:00Z

Status: FIXED (source-level; live dev-loop restart still required)

Related file:
`devscripts/dev-loop.py`

## Observed

After a source build failed, the loop continued to report and launch the
previous numbered artifact. Pressing `[1]` and automatic process recovery both
used the old executable, even though the current source generation was newer.

## Expected behavior

The loop must never launch an artifact known to be stale. `[1]` must queue a
build/retry when the latest artifact is stale or absent. A new dev-loop process
must restore the newest valid published artifact from `.dev/builds` and validate
the current source before launching it.

A failed build must leave the old process running when possible and print
compiler/build diagnostics in the dev-loop output.

## Confirmed cause

- `DevLoop.__init__` discarded all numbered artifacts and started with
  `latest_build = None`.
- `run_build()` retained the old `latest_build` after failure.
- `key_commands()` mapped `[1]` directly to `launch_latest()`.
- `maintain_process()` relaunched `latest_build` after process exit without
  checking `latest_stale`.

## Corrected code

`devscripts/dev-loop.py` now:

- restores the newest numbered directory containing a non-empty `mimita.exe`;
- marks restored artifacts stale until the initial current-source build passes;
- marks the retained artifact stale after every failed build;
- refuses stale launches in `launch_latest()`;
- prevents automatic crash recovery from relaunching stale artifacts;
- makes `[1]` queue a build/retry when stale or missing, otherwise switch to the
  current published build;
- prints filtered compiler/linker diagnostics after the complete child output
  has streamed.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- Non-launching state check restored build `0058` from disk and reported
  `stale=True`, `pending=True`.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Human validation still required after restarting the existing dev-loop
  process: fail a build, confirm diagnostics and stale waiting status, press
  `[1]`, then confirm a successful build publishes and launches a new number.

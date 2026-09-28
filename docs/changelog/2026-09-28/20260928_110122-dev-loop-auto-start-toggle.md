# Restore the dev-loop auto-start toggle

Date: 2026-09-28

## Change

`devscripts/dev-loop.py` now separates continuous incremental building from
automatic executable launching.

- The loop continues watching files and building whenever source changes.
- `[A]` toggles `AUTO-START: ON` or `AUTO-START: OFF`.
- With auto-start OFF, successful background builds are published but do not
  launch a new server/client pair, and an exited child is not unexpectedly
  relaunched.
- `[1]` still explicitly queues a stale/missing build or launches the newest
  valid build. That manual request overrides auto-start OFF for that launch.
- With auto-start ON, successful builds and valid child recovery retain the
  automatic launch behavior.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Live terminal acceptance remains for the user: start the loop, press `[A]`,
  edit a watched source file, and confirm OFF publishes without launching;
  then press `[1]` and confirm the build launches normally.

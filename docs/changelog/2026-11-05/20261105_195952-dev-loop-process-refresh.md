# Dashboard Process Refresh and Stop Selection

- Timestamp: `2026-11-05T19:59:52Z` (2026-11-05 14:59:52 EST, America/New_York)
- Branch: `20261011zt`
- Result: `IMPLEMENTED_SOURCE_CHECKED`; dashboard runtime and human visual review remain open

## Scope and changes

Implemented the requested process refresh and selection controls in the Python dev-loop dashboard:

- `devscripts/dev_loop/gui.py`: added the Refresh control, background process scans, and an in-place running MiMITA process count with the latest successful UTC ISO 8601 scan time. Rows are keyed by PID and process start time, checked rows retain identity across scans, and exited rows disappear. Scan errors preserve the previous list and timestamp while displaying the failure. Added per-row checkboxes and Stop selected while keeping Start server and Start client. Restart selected remains limited to a focused managed run instance.
- `devscripts/dev-loop.py`: added checkout-scoped process enumeration and the stop-selected request handler. Managed targets are checked against their run instance and PID; external processes are revalidated by PID, start time, executable, and role before stopping only the MiMITA process. Per-target outcomes are returned to the dashboard and recorded. Client stop confirms process exit.
- `docs/specs/dev-loop/dashboard.md` and `docs/specs/dev-loop/multi-instance.md`: specified refresh/count/timestamp, live-row removal, scan failure behavior, checkbox selection, stop results, and external process safety. `docs/ROUTER.md` routes dashboard requirements to the dev-loop specs and in-game GUI requirements to existing GUI specs.

The implementation follows the user's choice to retain the type-specific start buttons; process checkboxes drive Stop selected.

## Review and validation

- `python -m py_compile devscripts/dev-loop.py devscripts/dev_loop/gui.py`: PASS.
- Focused `git diff --check` on the changed implementation/spec/router files: PASS; Git emitted only line-ending conversion notices.
- Focused source review confirmed the stop-selected handler, external process identity recheck, result reporting, and no remaining `process_list` widget reference.
- No dashboard or game processes were launched or stopped for validation. Runtime process discovery, real stop behavior, and visual/human acceptance remain unverified.

## Concurrent workspace changes

The shared worktree contains unrelated pre-existing and concurrent edits, including source, assets, and documentation changes. This change left them untouched.

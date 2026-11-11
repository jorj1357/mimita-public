# Task

- Task ID: Use one shared checkout for agent development
- Summary: Retire mode 2 lanes as the normal agent workflow and make the existing checkout, branch, dev-loop, and EXE the shared default.
- Status: Documentation updated; live build remains blocked by an unresolved Windows build lock.
- Date, time, timezone: 2026-11-11; time recorded in filename; local timezone unavailable in this session.
- Branch: `checkpoint/pre-live-kernel-20261108` (reported by prior repository inspection)
- Final commit: No commit created.

# Requested behavior

The user asked to stop using mode 2 agent lanes and have agents edit the same files in the same folder, build the same EXE, and use incremental builds.

# Changes

- `AGENTS.md`: Changed the mandatory workflow to the current shared checkout and root dev-loop. Prohibited new lanes, worktrees, branches, and duplicate checkouts for ordinary agent work. Preserved the rule against agents invoking builds directly.
- `docs/specs/dev-loop/agent-lanes-build-authority.md`: Added the user's shared-checkout policy as current; clarified existing lane contents remain untouched and lane instructions below are historical.
- `docs/architecture/live-development/self-contained-workspace.md`: Added the shared-checkout rule and marked lane-specific procedures as superseded for normal work.
- `docs/specs/dev-loop/dev-loop.md`: Marked Mode 1 shared checkout as current and Mode 2 lane procedures as historical.

# Build investigation evidence

- `build/progress.json` reported 582 EXE translation units, 304 skipped, 0 compiled at its last update, and current file `src\\map\\map-loader-mesh.cpp`. This is evidence the incremental scan was skipping unchanged work; it was not compiling all 582 files at that snapshot.
- The last successful entry in `build/build-timing.json` reported 0 compiled, 624 skipped, and 18.129 seconds total (1.815 seconds compile phase, 13.823 seconds link phase) at `2026-11-11T18:31:59Z`. A separate cold build at `15:30:15Z` compiled 152 and skipped 472, taking 304.825 seconds. This shows both incremental and broader rebuilds occur; it does not establish which source/header edits caused the broader rebuild.
- The root dev-loop state reported `build_failed` with `PermissionError` in `build_lock.py` while acquiring/reading `.dev/build-serialization.lock`.
- `.dev/build-scheduler/exe-0.lock` contained PID 12188, owner `build.py:12188`, started at `2026-11-11T21:35:35Z`. `.dev/dev-loop.lock` contained PID 21908. The lock files could not be reliably reconciled to a live owner in this session. No locks were deleted and no processes were stopped.
- The last successful EXE artifact reported by the root dev-loop was from `2026-11-11T18:32:03.064364Z`; state marked it stale. No new executable was launched.

# Validation

- Documentation diff reviewed; `git diff --check` run on changed tracked documentation.
- No tests or build were run. The active root dev-loop/build lock is unresolved, so live build and runtime evidence are unavailable.
- Existing lane directories were not modified or removed. Full repository status remains constrained by prior permission errors reading the `external/libjuice` submodule metadata.

# Return to Mode 1 and restore the Python dashboard

- Time: 2026-11-11 18:31 EST
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Result: `IMPLEMENTED_NEEDS_WINDOWS_RESTART`

## Observed output and cause

The user's repair output showed the old root watchdog and duplicate root
daemons being stopped, then a new watchdog starting. The new root daemon
reported `waiting for the EXE build slot`; that means it was queued behind the
scheduler lock, not compiling at that moment. The subsequent local status files
showed root daemon PID 23544 and `exe-0.lock` PID 12144. The post-repair process
identity for PID 12144 is not available in this environment.

The earlier Windows snapshot had 28 root dev-loop candidates with a combined
working set of about 419 MB, plus two build workers. The watchdog repeatedly
misclassified live `pythonw.exe` processes as dead; root discovery also omitted
`pythonw.exe`. This likely contributed to memory pressure, though the snapshot
does not include total machine memory or compiler-child memory.

Source inspection found another cause of unsolicited work: every daemon
startup set `build_pending = True`, even when a valid artifact was already
published. A fresh daemon therefore requested a validation build without a
source edit or explicit request.

## Changes

- Existing-artifact startup now marks the build stale but stays idle. Saved
  compiler-input changes still queue builds. Explicit build, launch, server
  start, and server restart requests validate/build before use. A checkout with
  no valid artifact still queues its initial build.
- Kept the EXE scheduler lock. It serializes writers to shared build outputs;
  removing it could allow concurrent compilers and increase memory use.
- Added `devscripts/open-dev-loop-dashboard.bat`. It opens/reuses the Tkinter
  dashboard without creating a Windows Terminal tab; the GUI already has
  animated rainbow status values and a singleton dashboard lock.
- Updated the recovery launcher to stop verified root and lane watchdogs,
  daemons, and `build-only` workers, preserve lane files, then start the root
  dev-loop and dashboard.
- Aligned the active workflow text in the build, dev-loop, and live-development
  documents with Mode 1. Kept Mode 2 descriptions as labeled history; no lane
  directories or edits were deleted.

## Validation and remaining evidence

- `git diff --check` was attempted but Git reported it could not access the
  `external/libjuice` submodule in this environment.
- No build or tests were run. The updated daemon code is not active in the
  user's current process yet; run the updated recovery launcher once to restart
  the root daemon and open the dashboard.
- Runtime review must confirm one root watchdog/daemon/dashboard, no active
  build when the existing artifact is idle, and a build only after a compiler
  input changes or the user requests build/launch. The recovery launcher also
  stops active lane processes but preserves their worktrees.

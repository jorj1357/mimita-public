# Stop compiler work at the first hard error

## Request

The user asked to make a failed build stop before the observed 19-minute wait.

## Change

`devscripts/game_build_worker.py` now reads tracked GCC/MinGW compiler output as
it arrives. On a location-prefixed hard error or fatal error, it terminates that
compiler process tree immediately. The existing worker logic then records the
failed translation unit and cancels sibling compiles. Notes and source snippets
that merely contain the word `error` do not match.

The root dev-loop's existing terminal-failure watcher and 300-second worker
watchdog remain in place. The watchdog is intentionally not lowered: valid
translation units have taken more than 200 seconds under severe machine
pressure. The worker watchdog starts after worker launch; codegen and slot
waits have separate limits.

## Evidence

- Before this change, attempt `game-dll-20261124T230934953855Z-9ace92` failed
  on `src/engine/engine-tick-camera.cpp` after a 77.702-second changed-source
  preflight compile, with total attempt time 95.572 seconds. Thus the current
  system already failed far before 19 minutes. Its logs do not record when the
  compiler first emitted the diagnostic, so they cannot establish how much of
  that 77.702 seconds was post-diagnostic waiting. This change targets that
  previously unmeasured interval.
- A synthetic compiler emitted a GCC-style hard error and then slept for 20
  seconds. The new runner terminated it in under 4 seconds.
- Focused matcher and timeout-classification tests passed 8/8; `py_compile`
  and scoped `git diff --check` passed.
- No real build was requested. The current Mode 3 batch is pending for other
  owners, and no live build or runtime was changed.

## Limit

The new early abort is proven with a synthetic compiler, not yet with a real
MinGW diagnostic. A future root build should record the compiler error time and
worker exit time. Errors without a recognized GCC/MinGW hard-diagnostic line
still use normal process exit, terminal-failure observation, or the worker
watchdog. This does not reduce successful edit-to-live latency; that goal
remains open.

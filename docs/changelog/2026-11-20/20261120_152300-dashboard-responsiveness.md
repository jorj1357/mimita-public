# Dev-loop dashboard responsiveness

- Moved daemon/lane status collection and game-build progress file reads off the Tk event loop. Coordination database reads run on a dedicated reader thread.
- Reused process, checkout, and agent row widgets; skipped unchanged process text/layout work; batched coordination and checkout rows across event-loop callbacks. Reflow now ignores unchanged dimensions, coalesces resize work, and updates only changed wrap widths. The output gutter redraws only when its visible line range changes.
- Added bounded JSON timing events for status, refresh stages, reflow, coordination, and event-loop gaps. Reduced the decorative color animation to 120 ms.
- Fixed startup when no initial current status object is returned: status collection now safely falls back to the on-disk state instead of calling `.get()` on `None`.

## Evidence

- `python -m py_compile devscripts/dev_loop/gui.py devscripts/dev-loop.py` and `git diff --check -- devscripts/dev_loop/gui.py devscripts/dev-loop.py` passed.
- Restarted only the dashboard through the root dev-loop. The live dashboard showed five checkout/lane entries and current build status; PID 28468 reported `Responding=True`. Moved the window directly. Main daemon PID 28216 and active DLL generation 404 remained in place.
- In the latest 25-second timing sample, full Tk refresh callbacks peaked at 202.9 ms and individual measured stages at 90.1 ms. The background status provider peaked at 531 ms. Event-loop gaps peaked at 643 ms during the first five seconds; no >250 ms gap followed during the next ~29 seconds. Earlier iterations showed 7.2-second event-loop/status delays before moving the blocking reads and renders off the main path.
- No C++/EXE/DLL build was requested; the existing generation 404 build was preserved.

The issue remains open: startup still exceeded the 500 ms event-loop target, and the planned 30-minute no-Tk-error observation has not been completed.

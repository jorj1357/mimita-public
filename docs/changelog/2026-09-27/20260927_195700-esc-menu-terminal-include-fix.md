# ESC reset menu compiler fix

- time_utc: 2026-09-27T19:57:00Z
- display_timezone: America/New_York
- display_time: 2026-09-27 15:57:00 EDT
- branch: `afad20a-rebuild`
- result: `PASS_WITH_HUMAN_REVIEW`

## Observed failure

The supplied dev-loop output identified the first compiler error:

```text
C:\mimita-v9\src\gui\menus\pause-menu.cpp:197:13: error: 'Terminal' has not been declared
```

The dev-loop stale-build guard was already working: it printed the diagnostic,
kept the current game running, and refused to relaunch the stale artifact.

## Fix

Added the existing terminal owner declaration to
`src/gui/menus/pause-menu.cpp:23`:

```cpp
#include "devtools/terminal.h"
```

This makes the existing `Terminal::instance().execute("explode")` call compile
without adding another command owner or changing the `explode` command.

## Runtime/build evidence

- The active dev-loop recompiled after the include was added.
- It published build `0059`, then build `0060` after the queued source state
  settled.
- `.dev/state.json` reported build `0060` as current:
  `latest_successful_source_generation: 51`, `source_generation: 51`,
  `latest_build_stale: false`.
- Running process inspection found MiMITA processes from builds `0059` and
  `0060`; the current dev-loop state identifies `0060` as the active build.
- `python -m py_compile devscripts/dev-loop.py`: passed.
- `config/gui/pause-menu.json` parsed successfully.
- `git diff --check` passed, with only existing line-ending normalization
  warnings.

## Human review still needed

Open the build-0060 client and verify the visible flow: ESC → RESET, `YES` on
the right, `NO` returns to the pause menu, ESC closes the confirmation screen,
and `YES` runs the normal explode/death flow. The earlier build-0059 process
should be closed through the dev-loop's normal process ownership path if it is
still visible.

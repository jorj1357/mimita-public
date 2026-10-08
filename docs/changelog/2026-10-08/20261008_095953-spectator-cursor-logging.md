# Spectator cursor logging

Time: 2026-10-08T09:59:53-04:00 (America/New_York)
Branch: `afad20a-rebuild`
Source commit inspected: `90a61a67`

## Change

Added bounded GUI diagnostics for the spectator/freecam cursor investigation.

Events now record:

- `cursor.l_pressed`: every L-key edge, including whether the toggle was accepted;
- `cursor.mouse_lock_set`: the gameplay lock request plus GLFW mode before/after and whether it applied;
- `cursor.pause_open`: Escape pause-menu open and the visible-cursor request;
- `cursor.pause_restore`: pause-menu close/restore behavior;
- `cursor.freecam_override`: spectator/freecam forcing the cursor disabled after another owner requested visibility.

The event fields include match mode, local team number/name, spectator-team flag, replicated actor state, pause state, keyboard-enabled state, freecam state, mouse-lock state, and requested/actual cursor modes where applicable.

## Files

- `src/engine/engine-tick.cpp:247-279` logs every L edge before applying the accepted toggle.
- `src/input/mouse-lock.cpp:22-48` logs every gameplay cursor request and GLFW result.
- `src/gui/menus/pause-menu.cpp:49-77,172-202` logs pause cursor requests and restores.
- `src/engine/engine-tick-camera.cpp:900-934` logs spectator/freecam cursor overrides.
- `config/debuglogger.json` changes the GUI category from `off` to `important`; category-file output remains disabled, so the canonical `events.jsonl` stream is the diagnostic destination.

Pre-existing edits in `config/actor-presets/juggernaut_fighter.json` and `config/analytics.json` were preserved.

## Validation

- JSON configuration parse: passed; `gui_level=important`, `gui_file_output=False`.
- `git diff --check`: passed; only existing line-ending warnings were reported.
- Forced source recompilation: `engine-tick-camera.cpp`, `engine-tick.cpp`, `pause-menu.cpp`, and `mouse-lock.cpp` all compiled.
- Canonical build: `BUILD SUCCESS`, return code 0; timestamped executable `mimita-20261008T100000.exe` linked.
- Version identity: `mimita-20261008T100000.exe --versioninfo` succeeded and wrote `logs/10-08-2026/20261008_095939/events.jsonl`.
- The existing root `mimita.exe` was locked by another user-owned process, so it was not terminated or replaced for runtime testing.

## Runtime and human evidence

The versioninfo probe verified logger initialization and journal identity only; it did not exercise L, spectator freecam, or Escape because those require the real game session. Human acceptance remains: reproduce dead Juggernaut spectator -> press L -> press Escape -> move/click in the pause menu, then inspect the new `cursor.*` records for the first cursor-mode divergence.

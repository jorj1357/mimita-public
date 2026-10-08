# Spectator cursor fix

Time: 2026-10-08T10:03:19-04:00 (America/New_York)
Branch: `afad20a-rebuild`
Source commit base: `90a61a67`

## Change

Fixed the confirmed cursor ownership conflict from `logs/10-08-2026/20261008_100029/events.jsonl`.

1. `src/engine/engine-tick-replay.cpp:233-235` now keeps keyboard input disabled while `PauseMenu::isOpen()` is true. This prevents simulation input from re-enabling spectator freecam during the Escape menu.
2. `src/engine/engine-tick-camera.cpp:903` now lets spectator freecam force `GLFW_CURSOR_DISABLED` only while `MouseLock::locked()` is true. Pressing L while dead can therefore leave the cursor normal.

Existing cursor diagnostics remain active in the L, pause, mouse-lock, and freecam owners. Pre-existing working-tree edits, including `config/accounts/default.json` changing `equipped_slot` from 17 to 18, were preserved and not attributed to this work.

## Validation

- `git diff --check`: passed; only existing line-ending warnings were reported.
- First build caught and then corrected one missing `pause-menu.h` include in `engine-tick-replay.cpp`.
- Forced affected source compilation completed for `engine-tick-camera.cpp` and `engine-tick-replay.cpp`.
- Final canonical build: `BUILD SUCCESS`, return code 0; linked `mimita-20261008T101500.exe`.
- Version probe succeeded: `mimita-20261008T101500.exe --versioninfo`.
- Fresh journal identity: `logs/10-08-2026/20261008_100313/events.jsonl`.

## Evidence boundary

Source and build evidence are complete. The new executable has not yet been human-playtested through a live Juggernaut death, L toggle, and Escape-menu sequence in this session. Human acceptance remains required: while dead/spectating, verify L toggles normal/disabled cursor and Escape keeps the cursor visible and clickable.

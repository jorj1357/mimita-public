# Cursor journal analysis

Time: 2026-10-08T10:01:41-04:00 (America/New_York)
Branch: `afad20a-rebuild`
Journal inspected: `logs/10-08-2026/20261008_100029/events.jsonl`

## Evidence

The journal contains the expected cursor diagnostics from client PID 23216. The decisive sequence is:

- `seq=1442`, `cursor.pause_open`: requested and observed `GLFW_CURSOR_NORMAL` (`212993`), pause open, actor state `3`, mode `juggernaut`, team `1` / `Juggernauts`.
- `seq=1443`, `cursor.freecam_override` 8 ms later: pause still open, but `keyboard_enabled=true`, `any_freecam=true`; cursor changed from `212993` to `212995` (`GLFW_CURSOR_DISABLED`).
- `seq=1410-1412` and repeated later: L is accepted while actor state `3`; `cursor.mouse_lock_set` successfully changes to normal, then `cursor.freecam_override` immediately changes it back to disabled while `mouse_lock_locked=false`.

This confirms both reported bugs in the runtime journal. Escape is not merely a missing cursor image: the cursor is explicitly normal, then spectator freecam disables it while the pause menu remains open.

The journal also shows the local actor is not assigned to the configured spectator team: `local_team=1`, `local_team_name=Juggernauts`, `spectator_team=false`, while `local_actor_state=3`. Spectating is currently represented by actor state, not a team transfer.

## Source cross-check

`PauseMenu::toggle` disables keyboard input, but `src/engine/engine-tick-replay.cpp:233-234` later resets keyboard input to `!Terminal::instance().isOpen() && !isChatOpen()`, ignoring `PauseMenu::isOpen()`. That makes `anyFreecam` true again in `src/engine/engine-tick-camera.cpp:314-315`. The journal's `pause_open=true` plus `keyboard_enabled=true` fields are the runtime proof of this ordering/ownership conflict.

No source code was changed during this analysis. Existing user edits were preserved.

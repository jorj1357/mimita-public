# Pause-menu click fix after freecam cursor work

## Scope

Fixed the reported state where pause-menu buttons hovered and played sounds but clicks did nothing. Preserved the freecam cursor behavior from commit `47247ac4` (`ummmmm monsters for zobie tower we ened to add and stuff`). Existing unrelated working-tree edits were preserved.

## Finding and cause

`src/gui/ui-system-buttons.cpp:40` rejects every button click while `UISys::gDropdownModalActive` is true. `src/gui/menus/online-menu.cpp:410-414` sets that global from an open community-menu dropdown, but no owner cleared it when the user left the community menu. The pause menu therefore inherited a stale click-blocking flag: hover and hover audio still worked, but Leave/Reset/Help clicks were rejected.

The current freecam cursor guard remains source-correct at `src/engine/engine-tick-camera.cpp:902-905`: freecam forces `GLFW_CURSOR_DISABLED` only while `MouseLock::locked()` is true. The pause menu still requests a normal cursor and disables gameplay keyboard input, so the fix does not revert that behavior.

## Change

`src/gui/ui-system.cpp:89-95` now resets `gDropdownModalActive` at the start of every UI frame. The community menu can still set it during its own frame while a dropdown is genuinely open; the stale value cannot leak into the pause menu or another interactive screen on the next frame.

## Validation

- `python build_agent.py`: `Status: SUCCESS`, return code 0, duration 104.01 seconds.
- Fresh executable: `.dev/builds/1949/mimita.exe`.
- `--versioninfo` completed successfully with `RUN_ID=20261010_193419` and journal path `logs/10-10-2026/20261010_193419/events-000001.jsonl`.
- Focused diff check passed for the changed source; unrelated pre-existing working-tree edits remain.
- No live click acceptance was performed in this session. The existing running game was not restarted or replaced.

## Status

PASS_WITH_HUMAN_REVIEW. Test build 1949 in a dead/freecam match: open the pause menu, hover and click Leave, Reset, Help, and Settings. Confirm the cursor remains visible, hover sound plays once per transition, and each click performs its action.

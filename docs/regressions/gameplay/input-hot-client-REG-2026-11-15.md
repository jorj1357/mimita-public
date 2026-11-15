# Hot-client input and mouse look

Date: 2026-11-15
Status: PARTIALLY FIXED — host keys, terminal, equip and `agent.input` verified; physical mouse-look and all UI buttons still need human acceptance
Owner: input/router and game-input owners

## Changelog links

- [Hot input bridge, cosmetic state, hot-status tips](../../changelog/2026-11-15/20261115_162500-hot-input-bridge-cosmetic-state-hot-status.md)
- [Texture filter and visible world](../../changelog/2026-11-15/20261115_090500-hot-client-texture-filter-world-visible.md)

## Symptom

WASD, Escape, Grave/terminal, weapon equip, and UI buttons did not respond in
the hot client. The client could be foreground while GLFW still reported it
unfocused.

## Root cause

The game DLL links its own static GLFW copy. Resident-dispatched keys reached
the game only through `on_event`; DLL `glfwGetKey`/`glfwGetMouseButton`/
`glfwGetCursorPos` could not see the resident window. Direct GLFW reads were
scattered across gameplay and UI.

## Fix

- Host key events feed `InputCommandSystem`/`InputRouter`.
- `InputRouter` is the single normalized owner for keys, pointer, buttons, text,
  focus, agent input, and future gamepad/touch/VR sources.
- UI reads `InputRouter` pointer/button state.
- Terminal/menu/equip paths consume the same input owner.
- `agent.input` commands drive the same actions without focus.
- `config/input/binds.json` owns bindings; JSON wins over terminal edits.

## Evidence

Run `20261115_172313`, PID `24736`:

- `agent.input look 120 0`, `agent.input equip 2`, and
  `agent.input terminal_toggle` all delivered and completed.
- `equipslot2` resolved to `shotgun`.
- `terminal_open=true` after `agent.input terminal_toggle`.
- `input.state.key_callback_count` advanced and `walkforward_held` changed after
  host key injection.

## Remaining

- Physical mouse-look must be accepted by a human.
- All UI buttons must be exercised through the router.
- Proper `CAP_INPUT` remains the cold replacement for the Win32 fallback.
- L must be read through `InputRouter`; the DLL-local `glfwGetKey` path could not
  see the host key. This was fixed in `engine-tick.cpp`.
- Dev-loop command queue writes must target the primary client's recorded
  per-instance queue; writing the legacy run-level file was accepted but not
  observed as delivered.

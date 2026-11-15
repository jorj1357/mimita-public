# Resident window and input capability

Date: 2026-11-15

## Rule

The hot game DLL must not call the DLL-local GLFW with a resident window pointer,
and must not use `glfwGetCurrentContext()` as a proxy for window ownership. The
resident host owns the window, its GLFW state, cursor mode, and clipboard.

## Boundary

`HostCapabilityId::Window` (`WindowCapabilityV1`) provides:

- window size and framebuffer size;
- window position;
- cursor position and cursor positioning;
- focus;
- cursor mode (`0` normal, `1` hidden, `2` disabled);
- focused and iconified state;
- mouse-button and key state;
- bounded clipboard copy in and out.

The game uses `game/window-bridge-module` and falls back to `InputRouter` for
cursor/mouse/key state when the host capability is unavailable. No GLFWwindow
crosses into the bridge as a required argument.

## Cursor ownership

Mouse look has exactly one owner: the resident cursor callback feeds
`GameInputKind::Cursor` events into `InputRouter`, and the router applies deltas
only while pointer lock is active. The Win32 absolute-position fallback is used
only until the first host cursor event, and then permanently disabled for that
process. Camera look is intentionally unavailable while pointer lock is off.

This prevents the cursor-inside-window double-application/jitter caused by the
resident GLFW callback and the Win32 fallback both owning yaw/pitch.

Implementation path: `HostRuntime::installInputCallbacks` installs
`inputCursorCallback`; it enqueues `GameInputKind::Cursor` with the latest
position (consecutive cursor events coalesce); `handleGameInputEvent` calls
`InputRouter::ingestCursor`. The resident renderer's direct camera callback is
replaced in the hot client by this callback. The Win32 fallback remains only for
processes that never receive host cursor events.

## Known exceptions

The static gate allows three documented exceptions:

- `src/game/game-cli.cpp`: a hidden replay-export FBO self-test creates its own
  GLFW context. It is not the live game window path.
- `src/replay/replay-export-subprocess.cpp`: replay export owns a separate
  hidden capture window/context. Migrating it to a host export capability is
  deferred; the tradeoff is an isolated, explicit process-owned context rather
  than a live-window bypass.
- `src/physics/movement/actor-triangle-spike.cpp`: a legacy diagnostic probe.
  It is not gameplay/UI and is scheduled for removal or conversion to
  `gRenderer`.

These exceptions must remain explicit in `config/game-static-gate.json`; do not
widen the gate with a blanket GLFW allowance.

## Callers

GUI, GUI editor, map editor, avatar editor, chat text input, notifications, music
widget, pause/menu diagnostics, and engine cursor/focus checks use the bridge.

## Acceptance

- `rg "glfwGetCurrentContext|glfwGetCursorPos|glfwGetWindowSize|..."
  src/gui src/mapeditor src/audio src/notifications src/avatar` returns only
  intentional resident-owner calls, not hot UI/editor calls.
- Clipboard paste/copy works through the bridge.
- Cursor mode, focus, iconified, window size, and cursor position change through
  the host capability in the live hot client.

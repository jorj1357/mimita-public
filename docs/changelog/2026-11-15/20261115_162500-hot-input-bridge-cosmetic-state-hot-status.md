# Hot input bridge, cosmetic shader-state reset, live hot-status tips

## Task

- Task ID: agent-b-20261115-hot-input-cosmetic-status
- Owner: agent B
- Focus: restore hot-client input, stop cosmetic shader-state leakage, live hot-reload tips
- Status: hot generation 194 built; host key events and terminal toggle verified; cosmetic size/position still under investigation
- Date, time, timezone: 2026-11-15T16:25:00-05:00
- Branch: current shared checkout

## Pre-existing changes

The working tree already contained the packet/domain split, hot-client default,
texture-filter fix, PostFX work, cosmetic fallback, and NPC avatar changes. This
record claims only the edits listed below.

## Root cause found

The game DLL links its own static GLFW copy. The resident window belongs to the
EXE's GLFW copy. In the game DLL:

- `glfwGetWindowAttrib(GLFW_FOCUSED)` returned false even when Win32 reported the
  game window as foreground;
- `glfwGetKey`, `glfwGetMouseButton`, and `glfwGetCursorPos` never observed
  resident input;
- game-installed GLFW key callbacks did not replace the resident callbacks.

Evidence from run `20261115_155323` and `20261115_155554`:

- `input.state.focused=false`
- `input.state.key_callback_count=0`
- `input.state.w=...=false` while actual input existed
- `gameWindow` Win32 foreground could be true while `GLFW_FOCUSED` stayed false

This is a cold architectural gap. The proper fix is a resident normalized input
capability (`CAP_INPUT`); this change uses a hot bridge so input can work now.

## Exact changes

### `src/input/input-commands.h` / `.cpp`

- Added `beginFrame()` / `updatedThisFrame()` so input state is sampled once per
  frame without duplicate edge processing.
- Added `ingestHostKey()` and `mHostKeyDown[512]`. Host-dispatched key events now
  feed the same action states used by movement.
- Added `keyCallbackCount()` and `focused()` diagnostics.
- `update()` uses host key state when host input has been seen, with GLFW
  polling retained as the resident-EXE fallback.
- `isKeyHeld()` no longer rejects a host-fed key merely because the game DLL's
  static GLFW copy reports no focus.

### `src/game/game-input-module.cpp`

- `handleGameInputKey()` now ingests host keys before routing.
- Added rate-limited `input.host-key` evidence.
- Added host-path handling for Grave (open/close terminal) and Escape
  (password/pause/terminal close). The frame-level GLFW poll cannot see these
  keys in the hot DLL.
- Uses `gRenderer->window` for modal handlers instead of the game DLL's
  `glfwGetCurrentContext()`.

### `src/engine/engine-tick.cpp` / `engine-tick-net.cpp`

- `InputCommandSystem::beginFrame()` runs once per frame.
- `engineTickNet` guarantees one `InputCommandSystem::update()` when the fixed
  loop did not already sample.
- Added rate-limited `input.state` with focus, keyboard-enabled, terminal/chat/
  pause, scenario/AI/replay, raw-key, action-held, and callback-count fields.
- Added `recoverStuckKeyboardGate()` for a disabled keyboard flag with no active
  terminal/chat/pause/replay/scenario owner.
- Added Win32 visible-window focus attempt and `win32_focused` evidence.

### `src/input/host-input-fallback.h` / `.cpp` (new, game-owned)

Temporary hot-DLL adapter over the resident Win32 window:

- `setGameWindow()` stores the game window pointer;
- `gameWindowHandle()` resolves the HWND through GLFW native access, title, or
  foreground fallback;
- `gameWindowFocused()` and `focusGameWindow()`;
- `mouseDown()`, `cursorPos()`, `keyDown()` use Win32 state for UI/input while
  the DLL's static GLFW cannot see the resident window.

This is explicitly transitional. `hot-client-live-fix.md` now records the
required cold `CAP_INPUT` replacement.

### `src/gui/*`, `src/notifications/notifications.cpp`

- Replaced hot UI mouse polling for left/right buttons with
  `HostInputFallback::mouseDown`.
- Replaced UI cursor polling with `HostInputFallback::cursorPos` in the button,
  text-input, chat, pause, and notification paths.

### `src/avatar/cosmetic-system.h` / `.cpp`

- `renderCosmetics()` now receives `view` and `projection`.
- It explicitly sets `view`, `projection`, `uUsePartModels=0`,
  `uUseInstanceModel=0`, `uOutlinePass=0`, `uAlphaCutoff=0`, and neutral debug
  view before drawing.
- This closes shader-uniform leakage from the preceding body/outline/world
  draw. `pushState()` only restores GL state, not uniforms.
- Added `cosmetic.transform` evidence with attachment/model translation and
  scale, mesh vertex count, and the reset uniform values.
- Updated local and replay cosmetic call sites to pass view/projection.

### `src/game/hot-status-tips.h` / `.cpp` (new, game-owned)

Live hot-reload tips using the existing `NotificationSystem`/tip card:

- `JsonFileWatch` on `build/game` and `build/hot-reload`;
- parsing runs through the resident job executor, not the gameplay thread;
- reads `current.json`, `progress.json`, `ready.json`, and
  `change-report.json`;
- publishes detected/ready/failure tips with generation, phase, files, server
  and client tick, timestamp, and failure guidance;
- deduplicates by generation/result/phase/timestamp;
- emits `hot-status.notice` evidence.

No persistent HUD was added.

### Documentation

- `docs/regressions/gameplay/black-font-glyph-boxes-REG-2026-11-15.md`
- `docs/regressions/gameplay/README.md` Entry 37
- `docs/specs/hotreload/hotreload.md` cold-to-hot promotion pattern
- `docs/specs/hotreload/hot-client-live-fix.md` input cold gap and `CAP_INPUT`
  replacement target

## Runtime evidence

- Build generation 194:
  `2026-11-15T16:15:58.313262Z`
- Run `20261115_161501`, client PID `26648`
- Generation 194 activated in the same PID at 16:16:31:
  `game.generation.activated` (`old_generation=1`, `generation=194`)
- Host key event reached game DLL via resident `WM_KEYDOWN`:
  `input.host-key` with key 96 and action 1
- Terminal opened through the host path:
  `input.state.terminal_open=true`, `keyboard_enabled=false`
- `hot-status.notice` published generation 194 ready tip in the same PID
- `cosmetic.transform` for local `jason` reports:
  - attach position around `[120.28, 30.96, 313.67]`
  - attach scale around `[1.0, 1.0, 1.0]`
  - model scale around `[1.03, 0.997, 0.963]`
  - `use_part_models=0`, `use_instance_model=0`, `outline_pass=0`

## Not proved / unresolved

- The Jason mask still appears as a large close-up/fixed object in the current
  visual capture. The cosmetic transform log shows normal scale and a plausible
  actor transform, so the remaining cause is likely camera/actor overlap,
  projection, or a different renderable near the camera. This is not yet
  accepted as fixed.
- Mouse clicks were not yet exercised by a human after the Win32 fallback.
- No human acceptance of terminal text, movement, or UI buttons.
- Proper removal of the per-DLL GLFW dependency requires a cold `CAP_INPUT`.

## Regression records

- `docs/regressions/gameplay/black-font-glyph-boxes-REG-2026-11-15.md`
- `docs/regressions/gameplay/black-world-texture-filter-REG-2026-11-15.md`
- `docs/regressions/gameplay/black-cosmetic-glb-REG-2026-11-15.md`

## Next exact step

Verify with human input that WASD moves, Grave opens the terminal, Escape closes
it, and UI buttons click. Then trace the remaining Jason close-up by logging
camera mode, camera-to-local-actor distance, and projected cosmetic bounds, and
compare the local actor against the renderable actually occupying the center of
the screen.

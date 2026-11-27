# Keyboard and controller eaten by repeated ALT presses

- Recorded: 2026-11-27T07:04:00Z
- Issue: `issue-dbff2cfc9a6c`
- Status: FIXED IN GENERATION 783 — physical keyboard/pad acceptance pending
- Domain: gameplay / input focus

## Symptoms

The user reported that keyboard input was eaten and that ALT appeared to be
pressed over and over. The physical controller also appeared dead at the same
time. The game window was visible and rendering.

## Root cause

The uncommitted focus work of 2026-11-27 added the Windows foreground unlock to
`cachedWindow()` in `src/input/host-input-fallback.cpp`:

```cpp
if (!cached) {
    keybd_event(VK_MENU, 0, 0, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    ...
}
```

`cachedWindow()` is called from `keyDown`, `mouseDown`, `cursorPos`,
`gameWindowFocused`, and `gameWindowHandle`, many times per frame. When
`findGameWindow()` returned null (window handle temporarily unresolved), every
call synthesized an ALT down/up pair. That spammed the menu key, turned
keystrokes into menu accelerators, and made both keyboard and pad input appear
dead.

The committed base (`git show HEAD:src/input/host-input-fallback.cpp`) has **no
ALT call in `cachedWindow()`**; it only checked whether the foreground window
belonged to the game process. The regression was introduced by the uncommitted
focus fix, not by the controller code. The controller itself had been working
under the old always-poll input path.

## Evidence

| Evidence | Detail |
|---|---|
| `git diff HEAD -- src/input/host-input-fallback.cpp` | the working tree added the ALT block inside `cachedWindow()`; HEAD does not contain it |
| Live run `20261127_063344_rl-play` | `input.gamepad.muted`/`resumed` fire correctly, but the user saw ALT pressed repeatedly and keys were swallowed |
| Live run `20261127_054417` | `input.gamepad.poll` axes remain at rest while the user tries the pad |
| User report | "still it doesnt ... even kebord inpts eaten, its pressing alt over and ovr" |

## Fix

- ALT is synthesized only by the explicit `focus_game_window` command, in
  `focusGameWindow()`, at most once per 500 ms.
- `cachedWindow()` never sends input events; it rescans the game window at most
  once per second.
- An ALT key-up was sent to release the stuck modifier, and the offending
  client was stopped.
- Generation 783 contains the fix. A physical keyboard and pad test with the
  game window focused is the remaining acceptance.

## Why it looked like a controller failure

The same commit window also made the new `capture_mode: "focused"` the default:
the pad is read only while the game window has OS focus. That is the requested
behavior (mode 2), not a regression, but it means a background window no longer
drives the car. Mode 1 (`"always"`) is documented as the old behavior.

## Rule

A per-query path must never synthesize OS input events. Focus side effects
belong to an explicit command, are throttled, and are logged. The pad capture
policy is data (`config/input/gamepad.json` `capture_mode`), not a hidden
side-effect of focus queries.

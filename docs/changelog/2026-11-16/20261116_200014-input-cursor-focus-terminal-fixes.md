# Input cursor, focus, and terminal toggle fixes

- Date: 2026-11-16 20:00 EST
- Owner: opencode
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Scope: resident host input bridge, cursor-mode normalization, terminal toggle
  ownership, and runtime evidence.

## Summary

The resident host now delivers real mouse-button events to the game DLL, starts
the game window focused, and exposes cursor mode in the game's normalized
`0/1/2` space instead of raw GLFW enum values. The terminal grave shortcut now
has one owner when host key events are active. The game-side logging bridge no
longer lets a JSON serialization error terminate the server.

## Changes

- Added `GameInputKind::MouseButton` and `HostRuntime::inputMouseButtonCallback`;
  host mouse-button events now reach `InputRouter::ingestMouseButton`.
- Added `GameStartupV1::unlockCursor` and forwarded it into the game-owned
  `gUnlockCursorOnLaunch`.
- Focused and showed the resident game window after render-device creation.
- Normalized `windowGetCursorMode` to `0=normal, 1=hidden, 2=disabled` and
  mapped `windowSetCursorMode` back to GLFW modes. Gameplay and UI code already
  compares against `2`, so raw `212995` previously made `cursorCaptured` false.
- Removed the duplicate polling-based terminal toggle in
  `engineTickState` when host key events are present; the event owner remains
  `handleGameInputKey`.
- Removed stale `MIMITA_GAME_DISABLE` fallback branches from the resident path
  (previously changed in this working tree) and kept resident gameplay non-
  fallback.
- Added focused terminal open/close events at `Terminal::toggle`.
- Hardened `mimita::game::log_bridge::emitEvent` to dump fields with
  `error_handler_t::replace` and catch serialization exceptions. Previously an
  uncaught JSON exception in the log bridge terminated the server.

## Runtime evidence

- Run `20261117_005045`, client PID `688`, EXE
  `2026-11-17T00:50:03.657687Z`, game generation `290` then `291`:
  - `input.host-key` key `96` reached the game DLL.
  - Exactly one `terminal.opened`; a second press produced one
    `terminal.closed`.
  - `input.host-mouse` button `0` reached the game DLL.
  - `weapon.attack.local_request` showed `weapon=revolver`,
    `gameplay_active=true`, and a fired shot after `lc` set cursor mode `2`.
  - Pause menu set cursor mode `0`; cursor was visible.
- Build target gate: `PASS` (`exe=46`, `cold=45`, `tests=0`,
  `game_in_exe=0`, `cold_undefined=0`).
- Audit triage: `UNRESOLVED=0`.
- Cutover-1 remains green except `gameplay_live_fix_proven=false`.
- Server crash reports for `PID 2304` and `PID 22292` show
  `CXX_EXCEPTION_GCC`; the symbolized stack enters
  `mimita::game::log_bridge::emitEvent` and `nlohmann` serialization.

## Open issue

- The server process exited after the host player was killed by an NPC rocket
  in two runs (`PID 2304` at `20261117_005045`, `PID 22292` at
  `20261117_005850`). Crash reports are in
  `%LOCALAPPDATA%\Mimita\crashes\crash-2026-11-16_19-51-39-2304.txt` and
  `crash-2026-11-16_19-59-19-22292.txt`. The log-bridge hardening is the
  bounded fix, but the game generation containing it had not published when
  this note was written because the dev-loop game-build worker stopped with a
  stale serialization lock. The stale lock was removed; the server must be
  rerun through the dev loop to prove it survives a player death.

## Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.

## How to reverse

- Remove the mouse-button ABI/callback and startup `unlockCursor` field.
- Restore raw GLFW cursor-mode return values and direct set mode.
- Restore the unconditional polling terminal toggle.
- Restore direct `fields.dump()` if the log-bridge hardening is not wanted.
- Keep the terminal toggle diagnostic only if needed for tracing.

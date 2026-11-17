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
- Build identity: EXE `2026-11-17T01:01:29.903571Z`; game generation `294`.
- Build target gate: `PASS` (`exe=46`, `cold=45`, `tests=0`,
  `game_in_exe=0`, `cold_undefined=0`).
- Audit triage: `UNRESOLVED=0`.
- Cutover-1 remains green except `gameplay_live_fix_proven=false`.
- Server crash reports for `PID 2304` and `PID 22292` show
  `CXX_EXCEPTION_GCC`; the symbolized stack enters
  `mimita::game::log_bridge::emitEvent` and `nlohmann` serialization. The
  reports are in `%LOCALAPPDATA%\Mimita\crashes\crash-2026-11-16_19-51-39-2304.txt`
  and `crash-2026-11-16_19-59-19-22292.txt`.
- Run `20261117_135530`, server PID `28212`, game generation `294`:
  - The server survived three `server.death.transition` events from NPC rocket
    explosions (`spawn_generation` 4, 5, and 6) and was still alive with no new
    crash report.
  - The client received `input.host-mouse` button `0`; the press produced a
    `weapon.attack.local_request` (dry fire because the player was dead at the
    time), proving the click path remained live.
- The dev-loop game-build worker was wedged behind a stale
  `.dev/build-serialization.lock` owned by a dead PID. The lock was removed and
  the daemon restarted with permission; generation `294` published.

## Documentation added

- Added `docs/handoffs/networking-hot-cold-contract-20261117.md`, the approved
  coordination brief between the ICE hot/cold split and the
  reconnect/health/event/config work. It records file ownership, event
  ownership, config keys, resident transport core API, swap/reattach invariants,
  shared-file sequencing, and the validation gate.
- This is coordination documentation only; it is not build or runtime evidence.

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

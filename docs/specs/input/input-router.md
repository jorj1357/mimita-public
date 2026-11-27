# Input Router

## Purpose

`InputRouter` is the single hot owner for local input normalization. Every
keyboard, mouse, agent, scenario, and future gamepad/touch/VR source feeds the
same snapshot. Gameplay, UI, camera, terminal, and bindings read that snapshot;
they do not call the platform backend directly.

The OS/window event pump remains resident. The router is hot.

## Source adapters

- `Host`: resident-dispatched key/char/scroll/focus events.
- `Win32`: hot fallback for the resident window's mouse/pointer state.
- `Agent`: `agent.input` commands used by live development.
- `Scenario`: recorded/replayed input.
- Future: `Gamepad`, `Touch`, `VRHead`, `VRHand`.

Adapters only produce normalized tokens. They do not own actions, gameplay, or
camera policy.

## Gamepad capture mode

`config/input/gamepad.json` owns `capture_mode`, which decides whether a MiMITA
process reads a shared controller at all. The default is `focused`.

| Value | Name | Behaviour |
|---|---|---|
| `focused` | Mode 2 (default) | The pad is polled only while this process's game window has OS focus (`InputRouter::gameWindowFocused()`). When the window is not focused, axes and buttons are zeroed and no gameplay input is produced. A headless or server process with no focused game window never captures the pad. |
| `always` | Mode 1 (old behaviour) | The pad is polled whenever the process is open, even while another application is in front. This is the behaviour before the focus policy and is retained only for a deliberate single-process setup or a debugging session. |

- The mode is loaded at startup and logged as `input.gamepad.capture_mode`.
  Transitions are logged once as `input.gamepad.muted` /
  `input.gamepad.resumed`.
- Rationale: the first MiMITA process that opens the pad must not consume it
  from the foreground application. With parallel server/client runtimes, mode 1
  lets a background client eat every controller event.
- Mode 2 is presentation/input ownership only. It does not change vehicle
  physics, bindings, or the virtual `gamepad_virtual` test path; virtual input
  remains available to agent scenarios regardless of focus.

## Raw DualShock 4 HID fallback

GLFW 3.4's Windows joystick backend can report a Bluetooth DualShock 4 as
present and standard-mapped while every axis stays at about `1.5e-05`.
`GamepadInput` therefore keeps a direct HID fallback for Sony `VID 054C`
devices `PID 09CC`/`05C4`:

- When GLFW's standard and raw joystick states are both idle, enumerate the HID
  interface with SetupAPI, open it shared with overlapped I/O, and decode USB
  report `0x01` or Bluetooth report `0x11` into the same GLFW-standard axis
  (0-5) and button (0-14) slots the rest of the input path already uses.
- The raw HID backend is selected only for an idle GLFW device and remains in
  effect while reports arrive. If it stalls or the device leaves, GLFW is tried
  again. Virtual input still wins.
- Selection and the first decoded report are logged once as
  `input.gamepad.hid.selected` and `input.gamepad.hid.report`; the periodic
  `input.gamepad.poll` names the active source as `Wireless Controller (raw
  HID)`.
- `devscripts/hid-ds4-probe.py` is the read-only standalone probe for the same
  device. Run it only while the game is stopped because it opens the HID
  interface for the duration of its sample.
- DirectInput is not a usable fallback for this device on the affected host:
  `joy.cpl` did not list it even though Windows and Roblox could read it.

## Versioning

- `input_version` covers the normalized event/action contract.
- `binds_version` covers `config/input/binds.json`.
- Both are explicit and may evolve independently.
- New source types must not change gameplay action semantics.

## Frame and tick contract

Render frame:

1. `beginFrame(frameId, clientTick, serverTick)` samples each source once.
2. Mouse look is applied immediately to the camera.
3. UI pointer/hover state is updated immediately.

Fixed 60 Hz tick:

1. Action edges are consumed exactly once.
2. Held movement/combat state is snapshotted.
3. `MpInput` is built and sent to the server.

The server never receives render-frame input. Look stays immediate on the
client; the latest yaw/pitch travel with the next fixed-tick packet.

## Bindings

`config/input/binds.json` is authoritative:

```json
{
  "version": 1,
  "input_version": 1,
  "default": {},
  "binds": { "walkforward": "W" },
  "accounts": {}
}
```

Tokens remain backend-neutral: `key:W`, `mouse:left`, `pad:...`, `touch:...`,
`vr:...`. Account-specific overrides are future work; the structure is reserved
now. JSON reload wins. Terminal `input.bind` changes runtime state;
`input.save` writes the JSON explicitly.

## Latency

The frame path performs no allocations, locks, filesystem work, or extra thread
hops. Input sampling is measured separately and should remain near 0 ms. A
frame budget target is not satisfied by hiding work in a queue.

## Logging

All router events use existing `StructuredLogger` categories. AI/contributors
enable them in `config/debuglogger.json`; do not create a parallel log file.
See the Logging Catalog in `docs/specs/debug-logging/debug-logging.md`.


## Pointer lock and L behavior

- L toggles gameplay pointer lock, matching the established behavior.
- Lock uses the router/Win32 window owner, not the game DLL's static GLFW.
- Locked: cursor is clipped to the game window, hidden, and recentered for
  unlimited look.
- Unlocked: clipping and hiding are removed, and `firstMouse` is reset once.
- The router is the only look owner. Do not apply host callback deltas and Win32
  deltas in the same frame.
- The resident cursor callback feeds `GameInputKind::Cursor` events into the
  router. After the first host cursor event, the Win32 absolute-position
  fallback is disabled for the process.
- Look deltas are applied only while pointer lock is on.
- Pointer-lock and focus transitions reset the cursor baseline, so no delta is
  carried across a warp, lock change, or alt-tab.

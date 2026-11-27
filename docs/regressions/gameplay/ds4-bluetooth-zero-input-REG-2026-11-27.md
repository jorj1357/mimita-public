# Bluetooth DualShock 4 detected but all input axes zero

- Recorded: 2026-11-27T08:30Z
- Issue: `issue-dbff2cfc9a6c` (input), `issue-768e8a2aff63` (runtime cleanup)
- Status: FIXED IN GENERATION 794 with the raw-HID fallback; human-confirmed
  ("yes controller works")
- Domain: gameplay / input backend
- Related: `keyboard-alt-spam-REG-2026-11-27.md` (separate ALT/keyboard
  regression; fixed in generation 783)

## Symptoms

The physical controller worked on 2026-11-26 and stopped being usable on
2026-11-27:

- Windows listed `HID-compliant game controller`, VID `054C` PID `09CC`
  (DualShock 4 v2, Bluetooth).
- GLFW 3.4.0 reported `connected=true`, `standard_mapping=true`,
  `axis_count=6`, `button_count=15`, `name="Wireless Controller"`.
- Every axis was `1.5259e-05`; no button activity; the raw joystick fallback
  was also zero; `input.gamepad.raw.selected` never fired.
- `joy.cpl` showed nothing for the device.
- Roblox read the same controller normally.
- Mode 1 (`capture_mode: "always"`) changed nothing; focus flags were
  `focused=true`, `input_focused=true`, `win32_focused=true`.

The earlier "pad readable" evidence was a connected poll with all-zero axes, not
a live input sample. The keyboard was separately broken by an ALT-spam focus bug
in the same period (linked record).

## Evidence

| Evidence | Detail |
|---|---|
| Working physical input | run `20261126_233910`, build `2026-11-26T22:52:33`: `input.gamepad.state` `Wireless Controller`, `virtual=false`, `steer=-1.0/+1.0`, `throttle=0.578`, `jump=true` |
| Broken, mode 2 | run `20261127_074804_rl-play2`, `capture_mode=always`: live focus flags true, every axis `1.525e-05` |
| HID probe | `devscripts/hid-ds4-probe.py` enumerated `054C:09CC` and read 547-byte Bluetooth reports `11 c0 00 7b 7f 7d 7c 08 00 ...` while the game was stopped |
| Raw HID runtime | run `20261127_081928`: `input.gamepad.hid.selected` VID 1356/0x054C PID 2508/0x09CC; `input.gamepad.hid.report` `mode=bluetooth`, `report_id=17`, `length=547`; `input.gamepad.poll` `name="Wireless Controller (raw HID)"`, `device_id=-2`, live axes |
| Human acceptance | user: "yes controller works"; visible runtime `20261127_082112_controller-proof` (client 27812, gen 794) selected the raw HID and was left for the press |

## Root cause

GLFW 3.4.0's Windows joystick/gamepad backend enumerated the Bluetooth DS4 and
supplied a standard mapping, but stopped delivering its HID reports; both
`glfwGetGamepadState` and the raw joystick state returned zeros. Windows,
the HID stack, and Roblox received the same controller. DirectInput is not a
useful fallback on this host because `joy.cpl` does not expose the device.

The game had a **single input backend assumption**: if GLFW said the device was
connected and mapped, it was treated as working. There was no backend probe, no
"connected but no data" state, and no alternate path. The focus/capture/ALT
changes of the same day masked the failure and made it look like a bindings or
mode problem.

## Best solution as of 2026-11-27

1. **Raw-HID DS4 backend** in `src/input/gamepad-input.cpp`:
   - Enumerate the Sony HID interface (`054C:09CC` / `05C4`), open it shared
     with `FILE_FLAG_OVERLAPPED`, keep one outstanding read, and drain up to
     four reports per poll.
   - Decode Bluetooth report `0x11` (`0xc0 0x00` prefix, common block at +3) and
     USB report `0x01` (common block at +1): sticks, L2/R2, face/shoulder/PS/
     L3/R3, D-pad, into the existing GLFW-standard axes 0-5 and buttons 0-14.
   - Select it only when GLFW standard and raw joystick states are idle; retry
     GLFW if HID stalls or leaves. Virtual input still wins and the focus
     capture gate still applies.
   - Log `input.gamepad.hid.selected` once and `input.gamepad.hid.report` once
     with the first report; the periodic `input.gamepad.poll` names the source
     `Wireless Controller (raw HID)`, `device_id=-2`.
2. **Read-only probe** `devscripts/hid-ds4-probe.py`, run with the game stopped:
   SetupAPI/HID enumeration, USB 0x01 and Bluetooth 0x11 decoding, prints only
   state changes. This is the independent proof that the pad sends data.
3. **Link** `-lhid -lsetupapi` in the game-DLL link
   (`devscripts/game_build_worker.py`), no compile-flag change.
4. **Spec**: `docs/specs/input/input-router.md` records the raw-HID fallback and
   why DirectInput is not usable for this device here.

## What went wrong before

- A connected device with a standard mapping was assumed to be readable.
  `connected=true`, `standard_mapping=true`, and six axes told us nothing about
  live data; the all-zero value was the only clue.
- No alternate backend existed for a device Windows could see and Roblox could
  read. The user was told "it should work" from a poll snapshot instead of a
  live axis change.
- `joy.cpl` was treated as a controller test; it only covers DirectInput and
  shows nothing for this Bluetooth HID device.
- Same-day focus/capture/ALT work confused the diagnosis: mode 2 muted the pad
  when unfocused, the ALT spam ate the keyboard, and the shared launch killed
  peer clients. The mode 1 test (pad bypassed the gate) finally proved the data
  path was the problem.
- The visible runtime client and the probe had to contend for the same HID
  handle; the probe is deliberately run with the game stopped.

## Rule

An input backend is "working" only when a live axis or button changes in the
run journal. Log the backend, device, report id/length, and a live-data state;
never report a connected all-zero device as healthy. Keep the raw-HID fallback
and the probe. Prefer platform-neutral backend selection (raw HID, SDL2,
Windows.Gaming.Input) when a device is visible to Windows but not readable by
GLFW.

## Remaining / follow-up

- Review other input methods and controllers over time: the follow-up issue is
  `issue-dbff2cfc9a6c` (input) and the backend-resilience issue created with
  this record.
- USB DS4 report `0x01`, hotplug, multiple pads, per-player assignment, and
  other devices (Xbox, DualSense, 8BitDo) are not all proven yet.
- The GLFW 3.4.0 Bluetooth gap itself is not fixed; the raw HID path is the
  supported workaround today.

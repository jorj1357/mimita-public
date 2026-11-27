# Raw DualShock 4 HID fallback restores the physical controller

- Task/agent: `rocket-league-controller` (`opencode/deepseek-v4.1-flash`)
- Date, time, timezone: 2026-11-27T08:25Z, ISO 8601
- Status: implemented, probe-proven, built (gen 790 and 794), runtime-proven for
  HID selection and live raw axes; physical press -> car motion still needs the
  human at the pad.
- Linked handoff: `docs/handoffs/2026-11-27-rocket-league-controller-handoff.md`

## Pre-existing changes

The shared checkout already contained extensive uncommitted work from other
agents (Rocket League mode, body rig/ADS, Bloodtest, build/coordination, config,
and map work). This session preserved all of it and edited only
`src/input/gamepad-input.cpp`, `devscripts/game_build_worker.py`,
`devscripts/hid-ds4-probe.py` (new), and `docs/specs/input/input-router.md`.

## Requested behavior

The handoff asked for a HID report probe and a raw-HID DS4 backend because the
physical Bluetooth DualShock 4 stopped reaching the game: Windows enumerated
`VID 054C PID 09CC`, Roblox read it, but GLFW reported a connected,
standard-mapped device with every axis at about `1.5e-05`. Mode 1 did not help.

## Specification alignment

`docs/specs/input/input-router.md` still owns capture mode and logical actions.
The new text records the raw-HID fallback; GLFW standard, raw joystick, and
virtual input remain the fallbacks. No gameplay action semantics changed.

## Exact implementation changes

1. `devscripts/hid-ds4-probe.py` (new): read-only SetupAPI/HID probe for Sony
   `054C:09CC`/`05C4`. It decodes USB `0x01` (common block at +1) and Bluetooth
   `0x11` (`0xc0 0x00` prefix, common block at +3) and prints only state
   changes. It is run with the game stopped so it never contends during play.
2. `src/input/gamepad-input.cpp`: added a raw DualShock 4 HID backend behind
   `_WIN32`. It enumerates the Sony HID interface, opens it shared with
   `FILE_FLAG_OVERLAPPED`, keeps one outstanding read, drains up to four reports
   per poll, and decodes sticks, L2/R2, face/shoulder/PS/L3/R3 and D-pad into the
   existing GLFW-standard axes 0-5 and buttons 0-14. It is selected only when
   GLFW standard and raw joystick states are idle; GLFW is retried if the HID
   device stalls or leaves. Virtual input still wins and the focus gate still
   applies. Logs `input.gamepad.hid.selected` once and
   `input.gamepad.hid.report` once with the first raw report.
3. `devscripts/game_build_worker.py`: game-DLL link now includes `-lhid` and
   `-lsetupapi`. This changes only the link identity, not compile flags.
4. `docs/specs/input/input-router.md`: documented the raw-HID fallback and why
   DirectInput is not usable for this device on the affected host (`joy.cpl` was
   empty while Windows and Roblox read it).

DirectInput was not implemented: `joy.cpl` showed nothing for the same device,
so it cannot be a useful fallback here. Raw HID is the working path.

## Evidence

Source/probe (game stopped):

- `python devscripts/hid-ds4-probe.py` enumerated
  `054C:09CC` and read 547-byte Bluetooth reports. A correct decode is
  `11 c0 00 7b 7f 7d 7c 08 00 ...` -> `lx=123 ly=127 rx=125 ry=124 l2=0 r2=0
  dpad=8 buttons=none`. This proved the pad was sending reports while GLFW was
  idle.

Build:

- Generation 790 (`game-dll-20261127T080854268361Z-c02a71`) compiled
  `src/input/gamepad-input.cpp` in 8.558 s and linked `-lhid -lsetupapi`;
  postmortem: compile 11.528 s, link 0.896 s, mean host CPU 70.58%, minimum free
  memory 4878.6 MB, no dominant phase.
- Generation 794 rebuilt the same code against later peer changes.
- `input.gamepad.hid.selected`, `input.gamepad.hid.report`, and the first-report
  strings are present in `build/game/generation-790/game-g790.dll` and
  `generation-794` linked with the new libs.

Runtime (pinned gen 794, EXE `2026-11-27T08:12:40.178185Z`):

- Isolated test run `20261127_081928` (mode 64, client pid 4568):
  `input.gamepad.hid.selected` at t=1.938 with vendor `1356` (0x054C), product
  `2508` (0x09CC); `input.gamepad.hid.report` showed `mode=bluetooth`,
  `report_id=17`, `length=547`,
  `raw="11 c0 00 7b 7f 7e 7c 08 00 dc 00 00 3f cd 16 00"`,
  `lx=-0.0394 ly=-0.0079 rx=-0.0157 ry=-0.0315 l2=0 r2=0`.
  `input.gamepad.poll` then reported `name="Wireless Controller (raw HID)"`,
  `device_id=-2`, live axes instead of the old all-`1.525e-05`; `input.state`
  had `focused/input_focused/win32_focused=true` and
  `vehicle.input.local` had `gamepad_connected=true` with a seated
  `vehicle_id=1`.
- Visible runtime `20261127_082112_controller-proof` (mode 60, client pid 27812)
  is also focused on all three flags, selected raw HID, and is seated idle. It
  is left online for the human press/drive proof.

## Remaining human acceptance

A physical stick/button press must produce changing `input.gamepad.poll`
axes/buttons and `vehicle.input.local` throttle/steer/jump/boost with the car
moving. That cannot be synthesized honestly; the visible runtime above is left
running for that test.

## Coordination friction

A shared `--request launch` used dev-loop process reconciliation twice and
killed clients belonging to other agents (runs `20261127_080709`,
`20261127_080920`, and `20261127_081651`). The first two had manifests that were
not classified as parallel tests; the third was classified but the exact
`(pid, process_started_utc, executable)` key did not match, so the kill was
issued and the launch then refused with the process still alive. Evidence and a
proposed tolerance-based classification fix are appended to
`issue-768e8a2aff63`. The surviving servers were left untouched. The controller
proof therefore used isolated test runs and the runtime registry, which do not
run shared-launch reconciliation.

## Files changed

- `src/input/gamepad-input.cpp`
- `devscripts/game_build_worker.py`
- `devscripts/hid-ds4-probe.py` (new)
- `docs/specs/input/input-router.md`
- `docs/changelog/2026-11-27/20261127_0825-ds4-raw-hid-fallback.md`

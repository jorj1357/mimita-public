# DualShock 4 input regression record and backend-resilience review

- Task/agent: `rocket-league-full` (continuation docs after the controller fix)
- Date, time, timezone: 2026-11-27T08:35Z, ISO 8601
- Status: documentation and issue only; no code or build in this session
- Linked handoff: `docs/handoffs/2026-11-27-rocket-league-controller-handoff.md`

## Pre-existing changes

The shared checkout already contained the raw-HID DS4 implementation from
`rocket-league-controller` (generation 794), the ALT/keyboard fix from generation
783, and extensive unrelated uncommitted work. This session preserved all of it.

## Requested behavior

The user confirmed the physical controller works and asked for:

1. a regression document with the best current solution, what was done, and what
   went wrong before;
2. an issue for agents to review over time, especially how to avoid this class of
   problem and how to support other controllers and input methods.

## Exact changes

- New regression record:
  `docs/regressions/gameplay/ds4-bluetooth-zero-input-REG-2026-11-27.md`.
  It records the working 2026-11-26 run, the all-zero GLFW state, the read-only
  HID probe, the raw-HID fallback (Bluetooth report `0x11`, USB `0x01`), the
  live-axis runtime evidence, and the user confirmation.
- `docs/regressions/gameplay/README.md` entry 46 links the record.
- New improvement issue `issue-e15006f9618a`: input backend resilience, live
  data detection, other controllers/input methods, hotplug, multiple pads, and
  preventing a connected-but-zero device from being reported healthy.

## Evidence

- Working physical input: run `20261126_233910`, build
  `2026-11-26T22:52:33`: `steer=-1.0/+1.0`, `throttle=0.578`, `jump=true`.
- Broken: run `20261127_074804_rl-play2` (`capture_mode=always`), live focus
  flags true, every axis `1.525e-05`.
- Probe: `devscripts/hid-ds4-probe.py` read 547-byte Bluetooth reports
  `11 c0 00 7b 7f 7d 7c 08 00 ...`.
- Raw HID: run `20261127_081928` selected VID `054C` PID `09CC`;
  `input.gamepad.poll` `name="Wireless Controller (raw HID)"`, `device_id=-2`,
  live axes. Visible runtime `20261127_082112_controller-proof` was left for the
  human; the user confirmed "yes controller works".

## Build

No build. Generation 794 already contains the raw-HID fallback and the
generation-783 ALT/focus fixes.

## Remaining

- USB report `0x01`, hotplug, multiple pads, per-player assignment, and other
  controller classes are not all proven.
- The GLFW 3.4 Bluetooth gap itself is not fixed; the raw HID path is the
  supported workaround today.
- Keep the controller-proof runtime only while the user is testing; stop it when
  done.

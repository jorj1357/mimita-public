# Handoff: Rocket League controller + remaining mode work

Written 2026-11-27T07:55Z by `rocket-league-full`. Everything is reproducible
from this checkout; much of the work is uncommitted (the working tree is the
artifact). Do not revert unrelated uncommitted work.

## 1. Current state

- Game DLL generation **786** published; EXE build **2026-11-27T07:45:59**.
- Build 783 contained: the `focus_window` ALT-spam fix, fuzz respawn bounds,
  and the settle righting. Build 786 contains other agents' changes.
- The user can see the game, but **the physical controller is not read** and
  the keyboard was broken earlier today by a focus bug.
- My runtimes are stopped. A separate ADS/TDM run (`20261127_074621`, pids
  26880/27260) belongs to another agent; do not stop it.

## 2. Controller evidence (the important part)

The physical controller **did work** on 2026-11-26:

| Run | Build | Evidence |
|---|---|---|
| `20261126_233910` | `2026-11-26T22:52:33` | `input.gamepad.state` `Wireless Controller`, `virtual=false`, `steer=-1.0/+1.0`, `throttle=0.578`, `jump=true` |

Now:

- Windows has the device: `HID-compliant game controller`, VID `054C`
  PID `09CC` (DualShock 4 v2, Bluetooth).
- GLFW 3.4.0 reports `connected=true`, `standard_mapping=true`,
  `axis_count=6`, `button_count=15`, name `Wireless Controller`.
- Every axis is `1.5259e-05`; no button activity; the raw joystick fallback is
  also zero, so `input.gamepad.raw.selected` never fires.
- `joy.cpl` (DirectInput) shows nothing.
- **Roblox reads the same controller fine.**
- Focus is not the problem now: `focused=true`, `input_focused=true`,
  `win32_focused=true`.
- A **mode 1** test (`capture_mode: "always"`) was run as
  `20261127_074804_rl-play2`; the gate was bypassed and the axes were still
  zero. Mode 1 is not the fix; the config was reverted to `"focused"`.

So: the pad sends reports (Roblox), Windows exposes the HID device, but
**GLFW's Windows joystick backend returns no data for this Bluetooth DS4 now**.
It returned data at 22:52 yesterday. The working EXE was
`2026-11-26T22:52:33`; the current EXE is `2026-11-27T07:45:59`. The input
read code in `gamepad-input.cpp` only added capture/raw/logging; the standard
`glfwGetGamepadState` path is unchanged. Likely causes, in order:

1. EXE-level window/GLFW initialization changed between the 22:52 and 07:45
   builds (renderer/`main-init`/host window), so `glfwPollEvents` no longer
   updates the joystick state in this runtime.
2. The controller is now in a different HID mode (USB charging vs Bluetooth);
   GLFW 3.4.0 cannot parse this report. Roblox uses a different backend.
3. A GLFW 3.4.0 regression for this Bluetooth report.

## 3. Recommended next steps (controller)

1. **HID report probe**: read the Sony HID input report (report `0x11` for
   Bluetooth, `0x01` for USB) and log the raw bytes while the user presses.
   This proves which report mode is active. A standalone probe is fine; do not
   hold the device while the game runs.
2. **A/B a GLFW 3.3.9 DLL** (same API) in a test build. If it reads the pad,
   downgrade/keep 3.3.9; the current DLL is 3.4.0 dated 2024-02-23.
3. **Add a raw-HID DS4 backend** in `src/input/gamepad-input.cpp` (or a new
   `gamepad-hid.cpp`): enumerate VID `054C` PID `09CC`/`05C4`, open shared,
   read reports `0x11`/`0x01`, parse sticks/triggers/buttons, feed the existing
   `GamepadBinding` values. Latch when GLFW is idle; log
   `input.gamepad.hid.selected` and the first raw report. Keep GLFW/virtual as
   fallbacks.
4. If a dependency is preferred instead, vendor **SDL2** and use
   `SDL_GameController` (Roblox-like) or use `Windows.Gaming.Input.RawGameController`.
5. Acceptance: with the game focused, `input.gamepad.poll` shows changing
   axes/buttons and the car drives/jumps/boosts. Cite the run id and generation.

## 4. Keyboard/focus regression (fixed in 783)

- The ALT spam came from `cachedWindow()` in
  `src/input/host-input-fallback.cpp`: the uncommitted focus fix added
  `keybd_event(VK_MENU)` inside a function called by every key/mouse/focus
  query. When the window handle was unresolved it fired ALT many times per
  frame. The committed base (`HEAD`) had no ALT there.
- Fixed in 783: ALT is only in the explicit `focus_game_window` command,
  throttled; `cachedWindow` only rescans once per second.
- The capture policy is `config/input/gamepad.json` `capture_mode`:
  `"focused"` (default, mode 2) reads only while the game window is focused;
  `"always"` (mode 1) is the old always-on behavior. Documented in
  `docs/specs/input/input-router.md`.
- The focus gate uses `InputRouter::gameWindowFocused()`; the live journal shows
  all three focus flags true now, so the gate is not the remaining pad issue.

## 5. Settle guarantee (proven partial)

The user requires: no car ever rests on an edge/corner/side; only wheels or
roof; settling = speed <0.5 m/s and angular <0.5 rad/s.

- Implemented in `src/vehicles/vehicle-system.cpp`: any supported, still car in
  a non-wheels/non-roof pose gets a 7.5 rad/s righting impulse plus a 1.2 m/s hop
  after 15 updates (0.25 s).
- Fuzz adds random spin, random air roll, and random boost; classifies after a
  0.5 s supported settle; respawns out-of-arena cars.
- Run `20261127_070109_rl-fuzz4`, gen 783: **8 kicks, 8 wheels, 0 side, 0
  tilted, 1 side recovery, 1 escape**. The long run stalled because the respawn
  point `(0,0,1)` overlaps the center ball and the car never settles. Fix: use a
  safe kickoff such as `(-35,0,1)`. Then rerun a 300+ kick fuzz and require
  `side_rests==0`, `tilted_rests==0`, wheels dominating roof, escapes reported.
- Arena2 containment escape (`issue-a3e088b8181c`): a high-speed random car
  gets behind the back wall into the goal-recess pocket. The respawn catches it
  but the geometry should be solid; investigate the lower back-wall ramp.

## 6. Other completed work

- **Static map double-scale** (`issue-b09435bb9d9d`): the headless builder
  applied `blockTransform` (unit-cube scale) to metre-space collision. Fixed
  with `blockPlacement`; arena2 floor/ceiling are one solid box; ball uses
  analytic sphere-world contact. Regression record:
  `docs/regressions/collisions/static-map-double-scale-REG-2026-11-27.md`.
- **Ball**: rest `0.9093` (0.0068 above radius), rolling spin ratio 0.95-0.98,
  bounce 0.6; late rolling drag 0.052/s (target 0.03). Friction 0.35,
  restitution 0.6.
- **Input capture mode** and **raw fallback** in `gamepad-input.cpp`; mode 2
  default.
- **Build/issue rules**: AGENTS now has event-driven build approval (no
  timers; ask once, record no-response, build anyway) and issues-as-forum
  threads. Specs: `docs/specs/dev-loop/event-driven-coordination.md`,
  `docs/specs/dev-loop/issue-forums.md`. Improvement issue
  `issue-a07a25102c92`.
- Taskbar visibility issue `issue-529f9267ce05`; parallel-runtime cleanup issue
  `issue-768e8a2aff63` with a resource sample.

## 7. Remaining mode work (after the controller)

1. Fix the fuzz safe respawn and prove `side_rests==0` over 300+ kicks.
2. Closed-loop aerial controller; freestyle flip reset (`issue-1ffbfffaefa0`).
3. Ball drag 0.052 -> 0.03; wall/50/50/dribble scenarios.
4. Cruise mode 73; tick VFX; hitbox debug view; camera JSON; free-play D-pad.
5. Pro scripted bot; 1v1; 3v3; two-client match.

## 8. Build/coordination constraints

- One checkout; root dev-loop only builder. One approved build at a time.
  Queue a build note with `--changes ... --why ... --expected-seconds ...`,
  file `agent-coord build request`, ask once, and if no response record
  `no_response` and seal. Do not sleep on timers.
- Read `game-generation.ready.postmortem` after each build and append the
  measured cause to the issue.
- Preserve the uncommitted tree; do not revert other agents' work. Stop only
  your own runtimes and verify the PIDs.

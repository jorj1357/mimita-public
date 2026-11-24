# Non-blocking runtime identity + vendored controller mappings

**Date:** 2026-11-24 UTC
**Agent:** vehicle-arena-controller
**Issue:** `issue-e5a086b0a441`

## Purpose

Unblock joining a server created from the dev-loop GUI and make the controller
mapping surface broader without adding a second input framework.

## Root cause

The client rejected a perfectly valid local server because the dev-loop had
injected a stale `MIMITA_EXPECTED_RUN_ID`.

Evidence from the user's run:

- `logs/11-24-2026/Gameterminal_log_034542.txt`
  `[NET CONNECT] rejected server identity expectedRun=20261124_081518 actualRun=20261124_084513`
  with identical expected/actual build ids.
- `logs/11-24-2026/Server_log_040346.txt` shows the new server loaded
  `arena1`, bound `0.0.0.0:1357`, and then reported
  `players=2 ... mapReadyReceived=0 ... not-spawned ... input=0`.

The server was healthy. The client refused its `WELCOME`/join identity metadata
and never sent map-ready or spawn authorization, so no car could be seated and
KBM/gamepad input could never move a vehicle.

## Source changes

- `src/network/multiplayer-tick.cpp`: runtime run/build identity differences now
  emit `network.server-identity-warning` with `blocked=false` and continue the
  normal join/map-sync/spawn path. The blocking `rejectUnexpectedServer` path is
  removed for these metadata checks.
- `src/gui/gui-main.cpp`: a GUI-created local server clears a stale
  `MIMITA_EXPECTED_RUN_ID` before connecting. `ScopedServerJournalEnvironment`
  now updates both the Win32 environment and the CRT environment with
  `_putenv_s`, so the child server writes its own journal instead of inheriting
  the client's `client-01/events.jsonl`.
- `config/input/gamecontrollerdb.txt` and its zlib license are vendored from the
  open-source SDL_GameControllerDB project.
- `src/input/gamepad-input.cpp` loads that database with
  `glfwUpdateGamepadMappings` before polling and logs
  `input.gamepad.mappings`.
- `config/input/gamepad.json` names the mappings file.

## Spec changes

- `docs/specs/networking/networking.md`: new
  “Non-blocking runtime identity metadata” contract. Run/build identity metadata
  is diagnostic only; a mismatch must never reject, tear down, or suppress
  map-ready/spawn. Protocol, content, and explicit ownership checks remain
  separate.
- `docs/specs/vehicles/vehicles.md` §11.5: gamepad support uses GLFW standard
  mapping + raw fallback + the vendored mapping database. No external binary
  input library is required. A future in-repo Windows Raw Input/HID fallback is
  deferred until a real device test proves GLFW insufficient.

## Build and runtime evidence

The first batch failed in an unrelated UI compile (`UISys` / four-argument
`uiBeginFrame`). The owning agent repaired those files. The next build published
game generation 590.

**Identity warning proof** — runtime `20261124_111810_stale-proof`, client
started with deliberately stale `MIMITA_EXPECTED_RUN_ID=stale-guard-test` and
`MIMITA_EXPECTED_BUILD_ID=stale-build-test`:

- `network.server-identity-warning` fields: `blocked=false`,
  `run_differs=true`, `build_differs=true`, `result=continued`.
- The same client then reached `vehicle.seat.anchor` with
  `has_local_driver=true`. The identity mismatch did not block join, map sync,
  spawn, or vehicle seating.

**KBM proof** — runtime `20261124_110752_arena-proof-1`:

- A real `WM_KEYDOWN` W event produced `input.host-key key=87` in the client.
- Client `vehicle.input.local` reported `throttle=1.0` for vehicle 2.
- Server `vehicle.input.server` reported `actor_id=1`, `vehicle_id=2`,
  `throttle=1.0`, and a changing authoritative position/speed.

This confirms the KBM complaint was the fixed server-mirror input bug plus the
identity rejection, not a missing vehicle control path.

**Controller mapping proof** — runtime `20261124_112630_map-proof-2`,
generation 592:

- `input.gamepad.mappings` reported `accepted=2299`, `rejected=0`,
  `loaded=true`, `bytes=616074`.
- The game module now initializes its own GLFW joystick subsystem; the prior
  result was `glfw_error="The GLFW library is not initialized"` and zero
  accepted mappings.

Game generation 593 adds a one-time physical joystick census
(`input.gamepad.device`) so a missing pad can be distinguished from an unmapped
pad. The census is source/build verified; no physical pad was connected during
the automated runs.

## Residual

- The physical PS4 pad still needs a live `gamepad_status` check. No joystick was
  present during the automated mapping proof.
- The in-game controller rebinding/calibration screen remains pending.

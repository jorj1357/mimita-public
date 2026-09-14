# Step 3: hot movement.main + install of movement groundwork and fork visuals

- EST timestamp: 2026-09-13 21:47:00 EDT (UTC 2026-09-14T01:47:00Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + headless self-tests); runtime human proof pending

## What was implemented

### Step 3 — hot `movement.main`
- `src/hot-reload/modules/movement-system.cpp` reimplements the movement step as a
  real hot generic-runtime system (priority 0 in `gameplay.60`, so free-fly at
  priority 10 can still override in create mode):
  - reads `Transform`/`Velocity`/`MovementIntent`/`Body` components via
    `readComponent`;
  - computes yaw-relative horizontal acceleration/friction, gravity, jump, and a
    fall-speed clamp from hot-editable `MovementTuning` constants;
  - calls `physics.moveCapsule` (kernel capsule-vs-world solve) and publishes the
    result through `requestMovementOverride`, which makes the kernel apply it and
    skip the built-in `physicsMainUpdate`;
  - writes `Transform`/`Velocity` back so observers see coherent state.
- **Opt-in** via `hotmovement [0|1]` (shared flag `GAME_MODE_FLAG_HOT_MOVEMENT`),
  so it cannot silently regress normal gameplay before parity is proven.
- `editor-behavior.cpp` registers the `hotmovement` command (it owns the shared
  state pointer).

### Installed (previously blocked)
- `MovementStateV1` + `physics.moveCapsule` capability (steps 1–2).
- Fork visualization (`drawWireBox` + `forkOp` enumeration + hot colored boxes).
- `LiveBehavior::setDispatchWorld` is now called each tick so movement/query
  capabilities have world access.

## Evidence

- `python build_agent.py` -> `BUILD SUCCESS` (50 compiled, relinked).
- Startup registration: `[GENERIC_RUNTIME] package=mimita.core systems=5
  commands=3 schemas=1` (commands = hotdemo, modecreate, hotmovement).
- Self-tests PASS: creation, ragdoll-slice, live-code, hot-authoritative,
  entity-slice, project, phase456, telemetry.
- `-fsyntax-only` clean (EXE + DLL) for all changed TUs.

## Pending / next

- Step 4: parity + determinism tests for `movement.main` vs the built-in step
  (N-tick state comparison, no-allocation guard, fixed 60 Hz), then consider
  default-on.
- Step 5: fold free-fly into `movement.main` as the create-mode branch.
- Step 6: runtime human proof (edit `movement-system.cpp` live; edit editor source
  live; verify fork boxes; type `modecreate 1` / `hotmovement 1`).
- Then: multiplayer READY/switch-at-tick-N, Tool/Inventory, network-policy
  systems, asset providers.

## Files

Changed: `src/hot-reload/modules/movement-system.cpp`,
`src/hot-reload/modules/editor-behavior.cpp`, `src/hot-reload/game-api.h`,
`src/live-code/live-behavior.cpp`, `src/live-code/live-editor.cpp`,
`src/sim/simulate-tick.cpp`, `src/editor/creation-mode.h/.cpp`.

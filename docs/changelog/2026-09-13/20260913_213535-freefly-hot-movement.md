# Free-fly (design A): hot movement override system

- EST timestamp: 2026-09-13 21:35:35 EDT (UTC 2026-09-14T01:35:35Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + headless self-tests); runtime human proof pending

## What was implemented

### Generic kernel movement override (one generic primitive, not a feature slot)
- `game-api.h`: `GameRequestMovementOverrideFn` + `GameplayContextV1.requestMovementOverride`
  (append-only); `GameSharedStateV1.localPlayerEntity`.
- `generic-runtime.*`: `beginMovementTick`, `requestMovementOverride`,
  `consumeMovementOverride`, `movementOverrideActive`, `sharedState()`.
- `live-behavior.cpp`: `capRequestMovementOverride` capability wired into the
  gameplay context.
- `simulate-tick.cpp`: runs `gameplay.60` hot systems **before** the movement
  step, publishes the local player entity into shared state, and when a hot
  system requests an override, applies position/velocity/yaw and **skips the
  built-in physics step** for that tick (noclip). No `GAME_EVENT_MOVEMENT_STEP`.

### Hot free-fly system (design A)
- `modules/editor-behavior.cpp`: `movement.freefly` self-registers via
  `MimitaHotPackage::SystemRegistrar` in `gameplay.60` at priority 10 (before
  `movement.main`). When creation mode is on it reads the local player's
  `Transform`, `MovementIntent`, and `AimIntent` components through the generic
  read capability and requests a free-fly movement override:
  - WASD → camera-relative horizontal movement (`right*moveX + forward*moveY`),
  - vertical → jump/freeze (up/down),
  - camera aim direction from `AimIntentComponent`.
- The player entity remains real; only movement/collision handling changes.

### Note
- `movement.main` remains a stub (a later system will own the real built-in
  movement math). Free-fly does not move movement.main itself.

## Evidence

- `-fsyntax-only` clean: `generic-runtime.cpp`, `live-behavior.cpp`,
  `simulate-tick.cpp`; DLL-side `editor-behavior.cpp`.
- Hot package DLL link success with 9 sources.
- `python build_agent.py` -> `BUILD SUCCESS` (18 compiled, relinked).
- Startup registration observed: `[GENERIC_RUNTIME] package=mimita.core
  systems=5 commands=2 schemas=1` — `modecreate` is a runtime-registered command.
- Self-tests PASS: creation, live-code, hot-authoritative, entity-slice, project,
  phase456, telemetry, ragdoll-slice (exit 0; generic runtime active).

## Pending

- Runtime human proof: `modecreate 1`, free-fly with WASD + vertical, `CTRL+LMB`
  selection, wheel cycling, `CTRL+C/V/X`, combat suppression.
- Fork edits are recorded but not rendered (visual fork pass).
- Multiplayer READY/switch-at-tick-N using `moduleSetHash`/`logicalCodeHash`.

## Files

Changed: `src/hot-reload/game-api.h`, `src/hot-reload/generic-runtime.h/.cpp`,
`src/live-code/live-behavior.cpp`, `src/sim/simulate-tick.cpp`,
`src/hot-reload/modules/editor-behavior.cpp`.

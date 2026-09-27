# Step 4: movement primitive extraction + determinism/parity self-test

- EST timestamp: 2026-09-13 21:51:06 EDT (UTC 2026-09-14T01:51:06Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + self-tests); runtime human proof pending

## What was implemented

### Extract the capsule solve (one owner)
- `src/physics/movement/move-capsule.h/.cpp`: `Physics::moveCapsuleStep(
  MovementStateV1&, const World*, float dt)` — the single kernel capsule-vs-world
  solve (integrate + `collideWithWorld` + `depenetrateWorld`).
- `live-behavior.cpp` `capMoveCapsule` is now a thin wrapper calling it. The ABI
  capability and the self-test share one implementation.

### `--movement-selftest` (new)
`src/physics/movement/movement-selftest.h/.cpp`, registered in `game-cli.cpp`:
- free-fall determinism (two identical runs equal),
- gravity applied + finite state,
- parity with a reference `integrate()` (no world) within 1e-4,
- not grounded without a world,
- floor collision: a capsule above a synthetic floor does not fall through,
  becomes grounded, and its vertical velocity settles.

## Evidence

- `python build_agent.py` -> `BUILD SUCCESS`.
- `mimita.exe --movement-selftest` -> **PASS** (8/8 checks).
- All other self-tests PASS (creation, ragdoll-slice, live-code,
  hot-authoritative, entity-slice, project, phase456, telemetry).
- `-fsyntax-only` clean for `move-capsule.cpp`, `movement-selftest.cpp`,
  `live-behavior.cpp`, `game-cli.cpp`.

## Notes

- This proves the kernel primitive hot movement depends on (determinism +
  collision), not bit-parity between the simplified hot step and the full
  built-in `physicsMainUpdate`. Bit-parity remains future work if the hot step is
  to become default-on; the opt-in `hotmovement` flag keeps normal movement
  unchanged meanwhile.

## Next

- Step 5: fold free-fly into `movement.main` (create-mode branch); remove the
  separate `movement.freefly` override.
- Step 6: runtime human proof (edit `movement-system.cpp` live; verify fork boxes;
  `modecreate 1` / `hotmovement 1`).
- Then: multiplayer READY/switch-at-tick-N, Tool/Inventory, network-policy
  systems, asset providers, audio/UI/NPC migration, platform providers.

## Files

New: `src/physics/movement/move-capsule.h/.cpp`,
`src/physics/movement/movement-selftest.h/.cpp`.
Changed: `src/live-code/live-behavior.cpp`, `src/game/game-cli.cpp`.

# Step 4b: hot movement parity harness + double-gravity fix

- EST timestamp: 2026-09-14 11:06:07 EDT (UTC 2026-09-14T15:06:07Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + self-tests); runtime human proof pending

## Correctness fix (found while writing 4b)

- Double gravity: `Physics::moveCapsuleStep` applied gravity internally **and** the
  hot `movement.main` also subtracted gravity. Fixed by making the capsule solve
  the single owner of gravity:
  - `MovementStateV1` gained `gravityScale`; `moveCapsuleStep` uses
    `-9.81 * gravityScale`.
  - `movement.main` now only sets horizontal velocity and the jump impulse, sets
    `gravityScale = kTune.gravity / 9.81`, and no longer integrates gravity.

## `--movement-parity-selftest` (new)

`src/physics/movement/movement-parity-selftest.h/.cpp`, registered in
`game-cli.cpp`. It loads the hot package (`HotReloadSystem::startup()`), builds a
floor world, creates a component entity, sets the shared hot-movement flag, and
drives the **real registered `movement.main`** through the generic runtime and
capabilities each fixed step (`runDomain(GAME_DOMAIN_GAMEPLAY, ...,
LiveBehavior::hostContext(...))`), consuming the movement override. Asserts:
- hot package active with systems;
- hot movement produced overrides;
- two identical runs are bit-identical (hot path determinism);
- the body lands on the floor and reaches vertical rest (no fall-through);
- the landing height is within 0.35 m of the kernel capsule primitive from the
  same start;
- the result is finite.

## Evidence

- `python build_agent.py` -> `BUILD SUCCESS`.
- `mimita.exe --movement-parity-selftest` -> **PASS** (7/7); registration log
  shows `movement.main` (priority 0) + demo + banana systems.
- `mimita.exe --movement-selftest` -> **PASS** (8/8).
- All other self-tests PASS (creation, ragdoll-slice, live-code,
  hot-authoritative, entity-slice, project, phase456, telemetry).
- `-fsyntax-only` clean for `move-capsule.cpp`, `movement-parity-selftest.cpp`,
  `movement-selftest.cpp`, `game-cli.cpp`; DLL-side `movement-system.cpp`.

## Honest scope

- This is deterministic integration parity of the hot path vs the kernel capsule
  primitive. It is **not** bit-parity against the full built-in
  `physicsMainUpdate` (which has dash/down-dash/freeze/air-strafe/special
  movement the hot step does not yet implement). Making the hot step default-on
  still requires porting those and a record/replay comparison; the opt-in
  `hotmovement` flag keeps built-in movement authoritative meanwhile.

## Next

- Step 6: runtime human proof (in-game).
- Optional: port dash/freeze/air-strafe into `movement.main`, then a
  `physicsMainUpdate` record/replay parity harness before default-on.
- Then: multiplayer READY/switch-at-tick-N, Tool/Inventory, network-policy
  systems, asset providers, audio/UI/NPC migration, platform providers.

## Files

New: `src/physics/movement/movement-parity-selftest.h/.cpp`.
Changed: `src/hot-reload/game-api.h`, `src/physics/movement/move-capsule.cpp`,
`src/hot-reload/modules/movement-system.cpp`, `src/game/game-cli.cpp`.

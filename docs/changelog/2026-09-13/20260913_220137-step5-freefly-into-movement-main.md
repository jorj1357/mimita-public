# Step 5: fold free-fly into movement.main (single movement owner)

- EST timestamp: 2026-09-13 22:01:37 EDT (UTC 2026-09-14T02:01:37Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + headless self-tests); runtime human proof pending

## What was implemented

### One movement owner
- `src/hot-reload/modules/movement-system.cpp` (`movement.main`) now owns both
  modes:
  - **create mode** (`GAME_MODE_FLAG_CREATION`) → camera-relative free-fly/noclip
    (WASD + vertical from jump/freeze), no gravity/collision, via
    `requestMovementOverride`;
  - **otherwise**, opt-in `GAME_MODE_FLAG_HOT_MOVEMENT` → the normal hot step
    (gravity, accel/friction, jump) using `physics.moveCapsule`.
  - If neither flag is set, the system returns and the kernel runs the built-in
    step.
- Removed the separate `movement.freefly` system and its `SystemRegistrar` from
  `editor-behavior.cpp` (the editor package now only owns selection/overlay/edits
  and the commands).

## Evidence

- `python build_agent.py` -> `BUILD SUCCESS`.
- Startup registration: `[GENERIC_RUNTIME] package=mimita.core systems=4
  commands=3` (systems: demo.gameplay, demo.render, demo.custom, movement.main;
  free-fly system gone).
- All self-tests PASS: movement, creation, ragdoll-slice, live-code,
  hot-authoritative, entity-slice, project, phase456, telemetry.
- `-fsyntax-only` clean (DLL) for `movement-system.cpp`,
  `editor-behavior.cpp`.

## Pending / next

- Step 6: runtime human proof — `modecreate 1` free-fly, selection/overlap/clipboard
  + visible fork boxes, `hotmovement 1`, and live edits of `movement-system.cpp`
  and editor sources with the same EXE/world/EntityIds.
- Step 4b (optional): bit-parity harness vs `physicsMainUpdate` before making the
  hot step default-on.
- Then: multiplayer READY/switch-at-tick-N, Tool/Inventory, network-policy
  systems, asset providers, audio/UI/NPC migration, platform providers.

## Files

Changed: `src/hot-reload/modules/movement-system.cpp`,
`src/hot-reload/modules/editor-behavior.cpp`.

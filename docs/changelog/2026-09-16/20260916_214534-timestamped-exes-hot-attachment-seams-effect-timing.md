# Timestamped EXEs + hot attachment seams + explosion timing

- UTC timestamp: 2026-09-16T21:45:34Z
- Branch: `8292026stash`
- Commits: none (working tree; concurrent unrelated edits preserved)
- Result: `PASS` (hot build + cold build SUCCESS; all selftests PASS)
- Built executable: `mimita-20260916T174307.exe`

## Task

1. Make each build produce a NEW uniquely named executable so agents/humans can
   build and test their own changes without being blocked by a running EXE.
2. Make the tool attachment/socket math and the effect pipeline hot-editable
   (hot seams), fix the attached-model grip, and remove the rocket-effect
   flicker/missing-explosion problems.

## Build system

- `build_agent.py`: every agent build links `mimita-<local timestamp>.exe`
  (e.g. `mimita-20260916T174307.exe`), reports the name/path, and writes it to
  `build/changelog.txt` and `build/build-result.json`. It no longer refuses just
  because a different mimita executable is running; it only refuses to relink the
  exact target that is running. Build lock/history preserved.
- `buildv3.py` (new): the human-facing entry. Shares the same lock/pipeline by
  delegating to `build_agent.main()`; also produces a new timestamped EXE.
- `build.py`: `stage_runtime_dlls` skips a shared runtime DLL held by a running
  game (keeps the existing copy) instead of failing the build.
- `docs/operations/build-and-exe/build-and-exe.md`: rewritten for the
  timestamped-exe rule (agents `python build_agent.py`, humans
  `python buildv3.py`), replacing the old "single canonical mimita.exe" rule.

## Hot seams (cold additions; one-time)

- `game-api.h`: `GAME_CAP_SOCKET_RAW` + `GameSocketRawV1` (entity-local bone +
  mesh bind, no actor transform/yaw) and `GAME_CAP_MESH_BOUNDS` +
  `GameMeshBoundsV1` (model-local AABB).
- `live-behavior.cpp`: implemented + registered both. `capSocketRaw` returns the
  entity-local attachment point; `capMeshBounds` reads the mesh AABB.
- `presentation-render.cpp/.h`: `GpuMesh` stores its AABB; `meshBounds()` accessor.
- `effect-part-render.cpp`: the mesh-effect draw enables `GL_BLEND` (alpha fade).
- `presentation-entities.cpp`: builds `look` with `glm::radians(player.yaw)`
  (removed the last degrees-as-radians producer).

## Hot (live-editable)

- `attachment.cpp`: the attachment world transform is now composed in hot C++ —
  `actorPos × Rz(radians(actorYaw)) × socket.raw × mount × gripRecenter` — with
  the cold `socket.query` retained only as a fallback. Grip recentre uses
  `mesh.bounds`; units, mount and recentre are all editable live.
- `hit-visuals.cpp`: explosion primitives use a >= 2-tick lifetime (they were
  aged by `EffectPartSystem::update` before the render pass and died first),
  treat `spawnTick == 0` (the cold `effect.request` path) as "starts now", and set
  an explosion `replayType` so distant detonations use the long cull.

## Validation

- `python build_game_dll.py` -> success.
- `python build_agent.py` -> `Status: SUCCESS`, produced
  `mimita-20260916T174307.exe`.
- `--hot-combat-selftest` -> PASS (explosion tests updated to assert the new
  `ExplosionState` tick timeline).
- `--live-code-selftest`, `--production-loop-selftest`, `--glb-consumer-selftest`,
  `--tool-entity-continuity-selftest`, `--content-resource-selftest` -> PASS.
- `git diff --check` clean.

## Human confirmation still required (live)

- Grip: the weapon should sit in the right hand; tune the recipe mount
  (`viewPosition`/`viewRotation`) live if the seat is off.
- Rocket explosion: consistent red-sphere timeline + smoke, no flicker, visible
  through the correct distance, occluded by walls.
- Confirm hot edits to grip/explosion layers apply live without restarting.

## Pre-existing changes

Concurrent sessions continue to edit animation/tool-visual code; those edits were
preserved untouched.

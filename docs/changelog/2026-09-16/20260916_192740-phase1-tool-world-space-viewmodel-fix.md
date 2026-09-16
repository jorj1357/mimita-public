# Phase 1 — first/third-person tool models draw in world space (visible)

- UTC timestamp: 2026-09-16T19:27:40Z
- Branch: `8292026stash`
- Commits: none (working tree; concurrent unrelated animation edits preserved)
- Result: `HOT BUILD PASS; COLD BUILD/SELFTEST PENDING (mimita.exe running)`
- Evidence class: source + hot-DLL compile; live visual + cold selftest outstanding

## Task

Phase 1 of the hot tool-visual fix: the player's own weapon models were invisible
in both first and third person. Make the models visible regardless of camera.

## Root cause (pre-existing)

`hot.tool-presentation` marks the local actor's tool as
`HOT_ATTACHMENT_CONTEXT_VIEW` (regardless of camera mode), and
`presentationMeshTick` submitted the socket's **world** transform while tagging it
`GAME_RENDER_MESH_SPACE_VIEW`. The renderer then drew it with an identity view
(`presentation-render.cpp`), i.e. world coordinates interpreted as camera-relative
-> far off-screen. The old cold `WeaponViewModel` drew the world rightArm
transform with the normal world view, and third-person other-actor tools (WORLD
context) do the same, which is why only the local player's own weapon vanished.

## Change

- `src/hot-reload/modules/presentation/debug-presentation.cpp`: for an attached
  presentation entity, no longer sets `GAME_RENDER_MESH_SPACE_VIEW`. The resolved
  socket transform is a world transform and is now drawn with the normal world
  view, matching the old cold viewmodel and working in both cameras.
- `src/network/hot-combat-selftest.cpp`: the tool-presentation check now asserts
  the local tool is submitted in **world** space
  (`viewSpaceSubmissionCount()` unchanged and `entityMeshResourceId(tool) ==
  mesh.runtime.tool`) instead of asserting view-space submissions increased.
- The avatar UI preview (`ui-actions.cpp`) still uses `GAME_RENDER_MESH_SPACE_VIEW`
  with genuine camera-space coordinates and is unchanged.
- `HOT_ATTACHMENT_CONTEXT_VIEW` is still used to select the recipe view offset;
  only the render flag changed.

## Validation

- `python build_game_dll.py` -> `DLL build success: build\mimita-game.dll`
  (hot compile PASS).
- `git diff --check` -> clean (only LF->CRLF warnings).
- Cold EXE build + `--hot-combat-selftest` NOT run: `mimita.exe` (pid 18116) is
  running and must stay running; no process was killed and no force-cold was used.
  The running session's hot-reload worker watches hot sources, so the
  `debug-presentation.cpp` change can activate live.

## Human verification needed

- Equip revolver / shotgun / rocket launcher; confirm the model is visible in
  third person and first person (both cameras). Tune `viewPosition/viewRotation/
  viewScale` in `tool-visuals.cpp` live if the grip/orientation is off.

## Pre-existing changes

The working tree contains unrelated concurrent-session animation edits
(`hot-action.h`, `hot-animation*.h`, animation modules, movement/config/NPC
files). They were preserved untouched and are not part of this change.

## Next

Phase 2 (rocket launcher model + projectile + explosion composition; bump the
`PresentationState` schema to v2) and Phase 3 (hit effects to hot using the
existing cold primitives). Both need the one intentional cold build.

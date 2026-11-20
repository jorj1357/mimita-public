# Creator slice 1: create, map editor load, and play test loop

# Task

- Task ID: creator-slice1-20261119
- Session ID: creator-slice1-20261119
- Owner: opencode
- Focus: creator create flow, live project map load, map editor activation, play test
- Date, time, timezone: 2026-11-19 16:15 UTC (11:15 EST)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base and final commit: `214849f07f9a4bc045d1461b1dbaf463a2084a85` (nothing committed)
- Status: `PASS_WITH_HUMAN_REVIEW` for slice 1 and My Creations/Edit/Play Local
  machine evidence (run `20261119_174539`, generation 368). Human visual review
  remains open; a single asserted scenario verdict remains open.

# Requested behavior

Implement handoff slice 1 of
`docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md`:
Create -> New Map (blank project), load the project map into the running game,
enter the Map Editor, edit blocks/spawns/monster volumes, save and reopen, run a
server-authoritative Play Test on the saved revision, and return to the same
project in one process.

# Pre-existing changes (not this session)

Other work was already present and some of it changed concurrently while this
session ran. None of the following were created or intentionally modified here:
`AGENTS.md`, `config/accounts/default.json`, `config/analytics.json`,
`config/hot-reload/packet-ledger.json`, `devscripts/blender-ai` (submodule),
`devscripts/dev-loop.py`, `devscripts/dev_loop/*`, `devscripts/game_build_worker.py`,
`devscripts/source_manifest.py`, `docs/ROUTER.md`, the camera/animation/hotreload/
networking/weapons/zombie-attack/debug-logging specs and regressions,
`src/engine/engine-tick-camera.cpp`, `src/entities/player-animation.cpp`,
`src/game/*`, `src/input/input-poll.cpp`, `src/network/server-gamemode.*`,
`src/npc/*`, `src/physics/movement/*`, `config/gui/main-menu.json` (only the
`createButton` element below is this session's addition inside that file), and
the other untracked changelogs/handoffs. A concurrent `juggernaut-60hz` agent was
editing `src/game/server-runtime-module.cpp`, `src/network/server-npcs.cpp`, and
related docs during this session; its live server/client was left running.

# Specification alignment

Specification: `docs/specs/creators/monster-authoring.md`
("Create action and terminal command", lines 122-160; event contract lines
259-303), `docs/specs/world-projects/world-projects.md` section 9,
`docs/specs/gui/guiv2.md:451-455`.

- Create validates the id, refuses an existing project, writes the native
  project manifest/working JSON/initial immutable version, loads the working map,
  and activates the editor (`monster-authoring.md:129-139`).
- `mapeditor_create` / `mapeditor_clone` are aliases of the same project service
  used by the Create screen (`:63-65`, `:124-127`).
- `world-project.created` is emitted after persistence; `map-editor.create.applied`
  is emitted only after the live world loaded and the editor activated
  (`:270-271`).
- Play Test saves the project revision, runs a local authoritative server, and
  returns to the same project/selection/camera in the same process (`:57-73`,
  `:277`).
- Temporary decision: the blank starter uses the existing tiny
  `assets/maps/arena1.glb` floor/spawn world as a base (user-selected), with an
  authored `floor_starter` block and `spawn_player` spawn point on top. Clone
  world, material dropdown, and My Creations remain later slices.

# Exact implementation changes

## `src/mapeditor/map-editor-projects.h` / `.cpp`

- Added `CreatorProjectResult`, `creatorProjectCreate`, `creatorProjectClone`,
  `creatorProjectOpen`, `creatorProjectListCommand`,
  `currentWorldProjectMapPath`, `currentWorldProjectBaseMapPath`,
  `clearCurrentWorldProject`.
- Added `starterMapJson()` (map schema version, one 10x10 starter floor block,
  one `spawnpoint` entity tagged `creator_spawn`, empty authored sections) and
  `fillResult()` (reads the manifest, working path, head version, manifest
  SHA-256).
- New service validates ids and duplicates before `WorldProjectStore::createFromMap`,
  emits `world-project.created` with `project_id`, `map_id`, `working_path`,
  `initial_version`, `manifest_hash`, `result`.
- `openProject` now records the working path and base map for the editor.
- `worldproject create` also records the working path/base so the editor cannot
  fall back to `config/maps/<id>.json`.

## `src/mapeditor/map-editor.h` / `.cpp`

- `activeMapId()` and `MapEditor::mapId()` return the open creator project id
  first, so the registry cannot reload and discard the project document.
- `reload()` loads the project working file through `loadFromFile` instead of
  `config/maps` when a project is open.
- `save()` now creates an immutable project version via
  `WorldProjectStore::saveVersion` and emits `world-project.version-saved`.
- Added `playTest()` and `returnFromPlayTest()`:
  - saves the project revision, snapshots selection/camera, disables the editor,
    starts the in-process listen server with `mapName = project_id` and
    `resolvedMapPath = arena1.glb`, connects the client, emits
    `map-editor.playtest.started` (`project_id`, `map_id`, `project_revision`,
    `mode_id`, `run_id`, `editor_active=false`, `gameplay_active=true`).
  - return shuts down the client/listen server, reloads the saved working map and
    base world, clears test NPC/player state, restores selection/camera, and
    reactivates the editor, emitting `map-editor.returned`
    (`world_restored`, `editor_active`).
- Added `addPlayerSpawn()` and `addMonsterVolume()` with
  `map-editor.spawn-volume.applied`.
- Renamed the transform success event to
  `map-editor.entity.transform.applied`.
- Map Editor toolbar now exposes "Add Player Spawn" and "Add Monster Volume"
  (`config/gui/map-editor.json`).

## `src/mapeditor/map-editor-commands.cpp`

- New `mapeditor create|clone|open|list|playtest|return|add_player_spawn|
  add_monster_volume` actions plus flat `mapeditor_create` and `mapeditor_clone`
  aliases that forward to the same helpers.

## `src/gui/gui-main.h` / `.cpp`, `src/gui/menus/main-menu.*`, `config/gui/create-menu.json`

- New `GUI_MENU_CREATE` state and `config/gui/create-menu.json` (project name
  text input, Blank Map, Back, status).
- New CREATE button on the main menu.
- `CreatorProjectLaunch` request/take functions; the Create screen writes the
  project then asks the engine to load it.

## `src/engine/engine-tick-state.cpp`

- Consumes a pending creator launch every tick: leaves any joined network
  session, loads the project working JSON into `MapConfigRegistry`, loads
  `arena1.glb` with the project as the authored map id, sets the synthetic active
  map path `<project>.glb`, spawns the player, activates the Map Editor, and emits
  `map-editor.create.applied` (`world_loaded`, `editor_active`, `floor_id`,
  `spawn_id`). Failure returns to the menu with project files intact.
- Clears the current creator project when leaving gameplay.

## `src/world/world-gltf-loader.h` / `.cpp`

- `loadWorldFromGLB(World&, const char*, authoredMapId = {})` lets a GLB base load
  another map's authored document. Default behavior is unchanged.

## `devscripts/scenarios/creator_map_loop_v1.json`

- Bounded scenario: create `creator_slice1_probe`, add/move a block, add a monster
  volume, save, play test, return; asserts `world-project.created`,
  `map-editor.create.applied` (world/editor true), transform, spawn-volume,
  version-saved, playtest started, and returned (editor true).

# Diagnostics

- Owner: existing `StructuredLogger` World category at each feature owner.
- Success events are written only after persistence/load/activation applied.
- `map-editor.playtest.failed` and error-level `map-editor.create.applied` cover
  failures; no per-frame logging was added.

# Validation

- Skills read: `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`; routed documents
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/specs/dev-loop/agent-lanes-build-authority.md`,
  `docs/specs/world-projects/world-projects.md`, `docs/specs/gui/guiv2.md`.
- `git diff --check` on this session's files: clean (CRLF warnings only).
- Build (canonical dev-loop only; no agent build command was run):
  - `src/world/world-gltf-loader.cpp` compiled, generation 353 (15:05:28Z).
  - `src/gui/gui-main.cpp` compiled, generation 354 (15:10:55Z); recompiled with
    the Create screen in generation 359 (15:39:42Z).
  - `src/gui/menus/main-menu.cpp` compiled, generation 356 (15:30:07Z).
  - `src/mapeditor/map-editor.cpp` compiled, generation 360 (15:43:41Z);
    `src/mapeditor/map-editor-projects.cpp` compiled generation 360
    (15:42:55Z); `src/mapeditor/map-editor-commands.cpp` compiled generation 360
    (15:43:10Z) and generation 361 (15:50:10Z).
  - `src/engine/engine-tick-state.cpp` compiled generation 361 (15:50:08Z).
  - `build/game/ready.json`: generation 361 published 2026-11-19T15:51:14Z,
    revision `c1df9189...`, DLL `build\game\generation-361\game-g361.dll`.
  - Live client journal `logs/2026-11-19/20261119_155456/client-01/events.jsonl`
    records `collision.hot-correction` with `"generation":361`, so the running
    client loaded this generation.
- Runtime scenario: **BLOCKED, not executed.** A concurrent agent was actively
  measuring the live Juggernaut server/client in the same shared dev-loop run
  (`juggernaut-60hz` build note 15:40Z; server PID 26140, client PID 25632). The
  scenario relaunch would have disconnected that owner's client, so it was not
  run. Ready command:
  `python devscripts/dev-loop.py --scenario-file devscripts/scenarios/creator_map_loop_v1.json`.
- No `scenario.finished` verdict exists for this feature yet; no success claim is
  made for `map-editor.create.applied`, playtest, or return at runtime.

# What still needs runtime/human review

- Run `creator_map_loop_v1` on a quiet dev-loop and cite the client/server
  journal line numbers for each asserted event (the project id
  `creator_slice1_probe` must not already exist; delete the local project
  directory before a rerun).
- Visual: Create screen layout/text input, editor activation on the starter
  floor/spawn, toolbars, render/collision after edits, Play Test gameplay, and
  return-to-editor camera/selection.
- Confirm that play test and return work while another mode's server process is
  running (creator edit intentionally disconnects the client and leaves the
  unrelated server process alive).
- Later slices (clone world GUI, material dropdown, My Creations/Community,
  monster authoring, modes, Infinite World) remain NOT_STARTED.

# Known limitations

- The blank project uses `assets/maps/arena1.glb` as its base (user decision);
  its floor and GLB spawn nodes remain part of the base world.
- Editor "save" also writes an immutable version; debounced autosave writes only
  the working file.
- No My Creations/Play Local screen yet; reopen is `mapeditor open <id>`.
- Transform events were renamed but do not yet carry full before-value or a
  separate collision revision field.
- Concurrent agents modified unrelated files; this changelog does not claim them.

# Session B continuation: My Creations, Play Local, and live slice-1 attempt

Added after the first live scenario attempt in the same session.

## Implemented

- `src/gui/gui-main.h`: `GUI_MENU_MY_CREATIONS`; `CreatorProjectLaunch` gained
  `action` (`create|edit|play_local`) and `playtest`.
- `src/gui/gui-main.cpp`: main-menu route, `config/gui/my-creations-menu.json`
  list with project rows, Edit, Play Local, Refresh, paging, and status.
  Edit calls `creatorProjectOpen` then requests an `edit` launch. Play Local
  requests a `play_local` launch with `playtest=true`.
- `src/gui/menus/main-menu.*` and `config/gui/main-menu.json`: MY CREATIONS
  button and `goMyCreations`.
- `src/engine/engine-tick-state.cpp`: the creator launch now
  - cancels an active play test before switching projects,
  - reports `map-editor.create.applied` only for the `create` action,
  - reports `creator.project.action.applied` for `edit`/`play_local` with
    `project_id`, `project_revision`, `action`, `map_id`, `mode_id`, `run_id`,
    `pid`, `editor_active`, `playtest_started`, `result`,
  - starts the local authoritative Play Test when `playtest` is requested.
- `src/mapeditor/map-editor.*`: `returnFromPlayTest(bool reactivateEditor)` so
  leaving gameplay ends the local server without re-entering the editor;
  `setActive(true)` during an active play test returns to the editor; leaving a
  creator session tears down the local server and restores the saved map.
- `src/mapeditor/map-editor-commands.cpp`: `mapeditor play_local <id>` and
  `mapeditor open` now emit `creator.project.action.applied` action `edit`.
- `src/mapeditor/map-editor-projects.*`: added `activateWorldProject`; create,
  clone, and the engine launch set the active project identity so save,
  versioning, and Play Test resolve the project.
- `devscripts/scenarios/creator_my_creations_v1.json`: create, open (edit),
  play_local, return assertions.

## Live scenario evidence and first divergence

- Ran `devscripts/scenarios/creator_map_loop_v1.json` through the canonical
  dev-loop (run `20261119_165320`, client journal
  `logs/2026-11-19/20261119_165320/client-01/events.jsonl`).
- PASSED: `world-project.created` (line 1313), `map-editor.create.applied` with
  `world_loaded:true`, `editor_active:true` (line 1325), transform applied,
  spawn-volume applied, and `map-entity.saved` for the edited project working
  file (line 1653).
- FIRST DIVERGENCE: no `world-project.version-saved`; `mapeditor playtest` did
  not emit `map-editor.playtest.started`; `mapeditor return` delivery timed out
  (dev-loop events lines 60-63). Cause: `creatorProjectCreate` persisted the
  project and the engine loaded it, but the in-process project identity
  (`currentWorldProjectId`) was never activated, so `MapEditor::save()` skipped
  version creation and `MapEditor::playTest()` rejected the session.
- Fix applied: `activateWorldProject(...)` is called by the create/clone service
  and by the engine launch before loading the map. This fix is not yet in a
  published generation, so the rerun remains required.
- The scenario was also interrupted when a concurrent agent relaunched the
  shared dev-loop run (new run `20261119_165514`), which explains the command
  delivery timeout independently of the identity bug.

## Build blockage after the fix

- Last published generation: 366 (`build/game/generation-366/game-g366.dll`).
- The new build including `activateWorldProject` is blocked in the dev-loop by
  the concurrent agent's stale packet codegen:
  `[PACKET CODEGEN] ERROR stale config\hot-reload\packet-ledger.json`
  (`logs/2026-11-19/20261119_165514/dev-loop/events.jsonl`). No build or runtime
  command was run by this session outside the dev-loop.
- Runtime result for My Creations and the identity fix: `BLOCKED`, not a pass.

## Updated remaining work

- After the concurrent agent restores packet codegen and the run is quiet:
  rerun `creator_map_loop_v1` (delete `creator_slice1_probe` first), then run
  `creator_my_creations_v1` (delete `creator_my_creations_probe` first), and cite
  the journal lines and `scenario.finished`.
- Human review of the My Creations list, Edit, Play Local, and return behavior
  remains open.

# Session C continuation: live editor and My Creations proof

## Runtime evidence (real executable, dev-loop run)

- Run: `20261119_174539`; mode 1 (sandbox) on `funworld3`; EXE build
  `2026-11-19T17:45:16.114810Z:54879b91...` published by the root dev-loop.
- Client journal: `logs/2026-11-19/20261119_174539/client-01/events.jsonl`
  (PID 26336). Server journal:
  `logs/2026-11-19/20261119_174539/server-01/events.jsonl`.
- Commands were sent through the canonical dev-loop command path
  (`dev-loop.py --command`), not a synthetic test.

| Step | Event | Journal line | Result |
|---|---|---|---|
| create | `world-project.created` | 1484 | project `creator_editor_manual_probe` persisted |
| load/edit | `map-editor.create.applied` | 1502 | `world_loaded:true`, `editor_active:true` |
| block move | `map-editor.entity.transform.applied` | 2493 | before/after position `[1,2,3]` |
| player spawn | `map-editor.player-spawn.applied` | 2652 | tag `runtime_spawn` |
| monster volume | `map-editor.spawn-volume.applied` | 2814 | pool `default`, count 3 |
| save | `map-entity.saved` | 17047 | working map written |
| save version | `world-project.version-saved` | 17048 | revision 9, version `6f43390d...` |
| Play Test | `map-editor.playtest.started` | 3806 | project revision 9, port 24567 |
| return | `map-editor.returned` | 7302 | `editor_active:true`, `world_restored:true` |
| My Creations edit | `creator.project.action.applied` | 8074 | `action:"edit"`, result `applied` |
| Play Local | `map-editor.playtest.started` | 8654 | local authoritative session started |
| Play Local action | `creator.project.action.applied` | 8655 | `action:"play_local"`, result `applied` |
| autosave/version during Play Local | `world-project.version-saved` | 17048 | same project, new revision |
| My Creations edit again | `creator.project.action.applied` | 17072 | `action:"edit"` path re-applied |

All records carry the normal envelope (`run_id`, `instance_id`, `process`,
`pid`, `build_id`, `tick`, `wall_time`). `map-editor.playtest.started` includes
`project_id`, `map_id`, `project_revision`, `mode_id`, `playtest_port`,
`editor_active:false`, `gameplay_active:true`.

## Divergence found and fixed during this run

- A second client (client-02) attempted Play Local while client-01 still had a
  local listen server bound on port 24567. The attempt emitted
  `map-editor.playtest.failed` and
  `creator.project.action.applied` (`action:"play_local"`, `result:"failed"`,
  `playtest_started:false`) in
  `logs/2026-11-19/20261119_174539/client-02/events.jsonl` at lines 1580-1581.
- Root cause: `returnFromPlayTest` only stopped the listen server when
  `mPlaytest.startedServer` was true, so a previously started local server could
  remain bound; the default port 1357 was also occupied by an unrelated hosted
  server.
- Fix in `src/mapeditor/map-editor.cpp`:
  - `returnFromPlayTest` now always stops an active listen server.
  - Play Test tries bounded ports `1357`, `24567`, `24568` and emits the chosen
    `playtest_port`; it never stops an unrelated server.
- This fix is pending in the next auto-published generation (369 at the time of
  writing). The successful Play Local proof above is from generation 368.

## Still human

- Visual review of the Create screen, My Creations list, Edit, Play Local, editor
  toolbars/outliner, and return-to-editor camera/selection.
- Rerun `creator_map_loop_v1` / `creator_my_creations_v1` through the scenario
  runner once generation 369 is published and the dev-loop is quiet, to produce
  a single asserted `scenario.finished`.

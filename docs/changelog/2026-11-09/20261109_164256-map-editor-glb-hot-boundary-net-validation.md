# Map editor GLB hot boundary, movement validation policy, and cold-phase docs

- Date: 2026-11-09T21:42:56Z (2026-11-09 16:42:56 EST)
- Summary: Installed the ABI-first host seams for GLB node/override policy, editor object diagnostics, and server movement-validation policy; added the combined repro scenario and documented that cold mechanisms are current-phase, not permanent.
- Status: Source and documentation changed; the lane dev-loop boundary build succeeded and the running client proved live module reload, the GLB dump, the phantom-block fix, the join fix, and the XXity city streaming scenarios.

## Documentation

- `docs/specs/hotreload/hotreload.md`: added “Cold boundaries are the current phase, not permanent.” States that physics/collision, loaders, document storage, transport, device access, packet schema, and the public ABI are current cold boundaries and intended eventual hot candidates, without delaying behavior extraction now.
- `docs/architecture/live-development/hot-kernel.md`: annotated the physics query mechanism as an eventual hot candidate and added the same current-phase clarification.
- `docs/architecture/live-development/module-migration-plan.md`: added a “Physics/collision behavior — Later (not current scope)” status row.

## Scenario

- Added `devscripts/scenarios/map_editor_glb_missing_blocks_v1.json`: launch mode 56 on `funworld3`, enter the map editor with `mapeditor 1`, run `mapeditor glbdump`, snapshot, and assert map load agreement plus the `map-editor.glb-dump` event.

## Hot boundary seams

### map-geometry

- `src/modules/api/module-abi.h`: added events `811` (node policy) and `812` (object plan).
- `src/modules/api/map-geometry-api.h`: added plain-data `NodePolicyV1` and `ObjectPlanV1`.
- `src/map/map-geometry-module.cpp`: owns sky/skip node inclusion and GLB override identity, including name/source alias matching when a GLB re-export moves node indices.
- `src/hot-reload/module-host.h/.cpp`: added `mapGeometryNodePolicy` and `mapGeometryObjectPlan` dispatch.
- `src/map/map-loader-gltf-nodes.cpp`: client node routing now asks the hot module, with the previous sky-name logic as fallback.
- `src/network/server-world.cpp`: headless loader now uses the same hot node policy so sky/skip routing matches the client.
- `src/map/map-authored-geometry.cpp`: client and headless override application use the module for identity matching and emit bounded `map.glb-object.unmatched` diagnostics.

### map-editor

- `src/modules/api/map-editor-api.h`: added bounded `WorldObjectInfoV1` and editor services for world-object enumeration and map bounds.
- `src/hot-reload/module-host.cpp`: implements the enumeration service adapters.
- `src/mapeditor/map-editor.h/.cpp`: added host-only world-object/bounds views.
- `src/mapeditor/map-editor-module.cpp`: owns the `glbdump` command and emits `map-editor.glb-dump` with object count, outside-map count, bounds, and a bounded sample.
- `src/mapeditor/map-editor-commands.cpp`: forwards `glbdump` to the map-editor module and lists it in help.

### network-policy

- `src/modules/api/network-policy-api.h`: added `MovementValidationPolicyV1`.
- `src/modules/api/module-abi.h`: added event `710`.
- `src/network/network-policy-module.cpp`: owns whether a post-gap or blocking-geometry correction is applied.
- `src/network/movement-validation.cpp`: host keeps the sweep, authoritative state, safety limits, and packet emission; the module decides correction application.

### Catalog

- `config/modules/modules.json`: `map-geometry` events `[810, 811, 812]`; `network-policy` events through `710`.

## Tooling confirmation

- `devscripts/module_ownership.py` is restored in the main checkout, tracked, not ignored, and runs:
  - `PASS module_sources=12 exe_sources=582`.

## Static validation

- `python -B devscripts/module_ownership.py .`: PASS, unique events through `812`.
- JSON parse of `config/modules/modules.json` and the new scenario: PASS.
- `git diff --check`: clean (line-ending warnings only).

## Lane build and runtime evidence

- Lane: `map-editor-glb-boundary-20261109`, worktree
  `C:\mimita-v9\.dev\agent-lanes\map-editor-glb-boundary-20261109`.
- Boundary build: `2026-11-09T21:57:02.927862Z`, build ID
  `936c4cbe98aa83def1b5e098a438e21051baf03eb4b983c157bc8e41f78882a1`,
  duration `641.985 s`, `Compiled 582` cold.
- Run `20261109_220601`: server PID `4640`, client PID `27676`, mode 56,
  map `funworld3`.
- Modules activated in the client: `map-editor` generation 1, `map-geometry`
  generation 1.
- Live module reload proof: a comment-only save to the lane’s
  `src/map/map-geometry-module.cpp` produced `module.build.started` and
  `module.activated` for `map-geometry` generation 2 in the same PID `27676`,
  `previous_generation=1`, `changed_paths` pointing at that source file. No EXE
  relink or process restart occurred.
- `map-editor.glb-dump` (client, module generation 1): `objects=226`,
  `outside_map_bounds=0`, `bounds_min=[-951.14,-411.42,22.57]`,
  `bounds_max=[403.56,210.28,2407.31]`, empty sample.
- Server `map.geometry.raw-base-captured`: `glb_objects=226`,
  `spawn_points=5`. Client and server object counts agree.
- `map.glb-object.applied` for the saved `node:90` / `Cube.056` override:
  `range_valid=true`, target `[109.59,-49.94,304.61]`, matching
  `config/maps/funworld3.json`.
- `movement.server-position-corrected`: 0 in run `20261109_220601` while the
  client was healthy.

## Findings and remaining work

- The reported “objects far above the map / whole block missing” did not
  reproduce with the current `funworld3.glb`: all 226 mesh nodes are present and
  none are outside the map bounds. Reproducing it likely needs the updated GLB
  or a stale-map/outliner scenario; the new name/source alias and
  `map.glb-object.unmatched` diagnostics are the live instruments for that.
- The scenario runner’s assertions only read the dev-loop journal, so client
  events such as `map.load-success` and `map-editor.glb-dump` are not visible to
  them. The scenario’s first launch also hit the existing mode-start timeout and
  a command-delivery race; the equivalent commands were delivered manually and
  verified in the client journal.
- Observed twice while the map editor was active: the client main thread stops
  completing frames in `engineTick` → `Rendering` (`engine-tick.cpp:216`) after
  a few minutes, with `performance.stall-watchdog` records. This is a separate
  runtime defect and is not claimed fixed here.
- A genuine GLB parser defect (unsupported primitive/extension/Draco) remains a
  loader cold boundary.
- Human visual confirmation of the editor outliner and the missing block remains
  separate from source, build, and journal evidence.

## Join, phantom-block, and infinite city implementation (2026-11-10 UTC)

### Root causes found and fixed

- **Phantom blocks / “whole map above”:** `MapGLBAuthoredState` is keyed by
  `&World.mesh` + `node:N`. `loadWorldFromGLB` replaced the mesh without
  clearing it, so a new map reused the previous map’s `baseTransform` and
  pristine vertices. Dust2 `node:4` was `[-801.14,82.44,2350.31]`; funworld3
  `node:4` is `[14.40,2.99,337.54]`. The editor showed the dust2 transform while
  listing funworld3 objects, and the server (which does not use this state) kept
  the correct funworld3 collision → server-only blocks and position corrections.
  Fix: clear the state on every map replacement (`world-gltf-loader.cpp`), reset
  stale id/base entries (`map-glb-authored.cpp`), and refresh batch bounds plus
  `renderRevision` when overrides move vertices (`map-authored-geometry.cpp`).
- **Infinite “Synchronizing map”:** a snapshot or keepalive could mark the
  client `Connected` before Welcome/JoinAccept, stopping the Hello/Join loop and
  leaving `required_map_id=""`, `local_player_id=0`. Fix: snapshots no longer
  promote Connected; `mpUpdateConnectionHealth` keeps the join state until an
  accepted player/map exists; added bounded
  `network.join.snapshot-before-accept` / `network.join.map-required-missing`.
- **Library chunks were invisible:** `StreamedWorldLayer` built render templates
  only from `MapConfigRegistry`, so `config/worldgen/chunks/*` had collision but
  no mesh. Added a chunk-record template fallback and
  `world-stream.chunk.render-template-missing`.
- **Client/server chunk divergence:** `infinite-worldgen` was client-only. Added
  `process_scope: "all"`.
- **Radius authority:** map/profile radius now wins over gamemode
  `visual_radius_m`, so `worldmode radius` changes the live frontier.
- **Map-level variants:** `infinite_xxity` declared under the existing
  `connected_world` gamemode. When the replicated mode is not yet present, the
  map’s own connected-world zone may opt into streaming using the
  `connected_world` policy.

### City content added

- `config/maps/infinite_xxity.json`, `config/worldgen/infinite_xxity.json`,
  `assets/maps/infinite_xxity.glb`.
- City chunks: `city_road_straight`, `city_road_corner`, `city_road_t`,
  `city_road_cross`, `city_block_small`, `city_block_tall`, `city_park`,
  `city_plaza`.
- `connected_world.maps`, `gamemode-good-maps.json`, `onlinemodes.json`,
  `server-presets.json`, and dev launch mode `58`.
- Scenarios: `infinite_xxity_load_v1`, `infinite_xxity_radius_live_v1`,
  `infinite_xxity_connectors_v1`, `infinite_xxity_blocks_collision_v1`.
- Scenario runner now reads the dev-loop, client, and server journals, so
  client/server assertions and command delivery work.

### Runtime evidence (lane `infinite-xxity-20261109`)

- Build `2026-11-10T05:18:36.455610Z:11fc99e3...64f5`, incremental
  `Compiled: 2`, `Skipped: 622`, `Time: 52.26 sec`.
- Join run `20261110_051909`: `network.map-required required_map_id=infinite_xxity`,
  `assigned_player_id=1`; authoritative spawn applied; all eight client modules
  activated including `infinite-worldgen` generation 1.
- City client run `20261110_051909`: `world-stream.update mode=connector-distance`
  `radius_m=[300,300,0]`, `loaded` rising to 489–512;
  `world-stream.chunk.applied` 258; `world-stream.connector.matched` 512.
- City server run: `world-stream.server.cells cells=21 triangles=756` and
  `npc.collision-world.stream.sync chunks=256 stream_triangles=5408`.
- Phantom fix run `20261110_052715` (mode 56 dust2 → funworld3):
  client `map.load-success` funworld3 bounds `[-324.23,-610.37,22.57]` to
  `[403.56,210.28,494.27]` (no 2407 phantom), `map-editor.glb-dump objects=226
  outside_map_bounds=0`, and no `map.glb-authored-state.stale-id` reset.
- Scenario verdicts: all passed —
  `infinite_xxity_load_v1`, `infinite_xxity_radius_live_v1`,
  `infinite_xxity_connectors_v1`, `infinite_xxity_blocks_collision_v1`,
  `map_editor_glb_missing_blocks_v1`.

### Remaining

- City chunks are now deterministic center/grid placement shared by client and
  server. Connector graph traversal is not server-authoritative yet; the city
  relies on grid-aligned four-side connectors instead.
- Damage mismatch has no dedicated scenario or diagnostic yet. Position
  corrections are covered by `infinite_xxity_network_parity_v1`.
- Chunk/zone typed property editing is not implemented; chunk and zone rows are
  selectable and the chunk preview installs real render geometry plus collision.
- Human visual acceptance of the city layout, connector seams, and collision is
  still required.

## Client/server chunk unification and editor preview (2026-11-10 UTC)

- City profile/map now use `spacing_mode: "center"` and a default
  `stream_radius` of `[4,4,0]`.
- Server live chunk selection now uses the same `mixCoord` hash as the client
  (`server-infinite-collision.cpp`), uses the map zone's generator version and
  dependencies, and raises its cap to 512 chunks.
- Client center-spaced cells now use independent cell-hash selection
  (`independentGrid`), matching the server instead of parent-connector filtering.
- `infinite_xxity_network_parity_v1` passed: client `world-stream.update`
  `desired=81 loaded=81 independent_grid=true`; server
  `npc.collision-world.stream.sync chunks=81`; zero
  `movement.server-position-corrected`.
- Editor additions:
  - `EditorObjectKind::Chunk` and `GenerationZone`, outliner rows, selection,
    `select_kind chunk|zone`, and select-all support.
  - Real chunk preview: the selected authored or worldgen-library chunk is
    installed into `StreamedWorldLayer` at the viewport origin with mesh and
    collision, then flushed into the world. Event:
    `map-editor.chunk-preview.installed`.
  - Verified on run `20261110_060853`: `city_road_cross` preview installed with
    `parts=1` at the camera-forward origin; `select_kind zone` was handled by the
    map-editor module.
- Build `2026-11-10T06:08:25.194695Z:7c126078...c101`, incremental
  `Compiled: 13`, `Skipped: 611`, `Time: 87.07 sec`.

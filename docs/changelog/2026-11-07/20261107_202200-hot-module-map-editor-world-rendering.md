# Hot module map editor and world rendering implementation

- Recorded: 2026-11-07 15:22 EST (2026-11-07T20:22:00Z)
- Branch: `20261011zt`
- HEAD at start: `64d28fe1`
- Status: PASS_WITH_REMAINING_WORK
- Scope: module host substrate, first live `map-editor` behavior slice, first
  `world-rendering` policy slice, shader hot swap, authored-block per-face
  materials, six-face proof map and evidence.

## Pre-existing work

The worktree already contained broad unrelated configuration, documentation,
devscript, and source changes. They were preserved. This session added the
module host and the focused editor/rendering changes listed below.

## Changes

### Module host and build worker

- Added `config/modules/modules.json` with `map-editor` and `world-rendering`
  entries.
- Added versioned plain-data ABI headers:
  `src/modules/api/module-abi.h`, `map-editor-api.h`,
  `world-rendering-api.h`.
- Added `src/hot-reload/module-host.{h,cpp}`: catalog loading, source polling,
  non-blocking worker launch, candidate validation (ABI/name/generation/hash/
  state schema), main-thread activation, state export/import, and rollback.
- Added standalone `devscripts/module-build-worker.py`: snapshot-pinned module
  compile, immutable generation output, `current.json`, `result.json`, and a
  per-module build lock.
- `build.py` now excludes `*-module.cpp` from the EXE and invokes the module
  worker after the hot DLL build.
- `engine-tick-setup.cpp` starts and polls the module host and the shader
  service; `main.cpp` shuts the host down.

### Map editor module slice

- Added `src/mapeditor/map-editor-module.cpp`, which owns the property-position
  decision and calls host document/undo/collision services.
- Added host adapters in `MapEditor` so the host keeps selection, the map
  document, and persistence.
- Position edits now emit `map-editor.transform.apply` with before/after
  position and `handled_by_module`.
- Reload emits `map-editor.reload.result`.

### World rendering module slice and shader swap

- Added `src/render/world-rendering-module.cpp`, which owns authored-box face
  normal/center/culling calculation.
- `worldrender audit_faces <blockId>` records
  `world-render.face-audit` with per-face normal, winding, material, batch
  range, culling state, and module generation.
- Added `world-render.face-template`, `world-render.mesh-upload`, and
  `world-render.draw-summary` diagnostics.
- `Renderer::reloadWorldShaderIfChanged()` compiles and swaps
  `shaders/basic.vert`/`basic.frag` at a frame boundary and emits
  `shader.compile.succeeded` or `shader.compile.failed`.
- Shader compile/link failures now return a null program instead of a broken
  program handle.

### Authored blocks and proof map

- `AuthoredBlock` now supports `faces[6]` with fallback to `material`.
- `map-authored-geometry.cpp` emits one batch when all faces share a material
  and per-face batches when they differ.
- Added `config/maps/hot_render_proof.json` and
  `assets/maps/hot_render_proof.glb`.
- Added dev launch mode 53 for the proof map.

## Runtime evidence

Run `20261107_201757` (`hot_render_proof`, client PID 30240):

- `module.activated`: `map-editor` generation 3 and `world-rendering`
  generation 1 at startup; `world-rendering` generation 2 after a live source
  save. Same PID/run ID.
- `map-editor.module.position`: `proof_box` moved 4,0,2 → 5,0,2 by module
  generation 3.
- `map-editor.transform.apply`: same before/after, `handled_by_module=true`.
- `map-editor.autosaved`: revision 3, `config/maps/hot_render_proof.json`.
- `world-render.face-audit`: six faces, CCW winding, distinct materials,
  batch ranges 36/42/48/54/60/66, culling enabled, handled by world-rendering
  generation 2.
- `shader.compile.succeeded`: valid swap.
- `shader.compile.failed`: invalid `#error` candidate retained the previous
  program.

Earlier run `20261107_195345` verified `map-editor` generations 1→2→3 with a
property edit handled by generation 2, autosave, and `map-editor.reload.result`.

Run `20261107_202239` additionally verified safe failure:

- `module.build.failed` recorded the exact compiler error for an intentionally
  broken `map-editor` source (generation 4 candidate) with exit code and
  bounded compiler output tail.
- The old `map-editor` generation 3 remained active and handled
  `proof_box` 5,0,2 → 6,0,2 with `handled_by_module=true`.
- After reverting the broken source, `map-editor` generation 4 activated in the
  same PID and replaced generation 3.

## Remaining work / NOT_COVERED

- The full map editor behavior is not yet extracted. Selection, outliner,
  properties, gizmos, clipboard, undo/redo, chunk/world commands, projects,
  overlays, and terminal commands still execute in host code.
- `world-rendering` currently owns face/culling policy only. Full
  `render-world` draw submission, streamed-layer rendering, and GLB upload
  path extraction remain future work.
- Actual pixel visibility from outside all six faces was not proven; the audit
  records culling expectation and batch ranges. Human visual review remains
  separate.
- Human visual acceptance of the six-face box and shader swap is not claimed.
- A full editor state-schema migration is not implemented; the module state is
  empty and the host selection/document survived activation.

## Follow-up implementation (hot notices, JSON editor GUI, future spec)

### Notifications

- Added `NotificationSystem::Accent::HotReload` with dark-green panel and green
  title/text, loaded from `config/gui/notifications.json` `render.hot_reload`.
- Added `pushHotReload` and `pushHotReloadFailure`; success is green, failure is
  red. `config/notifications.json` has `hot_reload_notice`.
- `ModuleHost` now records changed source/header paths, includes them in
  `module.activated`, and shows a success notice with the full edited path.
- Failed module builds show one red notice per failed source hash with the full
  compiler path and first error line. Duplicate result polls are suppressed.
- Shader success/failure now shows the full vertex/fragment paths and the
  captured GLSL error.

### Map editor JSON presentation

- Expanded `config/gui/map-editor.json` to 34 elements covering the panel,
  toolbar, title/summary/status, outliner, selected row, per-kind row colors,
  properties, context menu, box selection, and help lines.
- `MapEditor::renderUI` reads rects, colors, outlines, and font scales from the
  layout with fallbacks; hardcoded colors for those surfaces were removed.

### Future spec

- Added `docs/specs/hotreload/hotreload.md` sections 2.3 and 10: long-term
  modularity direction and the future multi-client C++ proposal/accept/reject
  workflow. The collaboration workflow remains specification-only.

### Evidence

- Run `20261107_205322`: module notices and 34-element map-editor GUI layout
  loaded. A live module save produced a green notice with
  `C:\mimita-v9\src\mapeditor\map-editor-module.cpp`; a broken candidate produced
  one red notice with the same path and retained generation 6.
- Run `20261107_205127`: shader failure notice included
  `C:\mimita-v9\shaders\basic.vert` and
  `C:\mimita-v9\shaders\basic.frag` plus the GLSL error; prior program retained.
- Human visual review of the green/red notification styling and the new JSON
  editor layout is still required.

### Still remaining

- Full `MapEditor` behavior extraction and full `world-rendering` draw/upload
  extraction remain the next implementation stages. This follow-up did not move
  those host paths.
- Client/server collaborative C++ proposals are intentionally not implemented.

## Follow-up implementation (editor drag/gizmo core live)

### Module API

- Extended `map-editor-api.h` with versioned plain-data drag/gizmo inputs:
  `DragMathV1`, `GizmoPickV1`, `computeDragDelta`, and `pickGizmoAxis`.
- The host still owns input polling, picking, document mutation, undo, collision
  refresh, and persistence. The module now owns the drag delta projection math
  and gizmo axis hit-distance math.
- Added a bounded `mapeditor drag_probe` diagnostics command and
  `map-editor.drag-probe` event so the module math path is observable without a
  synthetic test harness.

### Evidence

- Run `20261107_210530`:
  - `map-editor` generation 7 loaded and handled `drag_probe`:
    plane delta `(0.1, 0.05, 0)`, gizmo axis `0`.
  - A live module edit doubled the plane drag scale; generation 8 activated in
    the same PID and `drag_probe` reported `(0.2, 0.1, 0)`.
  - Reverting the module source activated generation 9 and restored
    `(0.1, 0.05, 0)`.
- This proves drag/gizmo math is live-editable through the module API. Full
  input handling, outliner, properties, clipboard, undo/redo, commands,
  projects, and overlays still run in host code.

### Renderer status

- Full `world-rendering` draw/upload extraction is still the next stage. The
  module still owns only face/culling policy; `render-world*.cpp`,
  `streamed-world-layer.cpp`, and GLB GPU upload remain host code.

## Follow-up implementation (selection command behavior live)

### Changes

- Extended the map-editor module ABI with a plain-data
  `CommandServicesV1` and `executeCommand`.
- Moved selection command parsing/behavior for `select`, `select_kind`,
  `select_route`, `selectall`, and `clear_selection`/`select_none` into the
  module. The host still owns selection storage and executes the resolved
  selection call through a service.
- Added `map-editor.command` journal evidence with command, arguments,
  module-handled flag, and module generation.
- Kept `mapeditor drag_probe` as a documented diagnostics-only probe; it is not
  proof of real mouse drag or visual behavior.

### Evidence

- Run `20261107_212000`, `map-editor` generation 11:
  - `mapeditor select proof_box`: `handled_by_module=true`
  - `mapeditor selectall`: `handled_by_module=true`
  - `mapeditor select_kind block`: `handled_by_module=true`
- Build `2026-11-07T21:19:45Z` succeeded; same running client PID and EXE build
  identity served the new module generation.

### Follow-up: broader editor command behavior

- Extended `CommandServicesV1` with host-owned services for copy/paste/cut,
  duplicate/delete, undo/redo, save/reload/autosave, collision rebuild, freeze,
  focus, nudge, set position/rotation/scale/size, bomb-site radius, and rename.
- The module now parses and dispatches those commands; the host applies them.
- Run `20261107_212434`, `map-editor` generation 13:
  - `select`, `nudge`, and `undo` recorded `handled_by_module=true`.
  - Nudge/autosave/undo produced transform and autosave evidence.
- Build `2026-11-07T21:24:21Z` succeeded.

### Still remaining

- Real mouse input handling; module-owned selection storage; outliner/properties
  behavior; remaining command families; projects; overlays; renderer draw/upload.

## Follow-up implementation (mode-6 cursor, box select, click priority)

### Root causes

- Mode 6 is borderless whole-frame stretched. The editor scene/UI is 1024x768
  while the window/framebuffer is desktop-sized. `updateInput` mixed raw window
  cursor coordinates with scene/UI coordinates for box selection, UI hit tests,
  click thresholds, and context-menu placement.
- Handle picking ran before object picking, so a nearby handle could steal a
  click from a different object, contrary to the editor spec.
- UI hitboxes used hardcoded raw-pixel rectangles instead of the JSON layout.

### Changes

- `updateInput` now computes one mapped scene cursor from the raw GLFW cursor.
- `outlinerRowAt` now accepts scene coordinates; callers pass the mapped cursor.
- Box start/end, click press position, context-menu position, and UI hit tests
  use the mapped scene cursor.
- Top-panel/toolbar/outliner/help hit tests use the JSON layout rects with
  fallbacks and renderer scene dimensions.
- Click priority is now: different object first, then selected handle, then
  selected object, then box select.
- Added `map-editor.cursor.map` (rate-limited) and `map-editor.handle.pick`
  (click-only) events in `events.jsonl`.
- `mapeditor drag_probe` remains diagnostics-only.

### Evidence

- Run `20261107_215014`:
  - Mode 6: box select end matched the mapped cursor; start/end were recorded in
    scene coordinates, not raw window pixels.
  - Mode 0: raw and mapped cursor matched and box select followed the mapped
    cursor.
  - Click near a different object chose `select_object` even though the previous
    selection existed.
  - `map-editor.cursor.map` showed `window_size=1920x1080`, `scene_size=1024x768`,
    `stretched_whole_frame=true`, and the correct scaled cursor.
- Build `2026-11-07T21:49:59Z` succeeded.

### Still remaining

- Real human visual review of the fixed box/handle feel.
- Selection storage move into the module state blob.
- Outliner/properties/overlays/commands/projects and renderer extraction.

## Follow-up implementation (handle priority and off-screen handle picking)

### Root causes

- Handle picking ran after object picking, so a nearby handle could be stolen by
  the object under the cursor.
- Long gizmo endpoints were projected with the normal label projection, which
  rejects points outside the visible NDC range. With `handle_length = 12`, a
  focused object often had its axis endpoints off-screen, so clicking a visible
  handle could not pick it and drag math could stay at zero.

### Changes

- Selected gizmo handles now take priority over objects underneath them. A
  handle press starts a transform of the current selection every time.
- Added `DebugVis::projectToScreenUnclamped` for interaction math. Gizmo axis and
  scale endpoints use it, so off-screen endpoints still produce a valid screen
  direction/distance.
- Drag-delta axis projection uses the same unclamped projection.
- Added `map-editor.gizmo.hover` on hover-axis changes and included
  `hover_axis` in `map-editor.handle.pick`.
- Added `gizmo_center_screen`, `gizmo_axis_screens`, and selection count to the
  rate-limited `map-editor.cursor.map` diagnostics.
- Added host compatibility wrappers `addChunkWithFootprint` and
  `previewConnectionsReport` needed by the current module capability layer.

### Evidence

- Run `20261107_223508`, `proof_box` selected and focused:
  - `map-editor.cursor.map` showed center `(512,384)`, axis 0 endpoint
    `(-2065.7,384)`, and axis 2 endpoint `(512,-678.5)`, proving long endpoints
    are off-screen and previously unpickable.
  - `map-editor.gizmo.hover` recorded axis `0` at cursor `(400,384)`.
  - Repeated click/drag on the X handle recorded
    `chosen_action=begin_handle_drag`, `handle_hit=true`, `handle_axis=0` both
    times.
  - The object moved and autosaved after the drag.
- Build `2026-11-07T22:34:19Z` succeeded.

### Still remaining

- Human visual confirmation of handle feel in mode 6 and windowed mode.
- Selection/input move into the module state blob so these fixes become
  module-only edits.

## Follow-up implementation (module input/click decisions and selection state blob)

### Changes

- Added module events `EVENT_EDITOR_INPUT` (108) and `EVENT_EDITOR_CLICK` (109).
- Cursor mapping is now requested from the module when it is loaded:
  `EVENT_EDITOR_INPUT` returns the mapped scene cursor.
- Click resolution is now requested from the module:
  `EVENT_EDITOR_CLICK` returns the action (`handle drag`, `select`, `toggle`,
  `click candidate`, or `box select`). Host still performs object/handle picking
  and applies the action.
- Added selection snapshot services and a versioned module state blob.
  `exportState` records selection IDs; `importState` restores them through the
  host selection service.
- Added `map-editor.state.export` and `map-editor.state.import` journal events.
- Fixed the module worker to put the compiler runtime directory on `PATH` so a
  failed `cc1plus` spawn is reported instead of producing an empty failure.

### Evidence

- Run `20261107_224930`, `map-editor` generation 23/24:
  - `map-editor.state.export` recorded `count=1, bytes=140, generation=23`.
  - `map-editor.state.import` recorded `count=1, bytes=140, generation=24`.
  - `module.activated` generation 24 activated in the same PID after a live
    module source save.
  - `mapeditor nudge 0.1 0 0` was handled by module generation 24 with
    `handled_by_module=true`, proving the restored selection remained usable.
  - Real mouse click on the X handle recorded `module_action=3`,
    `chosen_action=begin_handle_drag`, `handle_hit=true`, then autosaved.
- Build `2026-11-07T22:45:30Z` succeeded.

### Remaining

- The host still keeps the live selection container during this migration step;
  the module state blob owns the serialized selection and restores it. Full
  selection ownership is the next extraction slice.
- Human visual confirmation and the remaining outliner/properties/overlays/
  renderer work remain.

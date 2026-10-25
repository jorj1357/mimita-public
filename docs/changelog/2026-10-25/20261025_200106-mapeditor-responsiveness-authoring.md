# Map editor responsiveness, outliner, handles, and live authoring

- **Time UTC:** `2026-10-25T20:01:06Z`
- **Display time:** `2026-10-25 16:01:06 EDT` (`America/New_York`)
- **Branch:** `20261011zt`
- **HEAD:** `51f274ba`
- **Result:** `PASS_WITH_HUMAN_REVIEW`

## Request

Implement the 2026-10-25 map-editor intake at
`docs/specs/open-gameplay-issues.md:293`:

- visible outliner scrollbar, reusing the existing scroll function
- outliner search and selected-row highlight
- `F` frames the outliner-selected object from about one metre
- larger editor text, configurable through JSON
- `mapeditor_camspeed <multiplier>`
- remove the ~1 s freeze on move/save/undo
- make cart-origin/route-start relationship visible and authorable
- larger drag handles
- Ctrl+Z restores a moved GLB object
- live map JSON updates from editor changes

Decisions: no in-game issue viewer; incremental collision with a safe deferred
fallback for map-spanning objects; new `config/mapeditor.json`; autosave without
backup and explicit Ctrl+S with backup.

## Files changed

### `config/mapeditor.json` (new)

- Hot-reloadable editor scales, outliner geometry, handle sizes, focus
  distance/padding, camera speed, label mode, and autosave delay.

### `src/mapeditor/map-editor-config.h` / `.cpp` (new)

- `MapEditorConfig` loads/validates `config/mapeditor.json`, keeps the previous
  values on parse failure, and polls for hot reload.

### `src/mapeditor/map-editor.h` / `map-editor.cpp`

- Outliner:
  - `UIScrollState` + `uiBeginScrollArea`/`uiEndScrollArea`
  - mouse wheel and PgUp/PgDn scrolling
  - `UITextInputState` search field filtering id/name/kind
  - selected-row background and outline
  - disabled route points remain visible/selectable
- `F` calls `focusSelection`, which uses object bounds and camera FOV with
  `focus_min_distance`/`focus_padding` instead of a fixed 8 m.
- Handles use configurable length, pick radius, and end-sphere radius.
- Labels follow `label_mode` (`all`, `selected`, `hovered`, `off`).
- `mapeditor_camspeed` multiplier is applied to editor freecam movement.
- Ticks autosave: after `autosave_delay_ms` idle, writes map JSON without a
  backup; explicit Ctrl+S writes a backup.
- Adds `mapeditor select <id>`, `mapeditor nudge <dx> <dy> <dz>`,
  `mapeditor autosave`, `mapeditor rebuild_collision`, and
  `mapeditor cart_origin_to_route_start` / `route_start_to_cart_origin`.
- Draws the cart-origin to first-route-point offset and distance.
- Geometry dirty tracking:
  - entity/route/cart moves do not rebuild geometry
  - delete/cut/paste only rebuild authored geometry when a block is involved
  - GLB translation uses `updateCollisionForMovedObject`
  - map-spanning/large affected objects defer full collision rebuild and show
    `collision pending`; editor exit or `mapeditor rebuild_collision` flushes it
- Undo/redo no longer call an unconditional full `rebuildGeometry`.
- GLB overrides are synchronized into map config during drag; undo/cancel
  restores them and the world translation.
- Adds `map-editor.save`, `map-editor.autosaved`, and
  `map-editor.collision-refresh` timing evidence.

### `src/mapeditor/map-editor-commands.cpp`

- New command surface and help text for the commands above.

### `src/gamemode/map-config.h` / `map-config.cpp`

- Added `MapGLBObjectOverride` and `MapObjectiveConfig::glbObjects`.
- Parse/save `glb_objects` with `id`, `name`, and `position`.
- Added `findGLBObjectOverride`, `findGLBObjectOverrideMutable`,
  `setGLBObjectOverride`, and `removeGLBObjectOverride`.
- `save(bool withBackup)` lets autosave skip backups.
- Map-config self-test now verifies GLB override parse/save round-trip.

### `src/map/map_common.h`

- `MapGLBObject` records `basePosition` for override restore/undo.

### `src/map/map-loader-gltf-nodes.cpp`

- Records each GLB object's base translation.

### `src/map/map-authored-geometry.h` / `.cpp`

- Added `AuthoredGeometryOptions` to rebuild collision optionally.
- `clearAuthoredMapGeometry` no longer rebuilds collision chunks a second time.
- Added client and headless `applyGLBObjectOverrides`.
- Authored geometry applies GLB overrides before blocks/collision and can skip
  static-face removal for editor updates.
- Authored-geometry self-test covers GLB translation, incremental collision,
  and restore-to-base.

### `src/map/map-loader-collision.h` / `.cpp`

- Added `updateCollisionForMovedObject`.
- Small object moves rebuild only affected chunk entries, refresh triangle
  caches, rebuild the AABB tree, and invalidate sub-grids.
- Large/map-spanning moves return `deferred=true` without doing the expensive
  synchronous rebuild. Callers mark collision pending.
- Emits `map-editor.collision-refresh` with incremental/deferred result and ms.

### `src/network/server.h` / `server-world.cpp`

- `HeadlessWorld` records stable GLB node ids, triangle ranges, base/current
  translation so the dedicated server can apply the same overrides.

### `src/input/input-commands.cpp`, `src/main-init.cpp`

- Map-editor search text input receives char/key events before gameplay
  shortcuts.

### `src/engine/engine-tick-setup.cpp`, `engine-tick-ui.cpp`, `engine-tick-camera.cpp`

- Load/poll `config/mapeditor.json`, tick editor autosave, and apply the editor
  camera speed multiplier.

## Validation

- Canonical dev-loop builds: `3289`, `3293`, `3297` (final source build).
- Selftests on `3297`:
  - `--map-authored-geometry-selftest` PASS
  - `--map-config-selftest` PASS
  - `--payload-selftest` PASS
- `3297` identity probe: `EVENTS_JSONL_PATH=logs/10-25-2026/20261025_160201/events.jsonl`.
- Runtime (Payload 2, build 3296 (equivalent source before the final docs-only build), `logs/2026-10-25/20261025_194546/events.jsonl`):
  - `mapeditor select node:0` + `mapeditor nudge 1 0 0` completed in 0.43 s wall
    time from the dev-loop command path.
  - `map-editor.collision-refresh`: `deferred=true`, `duration_ms=7.05` for the
    map-spanning node (no synchronous 1 s stall).
  - `map-editor.autosaved`: `duration_ms=3.74`, path
    `config/maps/payload2.json`.
  - `payload2.json` gained a live `glb_objects` override after nudge and returned
    to `glb_objects: []` after undo. No backup was created by autosave.
  - `mapeditor route_validate`, `mapeditor_camspeed 10`, `mapeditor status`
    completed through the real command queue.
- Earlier full-fallback attempt caused a client main-thread stall (62 s watchdog
  heartbeat). The fallback was changed to defer map-spanning collision rebuilds;
  the repeat runtime test above completed without a stall.
- Test edits to `config/maps/payload2.json` were reverted; the file is clean.

## Focused review

- `docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`. The intake
  items are implemented; editor feel and visual layout still need human review.
- `docs/skills/logging-checker-v1.md`: `PASS`. New events are owned by the map
  editor and collision refresh path and include duration/incremental/deferred
  reasons. Autosave success is status-only; failures use `notify`.

## Human review still needed

- Visual/interaction review of the scrollbar, search, row highlight, bigger
  handles, larger text, and `F` focus distance.
- Confirm autosave feels right while editing large maps (no backup spam).
- Review deferred collision behavior on map-spanning objects and the
  `mapeditor rebuild_collision`/editor-exit flush.
- Re-author Payload routes and playtest the larger map-editor workflow.

## Pre-existing working-tree edits

Preserved all unrelated edits present before this session, including the
existing modified/untracked files reported by `git status`.

## Follow-up — outliner coordinate fix, properties, handles, and route bulk move

- **Time UTC:** `2026-10-25T20:57:05Z`
- **Display time:** `2026-10-25 16:57:05 EDT` (`America/New_York`)
- **Branch:** `20261011zt`
- **Result:** `PASS_WITH_HUMAN_REVIEW`

### Root cause found

`uiBeginScrollArea`/`uiEndScrollArea` and `uiTextInputRender` expect design
coordinates and apply `designToScreen`, but the map editor passes screen pixels
from `uiScreenW()`/`uiScreenH()`. The scissor and scrollbar were therefore scaled
a second time. The outliner list was clipped to an empty region while
`OBJECTS 248` still reported the true count, and the scrollbar landed far from
the panel.

### Files changed

- `src/gui/ui-system.h`, `src/gui/ui-system-buttons.cpp`
  - Added screen-space scroll-area entry points.
  - Shared scrollbar drawing now accepts width/inset and draws inside the panel.
  - Existing design-space callers keep their behavior.
- `src/gui/ui-text-input.h`, `src/gui/ui-text-input.cpp`
  - Added `uiTextInputRenderScreen`; the existing design-space renderer is a thin
    wrapper over the same implementation.
- `src/mapeditor/map-editor.h`, `src/mapeditor/map-editor.cpp`
  - Outliner/search now use the screen-space widgets and manual row offsets.
  - Search results are clickable and no longer clipped.
  - Added a properties panel with editable X/Y/Z position fields and read-only
    rotation/scale/name/type details.
  - Added `selectKind` and `setSelectedPosition`.
  - Replaced ray-axis drag math with screen-projected axis/plane movement.
  - Handles are filled cylinders/spheres using configured length, thickness,
    radius, and pick distance.
  - Applied `block_wire_alpha` to unselected authored block wireframes.
  - Removed the bottom help strip; added a small documentation-path line.
  - Added a rate-limited `map-editor.outliner_layout` runtime diagnostic.
- `src/mapeditor/map-editor-config.h`, `.cpp`, `config/mapeditor.json`
  - Added `properties_height`, `scrollbar_width`, `scrollbar_inset`,
    `handle_thickness`, and `block_wire_alpha`.
  - Raised handle defaults to `12.0 m` length, `60 px` pick distance,
    `0.80 m` end radius, `0.48 m` thickness.
- `src/mapeditor/map-editor-commands.cpp`
  - Added `select_kind`, `select_route`, and `set_position`.
  - Updated usage/help text.
- `docs/specs/entity-editor/entity-editor.md`
  - Documented the outliner/properties behavior, screen-projected drag,
    block alpha, GLB position-only scope, complete command tables, payload path
    select/add/remove/bulk-move workflow, and direct JSON/autosave semantics.
- `docs/specs/open-gameplay-issues.md`
  - Recorded the follow-up implementation and remaining human review.
- `docs/architecture/json-configuration/json-configuration.md`
  - Expanded the map-editor configuration family and repaired a literal `\n`
  that had broken the table row.

### Validation

- Canonical dev-loop builds: `3311`, `3313`, `3316`, `3317` (final source build).
- Runtime executable used for the map-editor scenario: `.dev/current/mimita.exe`,
  build `3313`; the final build `3317` was launched and rechecked separately.
- `EVENTS_JSONL_PATH`:
  `C:\mimita-v9\logs\2026-10-25\20261025_205116\events.jsonl`.
- Final build `3317` runtime launch: `running_build=3317`, server/client ready,
  `builds_match=true`; the `map-editor.outliner_layout` record at
  `2026-10-25T21:00:17Z` (pid `26524`) again reported `object_count=248`,
  `list=(648,180,372,392)`, `properties_height=156`, and
  `scrollbar=14/6`.
- Selftests on `3317`: `--map-authored-geometry-selftest`,
  `--map-config-selftest`, and `--payload-selftest` all PASS.
- `map-editor.outliner_layout` on the live Payload 2 editor:
  `screen=1024x768`, `panel=(644,112,380,624)`,
  `list=(648,180,372,392)`, `object_count=248`,
  `properties_height=156`, `scrollbar_width=14`, `scrollbar_inset=6`.
- `mapeditor select_route` + `mapeditor nudge 0 0 0.5`:
  all 9 route points changed; autosave wrote the map JSON
  (`duration_ms=10.605`); `mapeditor undo` restored all 9 points.
- `mapeditor select cp1` + `mapeditor set_position <x+1> <y> <z>`:
  JSON changed live, autosave wrote it (`duration_ms=3.929`), and undo restored
  the original position.
- Agent visual capture (not human acceptance) observed rows for cart/routes/GLB
  nodes, a visible thick in-panel scrollbar, selected-row outline, filled large
  axis handles, and the properties fields. Final build capture:
  `C:\Users\guita\AppData\Local\Temp\opencode\mapeditor-3317-final.png`.

### Focused review

- `docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`. The
  reported defects have implementation owner and runtime evidence.
- No new regression entry: the outliner defect was part of the same unreleased
  editor slice and is recorded here rather than as a previously working feature.

### Human review still needed

- Visual/feel acceptance of row visibility, scrollbar placement/thickness,
  handle size, drag direction on all axes, and the property-field interaction.
- Multi-select the whole payload route in the outliner and drag it with the
  selection-center handle in a real session.
- Confirm the small documentation-path line is acceptable next to the HUD.

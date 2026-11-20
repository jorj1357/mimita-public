# Map editor chunk integrity, world modes, and meters snapping

# Task

- Session ID: map-chunk-modes-20261119
- Owner: opencode
- Date: 2026-11-19 20:27 UTC (15:27 EST)
- Branch: checkpoint/one-checkout-condensation-20261111
- Status: `PASS_WITH_HUMAN_REVIEW` for chunk delete/placement, shared surface
  data, snap/grid, and mode switching; visual alpha/color and procedural
  traversal remain open.

# Requested behavior

Make authored chunks live-editable (delete whole chunks and parts, show all
placed chunks, save to map JSON), give blocks the same surface fields as chunk
parts, add static/procedural/mixed world modes, use meters-based snapping and a
live grid, and record exact `events.jsonl` proof.

# Specification alignment

Updated `docs/specs/entity-editor/entity-editor.md` with:
- shared authored surface fields for blocks and chunk parts;
- chunk `origin`/`yaw`/`visible`, delete/visibility behavior, and all-chunk
  rendering;
- `world_mode` static/procedural/mixed semantics and map-driven streaming;
- meters snapping/grid configuration;
- the event contract and acceptance assertions.

`docs/specs/procedural-infinite-world` and `gamemodes/rpg.md` remain the mode
and RPG roadmaps; this slice implements the editor/data/runtime plumbing for
modes 1 and 2, not the full RPG content or mode 3 content.

# Implementation

## `src/gamemode/map-config.h/.cpp`

- `AuthoredBlock` and `ChunkPart` now share `color`, `opacity`,
  `material_preset`, `collision`, `anchored` (chunk parts already had
  material/uv/collision/visible).
- `AuthoredChunk` gains `origin`, `yaw`, `visible`.
- `MapObjectiveConfig` gains `world_mode` with static/procedural/mixed parsing
  and derivation from `generate_whole_world` for older files.
- Serialization/parse round-trip preserves all new fields.
- `MapConfigRegistry::deleteChunk`, `currentMutable`, and construction
  `deleteChunk` remove a whole chunk with its parts/connectors/spawns.

## `src/map/map-authored-geometry.cpp` + `map-loader-collision.cpp`

- Static/mixed chunks are appended to the client world as authored geometry at
  `chunk.origin` + yaw; empty chunks still have editor markers.
- Headless server maps append static chunk collision parts.
- Blocks/chunk parts with `collision:false` are rendered but excluded from the
  collision mesh via a per-vertex exclusion mask.
- Geometry signature covers the new surface fields, chunk origin/yaw/visibility,
  and `world_mode`.
- `map-authored-geometry.applied` reports world mode/chunk counts.

## `src/mapeditor/map-editor.cpp/.h`

- Delete/X/Ctrl+X now handle `EditorObjectKind::Chunk` and all chunk item kinds;
  deletion writes the working JSON/version and emits `map-editor.object.deleted`.
- Disabled chunks remain in the outliner with a `disabled` marker; chunk root
  position/yaw and visibility are editable.
- All placed chunks are rendered at their origins in static/mixed mode; the
  floating preview is disabled there to avoid duplicates. Empty chunks draw
  origin/bounds markers.
- Block and chunk-part property UI shares material, flags (collision/anchored/
  visible), and a `Browse` material picker that lists installed textures from
  `assets/textures` and `assets/textureshq`.
- Surface commands: `snap`, `snap_grid`, `grid`, `mode`, `delete`, `collision`,
  `anchor`, `visible`, `surface_color`, `opacity`.
- Snap state initializes from `config/mapeditor.json`, applies to drags and
  nudges, and exposes live grid overlay in meters; G toggles snap, Shift+G
  toggles grid, `[`/`]` change increment.
- `map-editor.scene.applied`, `map-editor.surface.applied`,
  `map-editor.snap.applied`, `map-editor.grid.applied`,
  `map-editor.project.mode.applied`, and `map-editor.chunk.visibility.applied`
  are emitted at their owners.
- Added `map-editor.delete` toolbar action.

## `src/render/render-world-mesh.cpp`, `src/map/map_common.h`, `shaders/basic.frag`

- `Mesh::Batch` gains tint/opacity; authored batches set them from the shared
  surface fields.
- World rendering applies per-batch tint/opacity with blending for transparent
  batches; `uOpacityEnabled` keeps other world-shader users opaque.

## `src/procedural/infinite-world-runtime.cpp`

- Procedural/mixed project maps opt into streaming even through a generic
  local/sandbox mode; `world-stream.profile` and `.update` include project
  revision/world mode.

## `config/mapeditor.json`, `config/gui/map-editor.json`

- Added `snap_enabled`, `snap_increment_m`, `grid_visible`, `grid_spacing_m`,
  `grid_extent_m`, `grid_major_every`; added Delete toolbar element.

## `devscripts/scenarios/map_editor_chunk_integrity_v1.json`

- New bounded scenario: create, add chunk, delete chunk root, add/move block,
  snap 0.1 m, grid on, color/opacity/collision, static->procedural mode, save.

# Build evidence

- Compiled successfully in the dev-loop generation 376/377/378 builds:
  `src/gamemode/map-config.cpp`, `src/map/map-authored-geometry.cpp`,
  `src/map/map-loader-collision.cpp`, `src/mapeditor/map-editor.cpp`,
  `src/mapeditor/map-editor-commands.cpp`, `src/mapeditor/map-editor-config.cpp`,
  `src/procedural/infinite-world-runtime.cpp`, `src/world/world-gltf-loader.cpp`.
- Generation 378 (`build/game/generation-378/game-g378.dll`) published with the
  rounded snap event; run `20261119_204229` used module generation 378.
- The tint/opacity renderer/shader change was saved after generation 378 and is
  pending in the next dev-loop generation (380 was compiling at handoff).
- No compiler or build script was invoked by this session.

# Runtime evidence (real executable, dev-loop)

- Run `20261119_204229`, mode 1 sandbox on `funworld3`, EXE build
  `2026-11-19T20:33:30.354586Z:a6ae23a1...`, module generation 378.
- Client journal: `logs/2026-11-19/20261119_204229/client-01/events.jsonl`.
- `scenario.finished` passed=true, execution_complete=true,
  assertions_evaluated=true, 8/8 assertions
  (`logs/2026-11-19/20261119_204229/dev-loop/events.jsonl`).
- Proven records:
  - `world-project.created` line 130.
  - `map-editor.create.applied` line 352.
  - `map-editor.object.deleted` line 545 (`kind:"chunk"`, id `probe_chunk`,
    part_count 1, connector_count 4).
  - `map-editor.surface.applied` line 859 (`kind:"block"`).
  - `map-editor.snap.applied` line 660 (`increment_m:0.1` after rounding fix).
  - `map-editor.grid.applied` line 712 (`enabled:true`, spacing 0.1 m).
  - `map-editor.project.mode.applied` line 990 (`mode:"procedural"`).
  - `world-project.version-saved` line 1031.
  - `map-authored-geometry.applied` line 986 (`world_mode:"procedural"`).
- Persisted `%LOCALAPPDATA%\Mimita\WorldProjects\creator_chunk_probe\working\map.json`:
  `world_mode="procedural"`, `chunks=[]` (deleted), `surface_block` has
  `color=[0.2,0.6,1,0.5]`, `opacity=0.5`, `collision=false`,
  `anchored=true`.

Divergence found and fixed during the run: snap event serialized `0.1f` as
`0.10000000149`, failing an exact assertion; the event now rounds to three
decimals. The passing 8/8 verdict above was produced after that fix on
generation 378.

# Still open

- Visual review of solid color/alpha transparency, material picker, all-chunk
  rendering, grid, and chunk bounds markers. The tint/opacity renderer change is
  compiled only in the pending next generation.
- The scenario's added `rendered_chunk_count`/`rendered_block_count` assertions
  require the next generation (scene logging was added after the 8/8 run).
- Procedural mode traversal/frontier growth in live Play Test, skybox/fog/
  lighting editing, shapes (sphere/cone/ramp/ladder), direct face-drag with
  WASD/RMB, teleporters/area effects, monster placement, block tracks, and Lua
  remain later slices. Unanchored physical blocks are spec-only.
- No commits were made; pre-existing and concurrent changes were preserved.

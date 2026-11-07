# 2026-11-07 22:56 UTC — Infinite World hot boundary and sizing fixes

## Purpose

Combine the Infinite World sizing/connection/collision/rendering work with the
map-editor hot-module migration. Install one reusable module boundary, then move
generation, rendering policy, collision policy, and editor authoring behavior
behind it so ordinary edits no longer require a full EXE relink.

## Source changes

- `src/modules/api/module-abi.h` now defines the generic v2 lifecycle/event ABI:
  `ModuleV1`, `ModuleEventV1`, `HostServicesV1::getCapability`, capability ids,
  and event ids. The host no longer validates or activates by module name.
- `src/hot-reload/module-host.{h,cpp}` loads catalog-declared events and
  capabilities, dispatches generic events, validates required capabilities,
  and keeps the previous generation on build/interface/state failure. Existing
  `mapEditor*` and `worldRendering*` entry points are thin event wrappers.
- New `infinite-worldgen` module (`src/worldgen/infinite-worldgen-module.cpp`)
  owns connector compatibility, deterministic neighbor choice, quarter-turn
  placement, chunk bounds, and grid-cell placement.
- `world-rendering` now owns the canonical `+X,-X,+Y,-Y,+Z,-Z` six-face box
  geometry and world-unit UV policy. Authored blocks and streamed chunk parts
  both use it through `src/render/world-box-mesh.{h,cpp}`. The old streamed cube
  emitted duplicate `+Z/-Z` faces and never emitted `+X/-X`.
- New `chunk-collision` module (`src/world/chunk-collision-module.cpp`) selects
  collidable parts and emits world-space box triangles after chunk and part
  transforms. `StreamedWorldLayer` runs the existing static-solid-union and
  face-removal path with a raw-triangle fallback.
- `StreamedWorldLayer::refreshChunk` now rebuilds collision for loaded records
  when geometry changes; material/UV edits call it with collision refresh off.
- `InfiniteWorldRuntime` now:
  - polls a validated worldgen profile/chunk registry;
  - uses a bounded world-space connector frontier for `spacing_mode=connector`;
  - interprets connector-mode `stream_radius` as meters;
  - caps loaded chunks at 256 and the pending frontier at 64;
  - records chunk bounds, transform, quarter-turn, and stream distance.
- `WorldGenLibrary::pollReload` watches the active profile and referenced chunk
  files, validates candidate data, and retains the last valid profile/chunks on
  failure.
- `MapConfigRegistry::pollReload` defers external map changes while editor edits
  are unsaved.
- The map editor now adds chunks from the active profile `chunk_size`
  footprint, reports connector/bounds previews, and routes add-chunk,
  chunk-part, connector, and preview commands through the module capability.
- `module-build-worker.py` writes transitive `deps.json` via `-MMD`, has an
  `--audit` overlap check, and prepends the compiler runtime directory to
  `PATH` so Windows `cc1plus` can start.
- Server startup now starts/polls `ModuleHost`; server headless generation uses
  the same worldgen module for connector placement when available.
- Added `config/worldgen/infinite_mixed.json`,
  `config/maps/infinite_mixed.json`, `assets/maps/infinite_mixed.glb`, and
  chunks `mixed_10`, `mixed_25`, `mixed_rect`, `mixed_turn`, `mixed_corridor`.
- Updated connector-mode `stream_radius` values to meters in
  `connected_world`, `infinite_cramped`, `infinite_dungeon_slayer`, and
  `tunnel`.

## Build evidence

- `python devscripts/module-build-worker.py --audit` PASS:
  `files=9 events=15`.
- Module generations built and validated for `map-editor`,
  `world-rendering`, `infinite-worldgen`, and `chunk-collision`.
- Dev-loop build `2026-11-07T22:51:36.445789Z:e460422c...` was published and
  launched; the executable at `C:\mimita-v9\mimita.exe` was relinked after the
  host/runtime changes.

## Runtime evidence

Run `20261107_224413` / client PID `23704`:

- `module.host.started` loaded four modules; `map-editor`,
  `world-rendering`, `infinite-worldgen`, and `chunk-collision` all emitted
  `module.activated`.
- A source save to `src/render/world-rendering-module.cpp` activated
  `world-rendering` generation 7 in the same PID (`previous_generation=6`).
- `infiniteworld load mixed` changed the client to `infinite_mixed`;
  `world-stream.profile` reported `profile=infinite_mixed`,
  `stream_radius=[60,60,0]`, `stream_radius_units=meters`.
- `world-stream.chunk.applied`/`connector.matched` showed `mixed_10`,
  `mixed_25`, `mixed_rect`, `mixed_turn`, and `mixed_corridor` placements with
  world-space bounds and transforms. `mixed_25` bounds were 25 m wide and
  `mixed_rect` 16 x 8 m, confirming authored geometry drove placement.
- Static union ran without fallback: `mixed_25` and `mixed_rect` reported
  `union_input_triangles=24`, `union_output_triangles=28`, `union_solids=2`.
- `agent.aim` + `agent.move 1 0 600` walked to approximately
  `(103.45, 1.20, -94.35)` while chunks streamed; collision summaries showed
  floor contacts (`normal +Z`) and the client remained above the floor.
- The bounded frontier stabilized around `pending≈64`, `loaded≈105` during
  movement instead of growing without limit.

Run `20261107_225226` / client PID `33448`:

- `mapeditor add chunk footprint_probe` through the module emitted
  `map-editor.chunk.added` with `footprint=[10,10,4]`, matching the active
  profile `chunk_size`, then streamed the chunk.
- `mapeditor chunk_preview footprint_probe` and
  `mapeditor preview_connections` executed through the module path.

## Not covered

- Human visual review with back-face culling on all six authored-box faces and
  actual visible-pixel texture-density comparison remains `NOT_COVERED`.
- A runtime case that forces a 90° quarter-turn on the corridor fixture was not
  separately observed; the placement solver supports it but the recorded run
  used unrotated connector matches.
- Cross-chunk seam fall-through was exercised by walking but not by jumping
  across every touching/overlapping/rotated part combination.
- Server/client descriptor-hash agreement was not re-captured after the final
  rebuild.
- Editor save/reload persistence of `footprint_probe` was not retained; the
  file was restored to avoid polluting the acceptance fixture.

## Human review still needed

Inspect an authored box from outside all six faces with culling enabled, check
per-face materials and texture density on 10 m and 25 m surfaces, walk/jump
across rotated and overlapping chunk seams, and confirm connector preview
orientation/bounds visually before saving.

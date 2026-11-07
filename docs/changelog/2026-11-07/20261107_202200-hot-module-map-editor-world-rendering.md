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

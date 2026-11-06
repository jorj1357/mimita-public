# World editor rendering, live chunk edits, and connector placement — 2026-11-06

## Session record

- Timestamp: 2026-11-05 22:50:11 EST (2026-11-06T03:50:11Z).
- Branch: `20261011zt`.
- Commit: not created.
- Pre-existing worktree: Bloodtest-agent edits under `src/combat`,
  `src/network`, `src/entities`, and related docs were present and preserved.
  This session did not edit those files.
- Task: world-editor agent only. Actor damage, snapshots, actor rendering, and
  blood effects remain owned by the Bloodtest agent.

## Implemented

### Rendering correctness

- Primitive face UVs now default to one texture repeat per world unit instead
  of `0..1`. UVs are derived from each face's local extents in
  `src/world/streamed-world-layer.cpp`.
- Added `ChunkPart::uvScale` and `ChunkPart::faceUvScale[6]` with map and
  worldgen-library JSON round-trips. Zero per-face values inherit the part
  scale.
- Legacy `stone_floor`, `stone_wall`, and `stone` material names resolve to the
  existing `floorv2`, `wallv2`, and `stonev2` textures, with one journal event
  per alias. Built-in chunk defaults and the four tracked worldgen chunk files
  now use the existing textures directly.
- `TextureStore::get` emits a one-time diagnostic for missing named textures so
  a fallback checkerboard is identifiable from logs.
- Streamed rendering now clears `uUseInstanceModel` after its draw. The VBO
  branch already cleared it; the instancing branch previously leaked the flag
  into the next base-world/actor draw.
- `worldstream render instancing|vbo` now rebuilds the loaded chunk
  representation in place without clearing chunks or rebuilding collision, and
  emits `world-stream.backend.changed`.

### Live edits without collision rebuild

- Added `StreamedWorldLayer::refreshChunk` to rebuild a loaded chunk's visual
  template and per-chunk VBO records while leaving its collision entity intact.
- Material edits now refresh both the instancing template and VBO records.
  Previously VBO records kept baked vertices and stale batches.
- `InfiniteWorldRuntime` now compares a structural generation signature instead
  of clearing streamed chunks on every `MapConfigRegistry` revision. Material
  and UV-only edits no longer reset chunks or collision. Structural edits emit
  `world-stream.reset` with `reason=generation-signature-changed`.
- Mode-library chunks are cached in the runtime instead of a function-local
  static vector, and `worldchunk save` invalidates that cache.
- Added `worldchunk material` and `worldchunk uv` commands; `worldchunk face`
  now refreshes loaded chunks immediately. Added `worldchunk remove`.

### Connector generation and authoring

- Added connector authority: side/opposite mapping, role matching, tag
  matching (empty tags are unconstrained), reciprocal explicit links, and
  deterministic candidate selection.
- `center` mode keeps uniform grid placement. `connector` mode aligns a
  candidate entry connector to the parent exit plus `connector_gap`.
  Crossings and cycles are filtered from already-resolved neighbors; unmatched
  cells emit `world-stream.connector.missing` instead of disconnected geometry.
- Added `allow_crossings` to `GenerationZone` and per-zone JSON.
- Added `worldconnector remove|connect|disconnect|preview`.
- Added `worldmode crossings` and `worldzone crossings`.
- `InfiniteWorldRuntime` emits `world-stream.anchor`,
  `world-stream.connector.matched`, and `world-stream.connector.missing`.

### Map editor primitive authoring

- Added `ChunkPart` and `ChunkConnector` editor object kinds with
  `<chunkId>/<itemId>` refs.
- Added non-colliding chunk preview placement:
  `mapeditor chunk_preview <chunkId>`. Selecting a chunk outliner row places or
  reuses that chunk's preview at the editor camera.
- Added world preview wire boxes for parts and side markers for connectors.
- Added part picking, position editing through the existing gizmo/position
  path, and properties for material, UV scale, size, rotation, collision, and
  visibility display.
- Added `mapeditor chunk_part_material`, `chunk_part_uv`, and
  `chunk_connector_field` terminal equivalents.
- Chunk part/connector deletion is supported through the existing delete path.
- Entering the map editor no longer reloads an already-active map. That reload
  was wiping unsaved `worldchunk`/`worldedit` in-memory changes and made
  chunk-part selection fail.

### Cross-agent build unblock

- The shared build was blocked by the Bloodtest agent's
  `src/combat/weapon-execution.h:159` using `ActorBodyPart` without the
  `entities/actor-body.h` include. With user approval, this session added that
  one include and did not change any other Bloodtest-owned source.

## Files changed

- `src/gamemode/map-config.h/.cpp`
- `src/world/streamed-world-layer.h/.cpp`
- `src/world/texture-store.cpp`
- `src/procedural/infinite-world-runtime.h/.cpp`
- `src/worldgen/worldgen-library.cpp`
- `src/worldgen/worldgen-commands.cpp`
- `src/mapeditor/map-editor.h/.cpp`
- `src/mapeditor/map-editor-commands.cpp`
- `src/combat/weapon-execution.h` (one-line include only, cross-agent unblock)
- `config/worldgen/chunks/chunk_{straight,corner,cross,deadend}.json`
- `docs/specs/world-projects/worldgen-mode-format.md`
- `docs/specs/world-projects/world-edit-commands.md`
- `docs/specs/entity-editor/entity-editor.md`

## Specification review

- Read `AGENTS.md`, `docs/ROUTER.md`, the world-project, worldgen-mode-format,
  world-edit-commands, procedural-infinite-world, entity-editor, networking,
  dev-loop, runtime-scenario-validation, build/EXE, debug-logging, and
  logging-checker documents.
- Applied `docs/skills/spec-behavior-review-v1.md` and
  `docs/skills/logging-checker-v1.md`.
- Alignment: the spec says primitive chunks are reusable parts plus connectors,
  that connectors match on compatible points/tags, and that missing compatible
  neighbors are reported. The previous implementation ignored connectors in
  placement; this session wires them into deterministic placement and reports
  missing matches.
- Decision: empty connector tags are treated as unconstrained, chosen with the
  user during this session, because existing `funworld3` connectors have no
  tags. Strict intersection alone would leave that authored chunk unmatched.
- Deferred by user decision: GLB `geometryPath` chunk packages remain
  primitive-only; collaborative multiplayer editing and authority replication
  are out of scope for this pass.

## Validation evidence

- Source compile (canonical debug flags, PCH, MinGW PATH):
  `src/gamemode/map-config.cpp`, `src/worldgen/worldgen-library.cpp`,
  `src/worldgen/worldgen-commands.cpp`,
  `src/procedural/infinite-world-runtime.cpp`, `src/world/texture-store.cpp`,
  `src/world/streamed-world-layer.cpp`, `src/mapeditor/map-editor.cpp`, and
  `src/mapeditor/map-editor-commands.cpp` all compiled with exit code 0.
- Full canonical build through the dev loop: success. Published artifact
  `2026-11-06T04:34:48.744150Z` on the running executable.
- Runtime scenario: dev-loop run `20261106_043623`, client journal
  `C:\mimita-v9\logs\2026-11-06\20261106_043623\client-01\events.jsonl`.
  Map `funworld3`, mode `sandbox`, 60 Hz live game loop.
- Runtime journal evidence:
  - `world-stream.anchor` for `infinite_zone` at cell `[2,0,0]`.
  - `world-stream.chunk.applied` with `backend=instancing` for
    `chunk_straight`, `chunk_corner`, `chunk_cross`, and `chunk_deadend`.
  - `world-stream.connector.matched` records show parent/child connectors,
    sides, tags, depth, and `spacing=center`.
  - `worldstream render vbo` emitted
    `world-stream.backend.changed` with `backend=vbo`, `chunks=8`,
    `rebuilt=8`; switching back emitted `backend=instancing`, `chunks=8`.
  - `worldchunk part chunk_a floor ...` emitted `worldgen.chunk.part-added`;
    the structural change correctly emitted `world-stream.reset` with
    `reason=generation-signature-changed`.
  - `worldchunk material chunk_a floor all wallv2` emitted
    `chunk.part.material-changed` and `world-stream.template.updated` with
    `collision_rebuilt=false`; no new reset followed.
  - `worldchunk uv chunk_a floor all 2 2` emitted
    `worldgen.chunk.uv-scale` and `world-stream.template.updated` with
    `collision_rebuilt=false`; no new reset followed.
  - `mapeditor 1`, `mapeditor chunk_preview chunk_a`,
    `mapeditor chunk_part_material chunk_a/floor wallv2`, and
    `mapeditor chunk_part_uv chunk_a/floor 2 2` emitted
    `map-editor.chunk-preview`, `map-editor.chunk-part-material`,
    `map-editor.chunk-part-uv`, and both `world-stream.template.updated`
    records with `collision_rebuilt=false`.
  - An earlier run (`20261106_035748`) emitted
    `worldgen.connector.preview` with one match,
    `worldgen.connector.connected`, and `worldgen.connector.disconnected` for
    a reciprocal `chunk_a/west_a <-> test_b/east_b` link.
- Visual/human acceptance: not performed. The reported instancing pixelation
  has not yet been visually accepted on the current executable. The journal
  proves the backend switch, UV refresh, and collision-preserving material
  refresh paths ran; it does not prove the scene looks correct.

## Runtime evidence still required

1. Human visual check: compare `worldstream render instancing` and
   `worldstream render vbo` at the same camera position, chunks, and materials.
2. Human visual check: confirm the default texture density is no longer
   stretched/pixelated and that per-part/UV overrides look correct.
3. Save/reload: confirm chunk/zone/UV/link JSON round-trips and that a loaded
   material edit persists after restart.
4. Multiplayer/collaborative editing remains deferred and is not claimed.

## Spec TODO note

`docs/specs/procedural-infinite-world/procedural-infinite-world.md:1` still
contains the informal TODO about explaining infinite-world generation. It was
not edited, per the router rule.

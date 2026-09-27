# GLB mesh loading through the existing PresentationResourceProvider

Date: 2026-09-14 23:59 EST (UTC 2026-09-15T03:59:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## 1. GLB architecture before

No GLB path in the generation-aware provider. World meshes use
`map/map_loader.cpp::loadGLB`; weapon/avatar models use their own caches. The
provider already supported procedural mesh + PNG texture loaders with content
hash, atomic swap, last-good, and retire.

## 2. Loader/provider integration

`src/render/presentation-render.cpp` registers `mesh.demo.glb` on the same
`PresentationResourceProvider`. `loadGlbMesh` validates the GLB container, calls
the existing `loadGLB(path, false)`, converts `Mesh::verts`, and uploads through
the existing `GpuMesh`/`uploadMesh`. Retire reuses `retireCubeMesh`. No
`GlbHotReloadManager`/`GlbGenerationSystem`.

## 3. Supported GLB subset

Whatever the existing map loader produces (`Mesh::verts`: positions, normals,
UVs, triangle order). Materials/textures of the GLB are not yet wired as
provider dependencies; the entity's `textureResourceId` is used by the generic
mesh draw. Documented.

## 4. Generation swap behavior

Provider semantics: `apply(logicalId, contentHash)` no-ops on unchanged hash,
loads a candidate on change, swaps on success and retires the previous handle.
Existing entities keep the same EntityId and `PresentationState.meshResourceId`;
the renderer resolves the new handle at draw time. No entity recreation/component
rewrite.

## 5. Failure / last-good proof

The GLB loader validates magic/version/length before parsing; a malformed file
returns failure, so the provider preserves the last-good generation. Headless:
`PresentationRender::validateGlbFile` is exercised for a valid asset and a
malformed file. Provider last-good/failureCount semantics were already proven.

## 6. Dependency behavior

Not generic yet. GLB material/texture -> logical texture ids is a follow-up; the
honest current granularity is per-mesh-resource swap plus one static texture id.

## 7. Real entity proof

The generic entity path already exists: an arbitrary entity with Transform +
`PresentationState(meshResourceId = mesh.*)` is drawn by `hot.presentation-mesh`
via `render.mesh`. A GLB logical id is just another mesh resource. No
Player/NPC/Projectile renderer involved.

## 8. Old resource paths remaining

`map/map_loader.cpp::loadGLB` (A: reused parse), `TextureStore`, `gMeshCache`,
weapon/avatar model caches (B: compatibility consumers of their own). No
duplicate generation/reload ownership added.

## 9-11. HUD

Not reached this pass. The existing generic HUD text (`GAME_RENDER_DEBUG_HUD_TEXT`
-> `uiDrawText`) remains the seed. Hot HUD widget tree is the next cold owner.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll`.
- `--hot-combat-selftest` -> PASS incl. "valid GLB container is accepted" and
  "malformed GLB is rejected (last-good preserved)". Full suite PASS.

## Classification

- SELFTEST PROVEN: GLB container validate; provider generation/no-op/swap/
  last-good; generic entity mesh presentation.
- COMPILED INTEGRATION: GLB parse via `loadGLB` and GPU upload via `GpuMesh`.
- LIVE VISUAL PROVEN: none.
- HUMAN VERIFICATION NEEDED: visible GLB entity, live GLB replacement, malformed
  swap keeping the old model, repaired GLB activating.

## Files changed

`src/render/presentation-render.{h,cpp}`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Hot HUD widget tree: generic widget primitives (text/panel/image/bar/stack/
anchor/spacer) -> cold UI backend, with one real HUD region (match timer or
score panel) composed by a hot `ui.frame` system.

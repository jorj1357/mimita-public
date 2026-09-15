# Generic presentation of runtime entities + real mesh/texture resources

Date: 2026-09-14 21:00 EST (UTC 2026-09-15T01:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Generic presentation of runtime/replicated entities. No server/entity-lifecycle,
transport, or gamemode work (owned by a concurrent agent).

## 1. Generic presentation state

Added a hot `PresentationState` dynamic schema
(`meshResourceId`, `textureResourceId`, `flags`, `scale`, `color`) registered
`GAME_NET_ALL`. Any entity may carry it; the type is never inspected. There is no
`PlayerPresentation`/`NpcPresentation`/`ProjectilePresentation`.

## 2. Replicated entity -> presentation

Flow implemented: entity -> `Transform` -> `PresentationState` -> hot
`hot.presentation-mesh` (render.frame) -> `render.mesh` command -> cold
`PresentationRender::submitMesh`. Local synthetic entities are used for the
headless proof; the same schema is replicated so a future replicated entity
feeds the same path.

## 3. Generic mesh render command

Added `GAME_CAP_RENDER_MESH` + `GameRenderMeshCommandV1` (append-only; no
`GameplayContextV1` field). The command carries entity, logical mesh/texture ids,
transform, scale, and color. Kernel `capRenderMesh` forwards to the cold
renderer. No `renderPlayer`/`renderNpc`/`renderRocket`.

## 4. Unknown-entity proof

`--hot-combat-selftest` creates a typeless entity (Transform +
`PresentationState`), runs the render domain, and asserts a generic mesh
submission occurred. Also verifies the wire-shape `PresentationDebug` path.

## 5. Model resource provider

Real procedural cube mesh loader registered with `PresentationResourceProvider`
(`gameHash("mesh.cube")`): GL VAO/VBO/EBO upload, generation swap, retire. GLB
parsing remains for a later slice; the provider/loader/retire mechanism is the
same.

## 6. Texture resource provider

Real PNG loader (`assets/textures/circuitryv1.png`, `gameHash("texture.default")`)
via stb_image -> GL texture, registered in the same provider with content-hash
polling (`PresentationRender::poll`). Same abstraction as the mesh.

## 7. Logical, not raw handles

Hot state holds logical resource ids; `submitMesh` resolves the current
generation handle from the provider. A generation swap does not touch entities.

## 8. Real presentation owner migration

Partially: the generic mesh path is a new presentation owner; the typed
projectile render traversal was not migrated this pass (deferred to avoid
touching another owner's in-flight projectile work). Not claiming it removed.

## 9. Live edit proof

Not run (no visible client). Provider semantics SELFTEST PROVEN; mesh/texture
loader path COMPILED INTEGRATION; visual swap HUMAN VERIFICATION NEEDED.

## 10-12. HUD/UI audit + hot HUD path

HUD composition is `engineTickUI` -> `engineTickUIHUD` / overlays / game HUD,
fixed C++ in the EXE (COLD), with mode-specific text embedded. Removed one cold
dependency: added `GAME_RENDER_DEBUG_HUD_TEXT` and a generic hot HUD panel
composed by `hot.debug-presentation` through `render.debug` -> `uiDrawText`
(flushed by `uiEndFrame` after the render domain). No `GamemodeHudType`.

## 13. Cold GPU mechanism

Unchanged and intentionally cold: GL context, buffers, textures, shader compile,
draw calls.

## 14. Concurrency

No edits to the other agent's server/entity-lifecycle code. (Earlier pass added
one missing include to their `npc-entity-selftest.cpp`; not repeated here.)

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `build_game_dll.py` -> `build/mimita-game.dll`, sources=24.
- `--hot-combat-selftest` -> PASS (render.mesh resolves; meshed generic entity
  presented; provider load/no-op/swap/last-good).
- `--dynamic-replication-`, `--dynamic-lifecycle-`, `--live-code-`,
  `--capability-`, `--gamemode-hot-`, `--movement-parity-`,
  `--hot-authoritative-`, `--entity-slice-selftest` -> PASS.

## Classification

- SELFTEST PROVEN: generic render.debug/render.mesh command chain, hot
  render.frame presentation of typeless entities, provider generation/no-op/
  swap/last-good, hot HUD text path (submission counts).
- COMPILED INTEGRATION: real procedural mesh + PNG texture loaders through the
  provider; per-frame poll; shader reload.
- HUMAN VERIFICATION NEEDED / LIVE VISUAL PENDING: visible wireframe/shader/
  mesh/texture swap in a running client; replicated-entity-driven presentation.

## Files changed

`src/hot-reload/game-api.h`, `src/live-code/live-behavior.cpp`,
`src/render/presentation-render.{h,cpp}`,
`src/hot-reload/modules/presentation/debug-presentation.cpp`,
`src/engine/engine-tick-render.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next cold owner selected

Migrate the typed projectile presentation path (`projectile-render.cpp` +
`mpRenderNetworkProjectiles` + NPC/projectile call sites) onto
`PresentationState` + `render.mesh`, then GLB model loading, then the hot HUD
widget tree.

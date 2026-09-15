# Hot presentation capability + generation-aware resource provider

Date: 2026-09-14 20:15 EST (UTC 2026-09-15T00:15:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

Presentation-side architecture target: hot presentation systems -> generic
render/resource capabilities -> cold GPU mechanism. No server replication,
transport, projectile, or gamemode work (owned by a concurrent agent).

## 1. Presentation architecture before (audit)

- "What draws / representation / material" is hardcoded in `engineTickRender`
  and per-system `render()` (COLD); projectile presentation had one hot policy
  seam (`GAME_EVENT_PROJECTILE_PRESENT`).
- All resource resolution (`TextureStore`, `gMeshCache`, weapon/avatar caches,
  sound cache, font pages) is per-owner, name/path-keyed, no content hash (COLD).
- All low-level GL is COLD. The intended small mechanical kernel layer is
  device/context, buffer/texture allocation, shader compile primitive, draw
  submission, and resource lifetime.

## 2. Exact cold owner targeted

- Debug/wireframe presentation policy: `engine-tick-render.cpp` calls +
  `DebugVis` usage (COLD policy) and per-pass draw selection.
- Shader resource resolution: `Renderer::shaderProgram` loaded once in the
  constructor, never reloaded.

## 3. Generic primitives added/reused

- Added `GAME_CAP_RENDER_DEBUG` + `GameRenderDebugCommandV1`
  (`game-api.h`, append-only; no `GameplayContextV1` field). Kernel provider
  `capRenderDebug` (`live-behavior.cpp`) maps shape to `DebugVis`. Reused the
  existing capability resolver and `DebugVis` buffer/flush mechanism.
- Added `LiveBehavior::flushRenderDebug()`; called after the `GAME_DOMAIN_RENDER`
  run in `engine-tick-ui.cpp` so hot presentation is same-frame.
- Added `DebugVis::debugLineVertexCount()` and a global `flushDebugLines`
  declaration for headless verification.
- Added `PresentationResourceProvider` (`src/project/presentation-resource.*`):
  logical id -> content hash -> generation -> opaque handle; `setLoader`,
  `adopt`, `apply` (no-op on same hash, last-good on failure, retire at swap),
  `current`/`handleOf`/`generationOf`.

## 4. Wireframe/debug migration result

New hot module `src/hot-reload/modules/presentation/debug-presentation.cpp`
registers a `render.frame` system (`hot.debug-presentation`) that presents every
entity carrying the package `PresentationDebug` dynamic component through
`render.debug`. The policy (which entities, color, size, shape) lives in hot
source; editing it and saving changes the running client's behavior. A
`hotpresent` command creates a generic-presented entity. `hot-modules.json`
globs gained `modules/presentation/*.cpp`.

## 5. Resource-provider design

`logical id -> loader/retire callbacks -> content hash -> generation -> handle`.
`apply(logicalId, contentHash)` skips reload on an unchanged hash, compiles/loads
a candidate on change, keeps the last-good generation and records
`lastError`/`failureCount` on failure, and on success advances the generation
and retires the previous handle at the swap boundary.

## 6. Shader proof (mechanism; live visual proof NOT run)

`Renderer::pollShaderReload()` hashes `shaders/basic.vert` + `.frag`, and on
change calls the provider. `createProgramStrict` returns 0 on compile OR link
failure, so a bad edit keeps the last-good `shaderProgram`; success swaps and
logs `[SHADER] live reload generation=N`. The provider swap/last-good semantics
are verified headlessly; the visible edit/rename/restore falsification requires a
running client and was not performed.

## 7. Model/texture proof

Not wired. The same provider supports them via loader/retire callbacks; texture
and GLB loaders are the next slice.

## 8. Unknown-runtime-entity presentation proof

`--hot-combat-selftest` creates an entity with only `Transform` and the package
`PresentationDebug` dynamic component, runs the `render.frame` domain, and
asserts the kernel line buffer grew. This proves a runtime entity unknown to the
EXE is presented through generic presentation state with no Player/NPC/Projectile
switch.

## 9. Remaining cold presentation owners

- `engine-tick-render.cpp` traversal and per-system draw selection.
- `render-player.cpp` / `player-render.cpp` representation + outline/wireframe.
- `projectile-render.cpp` mesh/config and `renderProjectile`.
- `effect-part-render.cpp`, `hit-effects-render.cpp`, `debug-visuals-*` draws.
- `gui/*`, `engine-tick-ui*` HUD composition.
- `renderer.cpp`, `render-world*`, `shadow*`, `skybox.cpp`, `post-fx.cpp` GL.

## 10. Files changed

`src/hot-reload/game-api.h`, `src/live-code/live-behavior.{h,cpp}`,
`src/debug/debug-visuals.h`, `src/debug/debug-visuals-lines.cpp`,
`src/engine/engine-tick-ui.cpp`, `src/engine/engine-tick-render.cpp`,
`src/renderer/renderer.{h,cpp}`, `src/project/presentation-resource.{h,cpp}`,
`src/hot-reload/modules/presentation/debug-presentation.cpp`,
`src/hot-reload/hot-modules.json`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## 11. Build / selftest results

- `python build_agent.py` -> `Status: SUCCESS`.
- `python devscripts/live-build.py` -> generation 16; `build/mimita-game.dll`
  rebuilt.
- `--hot-combat-selftest` -> PASS, incl. render.debug resolve, kernel draw
  buffer, hot render.frame generic entity, provider generation/no-op/swap/
  last-good.
- `--dynamic-replication-`, `--dynamic-lifecycle-`, `--live-code-`,
  `--capability-`, `--gamemode-hot-`, `--movement-parity-`,
  `--hot-authoritative-`, `--entity-slice-selftest` -> PASS.

## 12. Real visual/live proof vs unverified

Verified (headless/source): the generic render command path, the hot
render.frame presentation of a generic entity, provider generation/last-good,
and the hot module compiling into the DLL. Unverified (not run): the live
wireframe edit, live shader edit + bad-shader + fix, texture/model replacement,
and any visual result. These need a running visible client.

## 13. Concurrent-session blocker (documented, not redesigned)

The other agent's new `src/network/npc-entity-selftest.cpp` was missing
`#include "ecs/relationship-store.h"`, which blocked the full build. One include
line was added; its design was not touched. Its `ENTITY_CREATE`/`DESTROY`
replication work is explicitly out of scope here.

## Next cold owner selected

Generic entity presentation driven by replicated entities (connect Agent 1's
replicated runtime entities to the `PresentationDebug`/render.frame path), then
hot HUD/UI composition.

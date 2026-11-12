# Render command inventory (2026-11-12)

**Status:** static scan of draw calls, GPU resource allocation, shader
compile/link, state changes, and framebuffers. It is the hot-render inventory,
not a runtime resource-lifetime proof.

- Sites: 746
- Files with GPU work: 49
- Machine table: `docs/handoffs/cpp-render-command-inventory-20261112.csv`

## Hot-render target

`game.dll` owns draw policy and builds generic render commands. The resident
renderer owns the device, context, shader program service, and GPU resource
registry. A new primitive should be a command + mesh + shader asset, not a new
EXE draw call. A new backend/device feature remains a cold boundary.

## Sites by category

| Category | Sites |
|---|---:|
| state | 231 |
| vao_alloc | 151 |
| texture_alloc | 118 |
| buffer_alloc | 84 |
| shader | 79 |
| draw | 45 |
| framebuffer | 38 |

## Files by GPU work

| File | draw | buffer | texture | vao | shader | state | framebuffer |
|---|---:|---:|---:|---:|---:|---:|---:|
| `src/shadow/shadow-render.cpp` | 4 | 4 | 9 | 10 | 14 | 9 | 7 |
| `src/render/render-player.cpp` | 1 | 2 | 0 | 3 | 2 | 38 | 0 |
| `src/physics/physical-entity.cpp` | 3 | 6 | 0 | 13 | 3 | 20 | 0 |
| `src/entities/player-render.cpp` | 8 | 0 | 0 | 4 | 7 | 22 | 0 |
| `src/render/post-fx.cpp` | 1 | 2 | 8 | 5 | 7 | 5 | 12 |
| `src/effects/effect-part-render.cpp` | 2 | 4 | 0 | 7 | 2 | 21 | 0 |
| `src/render/skybox.cpp` | 1 | 3 | 11 | 6 | 7 | 8 | 0 |
| `src/engine/engine-tick-render.cpp` | 3 | 6 | 0 | 10 | 3 | 10 | 0 |
| `src/world/streamed-world-layer.cpp` | 2 | 12 | 0 | 13 | 2 | 2 | 0 |
| `src/entities/player-render-mesh.cpp` | 2 | 10 | 0 | 13 | 0 | 3 | 0 |
| `src/combat/projectile-render.cpp` | 2 | 5 | 0 | 11 | 2 | 6 | 0 |
| `src/avatar/avatar-atlas.cpp` | 0 | 0 | 22 | 0 | 0 | 0 | 0 |
| `src/gui/ui-system-render.cpp` | 2 | 4 | 0 | 4 | 7 | 2 | 0 |
| `src/debug/debug-visuals-lines.cpp` | 2 | 4 | 0 | 4 | 3 | 4 | 0 |
| `src/impact/destructible-render.cpp` | 1 | 4 | 0 | 5 | 1 | 6 | 0 |
| `src/replay/replay-export-target.cpp` | 0 | 0 | 7 | 0 | 0 | 0 | 7 |
| `src/map/map-loader-material.cpp` | 0 | 0 | 13 | 0 | 0 | 0 | 0 |
| `src/map/map_render.cpp` | 3 | 2 | 0 | 8 | 0 | 0 | 0 |
| `src/render/procedural-cylinder.cpp` | 1 | 2 | 0 | 5 | 1 | 4 | 0 |
| `src/render/render-world-mesh.cpp` | 1 | 2 | 0 | 4 | 1 | 5 | 0 |
| `src/avatar/cosmetic-system.cpp` | 1 | 2 | 0 | 4 | 0 | 5 | 0 |
| `src/debug/debug-visuals-tris.cpp` | 1 | 2 | 0 | 2 | 1 | 6 | 0 |
| `src/entities/player-loader.cpp` | 0 | 0 | 12 | 0 | 0 | 0 | 0 |
| `src/map/texture.cpp` | 0 | 0 | 12 | 0 | 0 | 0 | 0 |
| `src/render/render-world-sky.cpp` | 1 | 2 | 0 | 5 | 2 | 2 | 0 |
| `src/combat/weapon-model-cache.cpp` | 0 | 2 | 6 | 3 | 0 | 0 | 0 |
| `src/render/render-world-shadow.cpp` | 1 | 0 | 0 | 2 | 2 | 6 | 0 |
| `src/gui/font-stuff/font-loader.cpp` | 0 | 2 | 6 | 2 | 0 | 0 | 0 |
| `src/combat/revolver-system.cpp` | 1 | 2 | 0 | 5 | 1 | 0 | 0 |
| `src/gui/gui-media.cpp` | 0 | 0 | 8 | 0 | 0 | 0 | 0 |
| `src/gui/ui-system.cpp` | 0 | 0 | 0 | 0 | 2 | 6 | 0 |
| `src/gui/menus/menu-avatar-preview.cpp` | 0 | 0 | 0 | 0 | 0 | 7 | 0 |
| `src/renderer/renderer.cpp` | 0 | 0 | 0 | 0 | 5 | 2 | 0 |
| `src/gui/ui-system-buttons.cpp` | 0 | 0 | 0 | 0 | 0 | 6 | 0 |
| `src/debug/debug-diag-commands.cpp` | 0 | 0 | 1 | 0 | 0 | 0 | 4 |
| `src/gui/gui-main.cpp` | 0 | 0 | 0 | 0 | 1 | 4 | 0 |
| `src/render/render-world.cpp` | 0 | 0 | 0 | 1 | 1 | 3 | 0 |
| `src/replay/replay-export.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 5 |
| `src/combat/weapon-viewmodel.cpp` | 1 | 0 | 0 | 2 | 1 | 0 | 0 |
| `src/game/game-cli.cpp` | 0 | 0 | 0 | 0 | 0 | 1 | 3 |
| `src/gui/ui-text-input.cpp` | 0 | 0 | 0 | 0 | 0 | 4 | 0 |
| `src/main-systems.cpp` | 0 | 0 | 0 | 0 | 0 | 4 | 0 |
| `src/notifications/notifications.cpp` | 0 | 0 | 0 | 0 | 0 | 4 | 0 |
| `src/avatar/avatar-editor.cpp` | 0 | 0 | 0 | 0 | 0 | 3 | 0 |
| `src/network/client.cpp` | 0 | 0 | 0 | 0 | 0 | 3 | 0 |
| `src/avatar/avatar.cpp` | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/debug/debug-visuals-scene.cpp` | 0 | 0 | 0 | 0 | 1 | 0 | 0 |
| `src/map/map_loader.cpp` | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/world/texture-store.cpp` | 0 | 0 | 1 | 0 | 0 | 0 | 0 |

## Shader assets

- `shaders/basic.frag`
- `shaders/basic.vert`
- `shaders/post.frag`
- `shaders/post.vert`
- `shaders/shadow.frag`
- `shaders/shadow.vert`
- `shaders/skybox.frag`
- `shaders/skybox.vert`

## Required before render code moves

- A generic `CAP_RENDER` draw-command struct (vertices, indices, material, transform, state).
- A `CAP_RESOURCES` registry for VAO/VBO/texture/framebuffer ownership and release under the current context.
- A shader-asset service that compiles/validates/swaps independently of the code generation.
- Per-file classification of cached GPU objects vs per-call setup; cached objects need a resident owner or generation release.

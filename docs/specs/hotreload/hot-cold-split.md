# Hot/Cold Split (2026 11 14 1008)

This file is the absolute human-readable split between resident (cold) code and
replaceable (hot) code. It answers "does this source exist in the resident EXE
or in `game.dll`?" for every compiled translation unit.

`build/source-manifest.json` (written by
`python devscripts/source_manifest.py --write`) is the machine source of truth
for the same question. `config/source-owners.json` and
`config/modules/modules.json` are the owner inputs. If this file and the
manifest disagree, the manifest wins and this file is corrected in the same
change.

## Owner vocabulary

| Owner | Meaning |
| --- | --- |
| `resident` | Stable EXE code. Must not unload across a `game.dll` swap. |
| `bootstrap` | Process entry and orchestration. Resident in practice. |
| `game` | Replaceable game implementation. This is the set that becomes one `game.dll`. |
| `module:<name>` | Existing subsystem `ModuleV1` DLL, not the whole-game DLL. |
| `launcher` / `tool` / `test` / `vendor` | Not part of the whole-game hot/cold split. |

## Cold (resident EXE)

These files stay loaded for the life of the process. They own the process,
device, transport, logging, and worker lifetimes that outlive a generation.

| Path | Responsibility | Why cold |
| --- | --- | --- |
| `src/main.cpp` | process entry, `glfwInit`, main loop | must survive a swap |
| `src/main-init.cpp` | GLFW input callbacks, poll wiring | process window lifetime; owner-map override pending |
| `src/main-systems.cpp` | startup orchestration, resident state lookup | called once at process start; owner-map override pending |
| `src/renderer/renderer.cpp` | window, GL context, GLAD, swap, poll, video mode, cursor, VSync | device lifetime |
| `src/glad.c` | GL dispatch | must stay loaded with the context |
| `src/host/**/*.cpp` | host ABI, loader, state store, barrier, capability thunks | generation barrier |
| `src/hot-reload/**/*.cpp` | loader, module host, DLL mechanism | owns generation swaps |
| `src/debug/log-manager.cpp` | process-wide logging | outlives generations |
| `src/debug/structured-log.cpp` | canonical `events.jsonl` writer | outlives generations |
| `src/debug/crash-handler.cpp` | crash handling | outlives generations |
| `src/audio/audio.cpp` | audio device engine | device lifetime |
| `src/audio/music-manager.cpp` | decode worker | worker lifetime |
| `src/network/net_common.cpp` | socket/WSA lifetime | process network lifetime |
| `src/network/server.cpp` | dedicated/listen server loop | session lifetime |
| `src/network/server-ice.cpp` | ICE server | transport lifetime |
| `src/network/server-authored-rebuild.cpp` | authored-world rebuild worker | worker lifetime |
| `src/network/ice/ice-agent.cpp` | ICE agent | transport lifetime |
| `src/network/ice/ice-server.cpp` | ICE server | transport lifetime |
| `src/persistence/persistence-queue.cpp` | persistence worker | worker lifetime |
| `src/duel/duel-queue.cpp` | matchmaking poll thread | worker lifetime |
| `src/utils/json-file-watch.cpp` | file-watch worker | worker lifetime |
| `src/replay/replay-factory-worker.cpp` | replay save executor | worker lifetime |

Cold rendering is limited to the window/context/swap/thunk list above.
Anything else that decides what to draw is hot.

## Hot (replaceable `game.dll`)

These files are ordinary game implementation. They must be safe to unload and
reload in one generation swap.

| Path | Responsibility |
| --- | --- |
| `src/render/**/*.cpp` | render contract calls, world/player/shadow/skybox/postfx |
| `src/render/world-shader.*`, `src/render/shader-program.*` | shader ownership and reload |
| `src/shadow/shadow-shader.*` | shadow shader ownership |
| `src/engine/**/*.cpp` | simulation and render frame |
| `src/combat/**/*.cpp` | weapons, viewmodel, projectiles, effects |
| `src/effects/**/*.cpp` | visual effects |
| `src/entities/**/*.cpp` | player/actor loaders and render |
| `src/avatar/**/*.cpp` | avatars, atlases, cosmetics |
| `src/map/**/*.cpp` | map load, texture, material, render |
| `src/world/**/*.cpp` | world simulation, streamed layers |
| `src/physics/**/*.cpp` | physics |
| `src/gui/**/*.cpp` | game UI |
| `src/debug/**/*.cpp` (except the three resident files above) | diagnostics and debug visuals |
| `src/game/**/*.cpp` (except `src/game/game-cli.cpp` bootstrap override) | game module and capability bridges |
| `src/network/**/*.cpp` (except the resident network list above) | packet handlers and client logic |
| `src/replay/**/*.cpp` (except `replay-factory-worker.cpp`) | replay and export |
| `src/video/**/*.cpp` | video settings and output |
| `src/audio/**/*.cpp` (except `audio.cpp`, `music-manager.cpp`) | audio behavior |
| `src/auth/**/*.cpp`, `src/chat/**/*.cpp`, `src/gamemodes/**/*.cpp`, `src/npc/**/*.cpp` | gameplay features |
| `external/recastnavigation/**/*.cpp` | navigation compiled into the game build |

## Not part of the split

| Path | Owner |
| --- | --- |
| `launcher/**/*.cpp` | `launcher` |
| `tools/**/*.cpp` | `tool` |
| `tests/**/*.cpp` | `test` |
| `external/**` test sources | `test` |
| `external/**/*.cpp` not listed above | `vendor` |
| `src/game/game-cli.cpp` | `bootstrap` override |

## Transitional now (exists only until cutover)

| Path | Owner | Delete at cutover |
| --- | --- | --- |
| `src/render/render-command-legacy.cpp` | game | yes |
| `src/host/game-gl-legacy.cpp` | resident | yes |
| `src/host/game-state-legacy.cpp` | resident | yes |
| `src/host/game-job-legacy.cpp` | resident | yes |
| `src/host/game-file-legacy.cpp` | resident | yes |
| `src/host/game-socket-legacy.cpp` | resident | yes |

## Rendering split (D1)

| Cold | Hot |
| --- | --- |
| window creation, GL context, GLAD, framebuffer size, `glfwSwapBuffers`, `glfwPollEvents`, video mode, cursor, VSync, shutdown | shaders, programs, uniforms, GL state, textures, render targets, clear color, viewport, passes, all draws |
| `src/renderer/renderer.cpp` | `src/render/**`, `src/engine/engine-tick-render.cpp`, `src/combat/*render*`, `src/effects/*`, `src/entities/*render*`, `src/map/*render*`, `src/world/streamed-world-layer.*`, `src/avatar/*render*` |

The resident `Renderer::shaderProgram` path is deleted. Shader generation is
owned by `src/render/world-shader.*`.

## Pending moves

| Item | Current | Target |
| --- | --- | --- |
| GLAD load call | `src/engine/engine-init.cpp` (hot) | `src/renderer/renderer.cpp` (cold) |
| `src/main-init.cpp` owner | falls through to `game` default | explicit `bootstrap`/`resident` override |
| `src/main-systems.cpp` owner | falls through to `game` default | explicit `bootstrap`/`resident` override |

## Completeness rule

1. Every compiled `.cpp` appears in exactly one of: cold, hot, not-part-of-split,
   transitional.
2. `source_manifest.py --write` is run after any move; the manifest must report
   no unclassified source.
3. A new file must be placed here in the same change that adds it.
4. When a file moves tables, update this file, `config/source-owners.json`, and
   the manifest together.

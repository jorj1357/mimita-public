# Agent C: whole-game tick/render entry, fixed-tick clock, and frame split

Session date: 2026-11-13 08:45 local. Scope: Agent C lane on the shared
checkout. Added the game-owned whole-game entry point, made the fixed 60 Hz
accumulator a single named owner, and split the engine frame into simulation and
render halves so the module ABI can own each at its own boundary. The host
wiring in `src/game/game-module.cpp` and `src/main.cpp` is Agent A's; nothing
here performs the ownership cutover.

## Source changes

- Added `src/engine/fixed-tick-clock.h` and `.cpp`: one owner for the fixed
  60 Hz simulation accumulator. `advance` clamps the backlog to
  `maxCatchupTicks * TICK_DT`, `due`/`consume` step whole ticks, and `alpha`
  exposes the presentation fraction.
- Refactored `src/engine/engine-tick-replay.cpp` to use `FixedTickClock`
  instead of the file-local `static double simAccumulator`. The loop order,
  catch-up cap, `gSimMaxCatchupTicks` runtime knob, and
  `setAnimationRenderAlpha` value are unchanged. `SIM_DT` now aliases
  `FixedTickClock::TICK_DT`.
- Split `src/engine/engine-tick.cpp` into two entry points:
  - `engineTickSimulate` runs the frame preamble, setup (which begins the device
    frame and polls hot-reload config), audio, listen server, state, replay
    fixed-tick simulation, networking, camera, and combat.
  - `engineTickRenderFrame` runs render, UI, dev overlay, menus, input, terminal,
    PostFX, swap, and frame pacing, then ends the perf frame.
  - `engineTick` remains the single-call legacy wrapper for the resident EXE and
    calls simulate then render, preserving frame order and behavior.
  - `EngineFrameState` in `src/engine/engine-tick.h` carries `dt`,
    `worldPassRan`, the frame start time, and the measured frame time between the
    halves.
  - The perf scopes `EngineTick::Work` are now `EngineTick::Simulate` and
    `EngineTick::Render`. This is a profiler-label change only.
- Added `src/engine/whole-game-entry.h` and `.cpp`: the game-owned entry that
  the module lifecycle can call.
  - `initializeWholeGameEntry` creates a generation-owned `Engine`; under
    `MIMITA_GAME_DLL`, `Engine::init` borrows the resident `Renderer` through
    `CAP_RENDER_DEVICE` instead of constructing a device.
  - `tickWholeGame` runs `engineTickSimulate`; `renderWholeGame` runs
    `engineTickRenderFrame` with one persistent `EngineFrameState`.
  - `shutdownWholeGameEntry` releases only generation-owned state and never
    calls `Engine::shutdown`, because in DLL mode that would shut down the
    resident device.
- Added `src/render/render-command.h` and `src/render/render-command-legacy.cpp`:
  the game-owned render submission contract. `render-command.h` declares the
  full surface (bind, state, upload, draw, uniforms, primitive shader service,
  state guards, render targets) and is the authoritative missing-operation list
  for `CAP_RENDER`. `render-command-legacy.cpp` is `#ifndef MIMITA_GAME_DLL` and
  maps the semantic enums to raw OpenGL, so the resident EXE path is unchanged
  and no call sites move yet. The DLL build compiles it as an empty TU and
  references no missing capability symbols, so the whole-game link cannot break
  from this change.
- Added `src/render/render-types.h`: backend-neutral render vocabulary
  (`Primitive`, `TextureTarget`, `BufferTarget`, `BufferUsage`, `Capability`,
  `BlendFactor`, `DepthFunc`, `CullFaceMode`, `PolygonMode`, `IndexType`,
  `TextureFormat`, `PixelFormat`, `PixelType`, `TextureParameter`,
  `TextureValue`, `VertexType`, `ShaderStage`, `ShaderParameter`,
  `FramebufferTarget`, `FramebufferAttachment`, `RenderbufferFormat`). The
  contract never passes GL constants.
- Added `src/render/shader-program.h` and `shader-program-legacy.cpp`: the hot,
  game-owned shader program. The generation owns the program handle, source
  files, reload detection, and the `name -> location` cache. The EXE-side
  implementation uses the render-command EXE path and `CAP_GL_RESOURCE`
  deletion; the DLL implementation is added after the host shader capability
  lands. This is the critical path for the 93 per-frame uniform lookups.
- Appended the render hot-coverage worklist (family tables, gaps, owner split,
  batch order, evidence boundary) to
  `docs/specs/hotreload/hotreload.md` at the owner's explicit request. A's
  existing spec sections were not modified.
- Updated `docs/handoffs/hot-reload-agent-claims-20261113.md`: claimed
  `src/engine/**`, `src/render/**`, and `src/entities/player-render*.cpp` for
  Agent C, and recorded the A/C boundary and the render-contract handoff.

## Phase 1 render capability and skybox pilot (owner-directed)

- Added `src/host/abi/render-types.h`: the authoritative backend-neutral enum
  values shared by host and game. `src/render/render-types.h` now aliases them
  instead of defining a second copy.
- Extended `src/host/abi/render-capability.h` to version 2: semantic buffer,
  texture, vertex, shader, uniform, state, state-guard, and render-target
  operations. The host owns the mapping to the active backend; the game never
  passes GL constants.
- Extended `src/host/host-runtime.cpp`: semantic `toGl` mapping helpers, the new
  upload/shader/uniform/render-target thunks, a host-owned
  `pushState`/`popState` stack, and semantic-target handling in
  `renderSubmitThunk`. This is the resident half of the capability.
- Added the DLL side of the contract in `src/game/render-bridge-module.cpp`:
  all `mimita::game::render::` functions now forward to the capability. The
  whole-game DLL compiles and links it.
- Made `src/render/shader-program.cpp` shared (renamed from
  `shader-program-legacy.cpp`): the game generation owns the program, sources,
  reload detection, and uniform cache in both the EXE and the DLL.
- Migrated `src/render/skybox.{h,cpp}` as the Family A pilot:
  - the raw shader compile helpers and `glGetUniformLocation` calls are gone;
    `ShaderProgram` owns the program and caches uniform locations;
  - `Skybox::pollReload` calls `ShaderProgram::reloadIfChanged`, so editing
    `shaders/skybox.vert|frag` reloads live and a failed candidate keeps the
    previous program;
  - the manual GL state save/restore is replaced by `render::pushState` /
    `render::popState`;
  - cube-map upload, parameters, bind, draw, and attributes use the contract.
- `src/render/skybox.h` no longer uses `GLuint` or includes GL headers.

## Phase 1b world-shader relocation and world sweep (owner-directed)

- Added `src/render/world-shader.h/.cpp`: the game-owned main world shader.
  `WorldShader` owns a `ShaderProgram`, loads `shaders/basic.vert|frag`, reloads
  on file change, and owns the uniform cache. This implements the spec rule
  "the main world shader moves into the game generation".
- `src/engine/engine-tick-setup.cpp` now calls
  `worldShader().ensureLoaded()` / `reloadIfChanged()` at the frame boundary.
  The resident `Renderer::reloadWorldShaderIfChanged()` call is kept as a
  documented transition because `player-render`, `render-player`, and
  `engine-tick-render` still read the resident program. Remove it when those
  readers move.
- Fully swept the world-rendering unit:
  - `src/render/render-world.cpp`: world shader guard, `pushState`/`popState`
    around the proximity-fade blend, `render::bindVertexArray`/`useProgram`,
    and the shader id in the diagnostic.
  - `src/render/render-world-sky.cpp`: contract upload/bind/draw/state and
    cached world-shader uniforms.
  - `src/render/render-world-mesh.cpp`: contract upload/attributes/draw/state,
    `setUniforms(WorldShader&)` with cached locations, and the blend diagnostic
    derived from the owning config instead of `glIsEnabled`.
  - `src/render/render-world.h`: `setUniforms(WorldShader&)` and
    `renderWorldDepth(std::uint32_t)`.
- Migrated the shader/uniform family of the external world-shader callers:
  `src/physics/physical-entity.cpp`, `src/impact/destructible-render.cpp`, and
  `src/world/streamed-world-layer.cpp` now bind the game-owned world shader and
  use its cached uniform locations. Their bind/draw/state calls remain raw for
  the next family sweep. This fixed a real inconsistency where uniforms were
  looked up in the game-owned program while the resident program was bound.
- Discovered a new contract gap: `glDrawArraysInstanced` and
  `glVertexAttribDivisor` in `streamed-world-layer.cpp` have no contract ops.
  Recorded in the spec worklist; the instanced path stays raw until they land.
- Added `src/render/world-shader.cpp` and the new includes to the manifest.

## Phase 1c player-path shader migration (owner-directed)

- Fully migrated `src/engine/engine-tick-render.cpp`: replay-actor color
  overrides, impact-frame overlay, screen overlay, aftermath overlay, viewport,
  and clear now use the contract and the game-owned `WorldShader`; the file has
  no raw GL calls outside `glfw` window/input.
- Migrated the player path to the game-owned shader:
  - `Player::render` / `Player::renderCurrentPose` now take
    `mimita::game::render::ShaderProgram&`.
  - `drawCombinedBodyParts` (`player-render-mesh.cpp`) takes the same type.
  - `player-render.cpp` and `render-player.cpp` bind the game-owned world
    shader, use cached uniform locations, and pass the program object to the
    player renderer.
  - Added `WorldShader::programObject()` so a renderer that takes a
    `ShaderProgram&` can use the owned program without a second shader owner.
- The shader/uniform family is migrated in these files; their bind/draw/state
  calls remain raw for the next family sweep.
- Remaining resident-shader readers found after the sweep:
  `combat/revolver-system.cpp`, `combat/projectile-render.cpp`,
  `combat/weapon-viewmodel.cpp`, `render/procedural-cylinder.cpp`,
  `debug/debug-visuals-tris.cpp`, and `debug/debug-visuals-scene.cpp`. The
  resident `Renderer::shaderProgram` cannot be retired until they move.

## Phase 1d combat and primitive shader migration (owner-directed)

- Migrated the resident-world-shader readers in the combat and primitive paths
  to the game-owned `WorldShader`:
  - `src/combat/weapon-viewmodel.cpp`
  - `src/combat/projectile-render.cpp`
  - `src/combat/revolver-system.cpp`
  - `src/render/procedural-cylinder.cpp`
  Each now binds `worldShader()`, uses cached uniform locations, and no longer
  reads `Renderer::shaderProgram`. Bind/draw/state calls remain raw for the next
  family sweep.
- Fixed one compile blocker in Agent A's
  `src/game/server-command-dispatch-module.cpp:236`: the unqualified
  `getServerMatchMode()` is now `MimitaNet::getServerMatchMode()`. This was
  owner-directed because it blocked the whole-game DLL rebuild.
- The whole-game DLL generation 85 linked after that fix
  (`build/game/generation-85/game-g85.dll`, 1 compiled / 609 skipped,
  link 14.5 s).

## Phase 1e debug/effects/gui shader migration (owner-directed)

- Migrated the remaining debug/effects/gui resident-world-shader readers to the
  game-owned `WorldShader`:
  - `src/debug/debug-visuals-lines.cpp` (line, tri, and weapon-line batches)
  - `src/debug/debug-visuals-tris.cpp`
  - `src/debug/debug-visuals-scene.cpp`
  - `src/effects/effect-part-render.cpp`
  - `src/gui/gui-main.cpp` (player-preview lighting)
  Each binds `worldShader()`, uses cached uniform locations, and no longer reads
  `Renderer::shaderProgram`. Bind/draw/state remain raw for the next family
  sweep.
- After this sweep the only remaining resident-main-shader references are:
  - `src/shadow/shadow-render.cpp:420` (shadow path; needs a game-owned shadow
    program, not the world shader),
  - `src/debug/debug-diag-commands.cpp:144-182` (diagnostic readout; should read
    the game-owned program),
  - a commented-out printf in `src/render/render-player.cpp:532`.

## Phase 1f close the resident-main-shader readers (owner-directed)

- Migrated the last game-side resident-main-shader references:
  - `src/shadow/shadow-render.cpp` shadow-map overlay now uses the game-owned
    `WorldShader` and the contract (upload, bind, uniforms, draw).
  - `src/debug/debug-diag-commands.cpp` now reads
    `worldShader().program()` and uses `render::getProgramiv` /
    `getProgramInfoLog`.
- Removed the transitional `engine.renderer->reloadWorldShaderIfChanged()` call
  from `src/engine/engine-tick-setup.cpp`. The game-owned
  `worldShader().ensureLoaded()/reloadIfChanged()` is now the only world-shader
  reload path.
- After this change the only remaining `gRenderer->shaderProgram` in game code
  is a commented-out printf in `src/render/render-player.cpp:532`. Agent A can
  retire `Renderer::shaderProgram` creation and `reloadWorldShaderIfChanged`.
- The shadow program itself (`shadow-render.cpp` `gShadowShader`, created with
  raw GL) is still a separate owner and the next shader to move to a game-owned
  `ShaderProgram`.

## Phase 1g shadow, instanced, and post-FX ownership (owner-directed)

- Added the game-owned `src/shadow/shadow-shader.{h,cpp}`: a `ShaderProgram`
  that loads `shaders/shadow.vert|frag` with an embedded fallback, owns the
  uniform cache, and reloads on file change.
- Migrated `src/shadow/shadow-render.cpp` fully: the raw shadow program
  creation is gone, the shadow-map FBO creation is the only remaining raw GL
  (render-target family), and the depth paths/overlay use `shadowShader()` +
  the contract (`pushState` not needed here; explicit state calls moved to
  `render::`).
- Added `drawArraysInstanced` and `vertexAttribDivisor` to
  `render-command.h`, the EXE mapping, the DLL bridge, and the host submit
  switch. Migrated `src/world/streamed-world-layer.cpp` fully: template/record
  uploads, instance buffers, attributes/divisors, bind, draw, and state.
- Fully migrated `src/render/procedural-cylinder.cpp` (upload, push/pop state,
  bind, draw).
- Fully migrated `src/render/render-world-shadow.cpp` to
  `shadowShader().programObject()` with the contract.
- Migrated `src/render/post-fx.{h,cpp}`: `PostFX` now owns a `ShaderProgram`
  (`mPostShader`), the raw shader compiler is deleted, `setUniforms` uses the
  cached locations, and the FBO creation/bind/clear, quad upload, and draw use
  the contract render-target operations. The FBO handle types are now
  `std::uint32_t`.
- Evidence: EXE build `2026-11-14T00:39:41.789124Z` succeeded; whole-game DLL
  generation 97 linked (`build/game/generation-97/game-g97.dll`, 17 compiled /
  596 skipped, link 5.1 s).

## Phase 1h bind/draw/state sweep and shadow FBO contract (owner-directed)

- Fully migrated to the contract (0 raw GL calls):
  - `src/debug/debug-visuals-lines.cpp` (line and weapon-line upload, state,
    bind, draw)
  - `src/combat/weapon-viewmodel.cpp` (weapon draw)
  - `src/combat/revolver-system.cpp` (mesh upload and draw)
  - `src/impact/destructible-render.cpp` (upload, `pushState`/`popState`,
    bind, draw)
- Extended the render contract for the shadow FBO:
  - `TextureFormat::Depth24`, `TextureParameter::BorderColor`,
    `TextureValue::ClampToBorder`, `FramebufferBuffer` (`None`/`Color0..3`).
  - `texParameterfv`, `drawBuffer`, `readBuffer` in the contract, EXE mapping,
    DLL bridge, and host submit switch.
  - `src/shadow/shadow-render.cpp` now creates and binds the shadow FBO through
    the contract; it has 0 raw GL calls.
- CAP_WORLD: Agent A already owns and wires
  `src/game/world-bridge-module.{h,cpp}` (`initializeHostWorld`,
  `readWorldInfo`, `castHostWorldRay`, `pickHostSpawn`). A duplicate
  `src/world/world-query.*` was created and then removed to keep one owner. The
  remaining work is migrating world-query call sites to A's bridge.
- Evidence: EXE build `2026-11-14T05:48:33.339482Z` succeeded; whole-game DLL
  generation 102 linked (`build/game/generation-102/game-g102.dll`,
  0 compiled / 615 skipped, link 6.8 s).

## Phase 1i remaining small-file sweep (owner-directed)

- Fully migrated to the contract (0 raw GL calls):
  - `src/debug/debug-visuals-tris.cpp` (tri upload, state, draw)
  - `src/combat/projectile-render.cpp` (mesh upload, model/fill draw,
    push/pop state)
  - `src/effects/effect-part-render.cpp` (mesh upload, model draw, textured
    hit particles, billboard depth state via `pushState`/`popState`)
- Evidence: EXE build `2026-11-14T12:55:28.976130Z` succeeded; whole-game DLL
  generation 111 linked (`build/game/generation-111/game-g111.dll`,
  0 compiled / 615 skipped, link 13.4 s).

## Phase 1j player mesh sweep (owner-directed)

- Fully migrated `src/entities/player-render-mesh.cpp` to the contract (0 raw
  GL calls): capsule/body-part/render/combined uploads, attributes, combined
  body-part bind/cull/draw, and the handle types are now `std::uint32_t` /
  `std::int32_t`.
- Evidence: EXE build `2026-11-14T13:09:43.795325Z` succeeded; whole-game DLL
  generation 115 linked (`build/game/generation-115/game-g115.dll`,
  0 compiled / 615 skipped, link 15.5 s).

## Phase 1k render lane complete (owner-directed)

- Fully migrated the last four render-lane files to the contract:
  - `src/entities/player-render.cpp` (body-part/extra-part draw, outline/cull,
    render-mesh and capsule paths, shadow depth, `pushState`/`popState`)
  - `src/entities/player-render-mesh.cpp` (already done in Phase 1j)
  - `src/physics/physical-entity.cpp` (box, sphere, and prop-model paths,
    `pushState`/`popState`, uploads)
  - `src/render/render-player.cpp` (blend/outline/capsule/wireframe guards,
    trail batch, `pushState`/`popState`)
- Result: **the entire render lane has 0 raw GL calls** outside `glfw`
  window/input. Every shader program is game-owned (`WorldShader`,
  `ShadowShader`, `PostFX`, skybox).
- Evidence: EXE build `2026-11-14T13:54:46.160916Z` succeeded.
- Whole-game DLL compile is blocked by Agent A's in-progress
  `src/game/game-input-module.cpp:83` (`event.paths` is
  `const char (*)[260]` but `handleGameInputDrop` takes `const char**`). That
  file is A's lane; the last successful DLL remains generation 115.

## Phase 1l upload and mipmap sweep (owner-directed)

- Extended the render contract for texture uploads:
  - `PixelFormat::RG`, `PixelStoreParameter`, `pixelStorei`, and
    `generateMipmap(TextureTarget)` across the contract, EXE mapping, DLL
    bridge, and host submit switch.
- Fully migrated:
  - `src/combat/weapon-model-cache.cpp` (weapon texture upload, mesh upload)
  - `src/avatar/avatar-atlas.cpp` (all three atlas texture uploads)
- Evidence: EXE build `2026-11-14T14:18:20.275535Z` succeeded; whole-game DLL
  generation 130 linked (`build/game/generation-130/game-g130.dll`,
  54 compiled / 562 skipped, link 23.4 s).

## Phase 1m map/player/replay upload and target sweep

- Spec: recorded the **world-ownership decision** (game DLL owns the client
  `World`; `CAP_WORLD` stays the server `HeadlessWorld` surface; client world
  queries stay in the DLL) in `docs/specs/hotreload/hotreload.md`.
- Contract: added `TextureFormat::RGBA8` (EXE mapping and host mapping).
- Fully migrated (0 raw GL):
  - `src/map/texture.cpp` (fallback + `loadTexture` upload)
  - `src/entities/player-loader.cpp` (white texture + glTF image upload)
  - `src/replay/replay-export-target.cpp` (offscreen color texture + depth
    renderbuffer + completeness check)
- `src/map/map_render.cpp` counted 26 raw-GL "sites" but they are all
  commented-out dead code; no active GL there.
- Evidence: EXE build `2026-11-14T14:41:21.629869Z` succeeded.

## Boundary findings (source-checked)

- `renderContext` is the host `Engine*`: `src/main.cpp:550` calls
  `driveFrame(1.0 / 60.0, &engine)`. It is not the `Renderer*` that
  `CAP_RENDER_DEVICE::acquire` returns, although both point at the same
  resident renderer.
- The host only takes over the frame when
  `HostRuntime::ownsGameFrame()` is true: `MIMITA_GAME_OWN_FRAME=1`,
  `api->frameCapable != 0`, and both `tick` and `render` are present
  (`src/host/host-runtime.cpp:1726-1732`).
- Agent A wired `src/game/game-module.cpp` to `whole-game-entry` at 08:29 and
  set `api.frameCapable = 1`, so the host can now take the frame when the flag
  is set.
- `CAP_RENDER` now has host-executed buffer/texture/attribute/uniform/draw ops,
  but migrating game-owned raw-GL call sites needs either resident legacy twins
  (A-owned) or a static-gate exemption. C did not start that migration to avoid
  an EXE link break without a build.

## Build evidence

- Before the frame split: `source_manifest.py --write` reported revision
  `9654259f37fc8012b2e4381c957e53a7b2e997560f17d3ce9541a1f4c860e8ea`,
  total 772 sources (game 605, resident 28, bootstrap 4).
- Before the frame split: `game_static_gate.py` reported 0 violations / 0
  files.
- Before the frame split: the dev-loop published
  `2026-11-13T13:27:58.826703Z` and the EXE contained `tickWholeGame`,
  `FixedTickClock`, and `whole-game-entry`, proving those units compiled.
- The dev-loop published build `2026-11-13T14:36:35.301681Z`
  (EXE `.dev/builds/2026-11-13T14-36-35.301681Z/mimita.exe`). The published EXE
  contains `ShaderProgram`, `pushState`, `popState`, and
  `FramebufferAttachment`, and the build log shows
  `src/render/render-command-legacy.cpp` and
  `src/render/shader-program-legacy.cpp` compiled. This is compile evidence for
  the frame split and Phase 0, not runtime evidence.
- A transient build failure during the same window was
  `src/main-systems.cpp:706` and `:923` `error: 'gameState' is not captured`
  (Agent A's file). Agent A fixed it and the next build succeeded; it was not
  caused by the render files.
- Phase 1: the dev-loop published build `2026-11-13T15:03:50.413680Z` after the
  capability, bridge, and skybox pilot compiled in the EXE.
- Phase 1: `python devscripts/game_build_worker.py` built and linked the
  whole-game candidate. Generation 79 first linked the bridge and migrated
  skybox (49 compiled / 560 skipped, link 8.7 s). Generation 80 rebuilt
  `skybox.cpp` with the live shader-reload check (1 compiled / 608 skipped,
  link 9.5 s, `build/game/generation-80/game-g80.dll`). Generation 82 linked
  the world-shader relocation and world sweep (37 compiled / 573 skipped,
  link 9.3 s, `build/game/generation-82/game-g82.dll`). This proves the DLL
  bridge, the migrated skybox, and the world-rendering unit link into
  `game.dll`; it does not prove runtime behavior.
- Phase 1b: the dev-loop published build `2026-11-13T15:44:44.905606Z` after the
  world-shader relocation and world sweep compiled in the EXE.
- Phase 1c: the dev-loop published build `2026-11-13T23:05:53.756786Z` after the
  engine-overlay and player-path shader migration compiled in the EXE.
- Phase 1c whole-game DLL rebuild was blocked by Agent A's in-progress
  `src/game/server-command-dispatch-module.cpp:236`
  (`getServerMatchMode` missing its `MimitaNet::` qualifier). That file is A's
  lane; the last successful whole-game DLL remains generation 82. This is a
  compile blocker outside C's changes, not a render-link failure.
- Phase 1d: C fixed that one-line blocker and the whole-game DLL generation 85
  linked (`build/game/generation-85/game-g85.dll`, 1 compiled / 609 skipped,
  link 14.5 s).
- Phase 1d: the combat/cylinder sources compiled in the EXE build log
  (`projectile-render.cpp`, `revolver-system.cpp`, `weapon-viewmodel.cpp`,
  `procedural-cylinder.cpp`). The EXE build is currently blocked by Agent A's
  in-progress `host-runtime.h` / `main-systems.cpp` state refactor
  (`MenuStateV1` not a type, `HostRuntime::menuState` missing). Those are A's
  files and A is actively editing them.
- Phase 1e: the debug/effects/gui migration is in source; compile verification
  is pending the same A-owned host build.
- Phase 1f: the dev-loop published build `2026-11-14T00:18:31.902187Z` after the
  debug/effects/gui, shadow-overlay, diagnostic, and transitional-reload changes
  compiled in the EXE.
- Phase 1f whole-game DLL rebuild compiled all 613 sources but **failed at link**:
  `src/engine/whole-game-entry.cpp:32` references
  `gameInit(int, char**, Engine&)`. That symbol is defined in
  `src/main-init.cpp`, whose manifest owner is `bootstrap` ("process entry and
  orchestration; must stay resident"), so it is not part of `game.dll`. The
  last successful whole-game DLL remains generation 85. This is an A-owned
  initialization-boundary decision, not a render-link failure: the DLL needs a
  game-owned initialization path (or a host bridge) instead of calling the
  bootstrap `gameInit` directly.

## Runtime evidence

- None. No process was launched, no generation was loaded, and no
  `events.jsonl` was read. Compile/link evidence is not behavior evidence.

## Not proved

- The host has not been observed taking the frame with `MIMITA_GAME_OWN_FRAME=1`.
- The migrated skybox and world-rendering unit have not been run or visually
  inspected; render parity, shader reload, and the state guard are unverified at
  runtime.
- `player-render`, `render-player`, and `engine-tick-render` now use the
  game-owned world shader for the shader/uniform family, but the resident
  `Renderer::shaderProgram` is still read by combat/debug/primitive files, so it
  is not retired yet and the transitional resident reload remains.
- `physical-entity`, `destructible-render`, `streamed-world-layer`,
  `player-render`, `render-player`, and `player-render-mesh` still use raw
  bind/draw/state calls (shader/uniform family only). The instanced chunk path
  has no contract ops yet.
- The latest successful whole-game DLL is generation 111; earlier generation
  blockers are resolved.
- Swap hardening (per-generation GL registry, `gameQuiesce` Busy,
  `gameShutdown` release, pending-render drain) is not implemented.
- No same-PID generation swap, state continuity, render parity, VSync
  observation, or human visual acceptance.

## Next

1. Continue the remaining non-render raw-GL sweep: `avatar/cosmetic-system`
   (44), `map/map_render` (26), `replay/replay-export` (22),
   `replay/replay-export-target` (15), `map/map-loader-material` (16),
   `entities/player-loader` (14), `map/texture` (14), and the smaller
   debug/game-cli/network/perf/notifications files.
2. Agent A retires `Renderer::shaderProgram` creation and
   `reloadWorldShaderIfChanged`; game code no longer reads it.
3. Agent A: swap hardening (per-generation GL registry, `gameQuiesce` Busy,
   `gameShutdown` release, pending-render drain).
4. Runtime proof: whole-game own-frame, then save → rebuild → same-PID swap with
   continuity.
5. Extend the static gate to ban raw `gl*` and finish the ownership cutover.
6. World stays DLL-owned: do not add a host client-world adapter or migrate
   client `World` queries to CAP_WORLD.

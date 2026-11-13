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
  full GL-shaped surface (bind, state, upload, draw, uniforms, resident shader
  service, queries) and is the authoritative missing-operation list for
  `CAP_RENDER`. `render-command-legacy.cpp` is `#ifndef MIMITA_GAME_DLL` and
  forwards every helper to raw OpenGL, so the resident EXE path is unchanged
  and no call sites move yet. The DLL build compiles it as an empty TU and
  references no missing capability symbols, so the whole-game link cannot break
  from this change.
- Updated `docs/handoffs/hot-reload-agent-claims-20261113.md`: claimed
  `src/engine/**`, `src/render/**`, and `src/entities/player-render*.cpp` for
  Agent C, and recorded the A/C boundary and the render-contract handoff.

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
- **The frame split edits (`engine-tick.h`, `engine-tick.cpp`,
  `whole-game-entry.*`) were not compiled.** The owner instructed this session
  to make code changes only and not run builds, so this is unverified source,
  not build evidence.

## Runtime evidence

- None. No process was launched, no generation was loaded, and no
  `events.jsonl` was read. Compile/link evidence is not behavior evidence.

## Not proved

- The split has not been compiled or run.
- The host has not been observed taking the frame with `MIMITA_GAME_OWN_FRAME=1`.
- No same-PID generation swap, state continuity, render parity, VSync
  observation, or human visual acceptance.

## Next

1. Compile the split and fix any errors.
2. Agent A implements the `CAP_RENDER` operations declared in
   `src/render/render-command.h` (state, texture targets, FBO/renderbuffer,
   extra uniforms, queries, resident shader/uniform-location service) and
   hardens the swap (per-generation GL registry, `gameQuiesce` Busy,
   `gameShutdown` release, drain `renderPending_`).
3. Agent C adds the `MIMITA_GAME_DLL` bridge for the contract and migrates the
   draw/bind/upload/uniform family in batches.
4. Launch the whole-game candidate with `MIMITA_GAME_PROBE=1`,
   `MIMITA_GAME_OWN_FRAME=1`, and `MIMITA_GAME_DLL` set; read the exact
   `events.jsonl` for `game.frame.tick-failed` / `game.frame.render-failed` and
   the probe start event.
5. Edit one game-owned source, rebuild the candidate, and prove the same-PID
   swap with world/player/NPC continuity at the tick boundary.

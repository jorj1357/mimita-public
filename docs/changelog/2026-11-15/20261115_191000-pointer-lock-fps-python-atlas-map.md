# Pointer lock, always-on FPS, Python window suppression, NPC atlas retry, map base recovery

Date: 2026-11-15
Requested by: jorj1357
Agent: DeepSeek v4.1 Flash

## Source changes

- `src/camera.h`, `src/camera.cpp`: added `ignoreHostMouse` so the router can own
  look while pointer lock is released.
- `src/engine/engine-tick-camera.cpp`: removed the per-frame firstMouse reset and
  the freecam GLFW cursor-mode override; lock transitions now reset firstMouse
  once, call `InputRouter::setPointerLock`, and log `input.pointer-lock`.
- `src/input/input-router.h`, `src/input/input-router.cpp`: added Win32 cursor
  clipping for gameplay lock, mouse injection for agent attack, agent look
  bypass of the host-mouse throttle, and kept normalized look in the router.
- `src/input/mouse-lock.cpp`: cursor visibility now uses the resident
  `setHostCursorVisible` capability; the router only clips the cursor.
- `src/npc/npc-avatar.cpp`: NPC avatar assignment now uses the resident
  renderer as the GL-availability signal instead of the hot DLL's static GLFW.
  This was preventing NPC avatar and animation binding entirely in the hot
  client.
- `src/entities/player-loader.cpp`: the shared white fallback texture now uses
  the resident renderer instead of the static GLFW context check.
- `src/gui/hud/chat-window.cpp`, `src/main-init.cpp`,
  `src/input/input-commands.cpp`: chat open/close uses `MouseLock`/host cursor
  visibility; key callbacks pass their actual window to chat text input.
- `src/network/server-npcs.cpp`: added a bounded last-resort root push for NPCs
  still inside a closed solid after the normal containment pass, plus a
  rate-limited `npc.collision.stuck` event with contact/penetration evidence.
- `src/network/server-gamemode.cpp`: added `payload.cart-spawn-failed` evidence
  when the cart physical entity cannot be created.
- `src/input/mouse-lock.cpp`: lock changes now delegate cursor ownership to the
  router instead of the game DLL's static GLFW.
- `src/main-systems.cpp`: added `agent.input shoot`.
- `src/engine/engine-tick.cpp`: L now reads `InputRouter::keyDown` instead of the
  DLL-local GLFW key state.
- `devscripts/dev-loop.py`: command requests without an explicit client id now
  use the primary client's recorded queue path, not only the legacy run-level
  path.
- `src/perf/perf.cpp`: added an always-on `performance.fps` event every one
  second with FPS, frame quantiles, budget, NPC count, and effect count.
- `devscripts/dev_loop/cli.py`, `devscripts/dev-loop.py`: Windows Terminal
  relaunch is now explicit (`--relaunch-terminal`), an existing dashboard PID
  prevents a second window, and the CLI control/command path no longer starts a
  dashboard during daemon recovery.
- `src/avatar/avatar-atlas.cpp`, `src/entities/player-render.cpp`,
  `src/entities/player.h`: retry atlas finalization at the visible render
  boundary when the async model/avatar path left the NPC atlas at zero; log
  `avatar.atlas.finalize.failed` instead of silently falling back. The deeper
  root cause was the hot DLL's static GLFW reporting no context; avatar GL
  availability now uses the resident `Renderer*` (`gRenderer`).
- `src/map/map-authored-geometry.cpp`: reconstruct pristine GLB object bases
  after a hot generation swap, using the inverse of the currently applied
  transform, so authored overrides are reapplied idempotently. The public
  two-argument entry point was preserved for the map editor and self-test.

- `src/host/abi/window-capability.h` + `HostCapabilityId::Window`: new resident
  window/input capability for size, framebuffer size, position, cursor, focus,
  iconified, cursor mode, mouse/key state, and bounded clipboard copy.
- `src/host/host-runtime.cpp`: resident implementation using the host-owned
  window, registered as capability 27; installs a resident cursor callback and
  coalesces cursor events before dispatching them to the game.
- `GameInputEventV1` now carries `Cursor` position events (version 2), and
  `game-input-module.cpp` feeds them into `InputRouter`.
- `InputRouter` now owns mouse look: it applies cursor deltas only while pointer
  lock is active, disables the Win32 fallback after the first host cursor event,
  and resets its baseline on lock/focus transitions.
- `src/game/window-bridge-module.{h,cpp}`: game-side capability/facade with
  `InputRouter` fallbacks, initialized from `game-module.cpp`.
- GUI/editor/chat/notification/avatar-editor paths now use that facade instead
  of passing a resident GLFW window into the DLL-local GLFW. This includes
  cursor position, window/framebuffer size, mouse/key state, cursor mode,
  focus/iconified checks, and clipboard.
- `src/world/world.h` + `src/map/map_common.h`: resident `AuthoredGLBBase`
  storage keyed by GLB object id. Authored overrides prefer these pristine bases
  across hot generation swaps, so the inverse-transform recovery is only a
  legacy fallback.
- `config/game-static-gate.json`: added the hot-window GLFW rule; all game
  sources pass. The three explicit exceptions are documented in
  `docs/specs/input/host-window-capability.md`.
- Remaining game-owned key/cursor/window probes were routed through
  `InputRouter` or `game/window-bridge-module`.

## Documentation

- `docs/specs/dev-loop/dashboard.md`: one dashboard per checkout; no implicit
  Python/Terminal popup.
- `docs/specs/dev-loop/build-time-observability.md` and new
  `docs/architecture/live-development/incremental-build-and-header-fanout.md`:
  header fan-out and explicit cold-object deletion rules.
- `docs/specs/performance/performance.md`: 1 Hz `performance.fps` and tradeoff.
- `docs/specs/input/input-router.md`: L pointer lock, cursor clipping, and
  single look-owner rule.
- `docs/specs/debug-logging/debug-logging.md`: `performance.fps` catalog entry.
- `docs/specs/input/host-window-capability.md`: resident window/input boundary
  and the no-resident-GLFW-pointer rule.
- `docs/architecture/live-development/resident-authored-glb-bases.md`: why GLB
  bases live in the resident `World` and how hot swaps use them.

## Source/build evidence

No build was invoked for the final renderer/NPC-avatar, L/queue, cursor
ownership, host-window event, static-gate, or resident-GLB-base changes; they
are waiting for the other agent's cold/game build. Earlier
generation evidence below is from before that request.

- Game generation 215 published and activated in PID 24736 at 19:54:43.
- Generation 217 later published and was active in PID 10640 at 20:04:40.
- Generation 218 published `src/entities/player-render.cpp` at 20:07:09 with the
  success-gated atlas retry; a new dev-loop run (`20261115_200646`) was active
  by 20:09 but had not yet logged the g218 activation.
- Generation 218 predates the later `gRenderer` context-detection fixes in
  `npc-avatar.cpp`/`player-loader.cpp`, the chat/cursor ownership changes, the
  NPC collision safety pass, and the L/queue fixes; those require a following
  generation.

- The map recovery code applied `map-authored-geometry.applied` for
  `funworld3`: 128088 render vertices, 44016 collision triangles, and one GLB
  override.
- A generation 216 link artifact exists, but generation 217 is the active
  generation observed in the journal.
- The parallel cold-build work owned by the other agent was active throughout;
  cold and game build statuses repeatedly interleaved.
- Same-PID hot activation was lost when a separate `restart-instance` request
  restarted `client-01`; PID 24736 became PID 26308 and then PID 10640. This was
  not a source-change build requirement.

## Runtime evidence

- `performance.fps` was observed in the live client journal, for example:
  `fps`, `avg_frame_ms`, `p50/p95/p99_frame_ms`, `frame_budget_ms`,
  `frames_over_budget`, `npcs_active`, and `effects_alive`.
- At normal window state `performance.fps` reported 85.9, 84.9, 108.9, 101.7,
  and 94.3 FPS in consecutive one-second windows. While the window was
  iconified during a build it reported roughly 0.5–1.5 FPS. This proves the
  event is emitted; it is not an FPS acceptance result.
- The old PID 24736 reached generation 215, then a dev-loop `restart-instance`
  request at 19:57:48 replaced it with PID 26308 and later PID 10640. The run and
  journal remain the same, but the original PID was not preserved.
- Generation 217 logged `avatar.atlas.finalize.failed` with
  `gl_context=false`, confirming the deferred-finalize diagnosis. Generation 218
  contains the follow-up render-boundary retry that waits for a successful
  finalize instead of marking the attempt before the GL context exists; it was
  published but not yet observed active.
- `manual-13eec687` queued `agent.input look 120 0`; no
  `dev-loop.command.delivered` or `camera.look-input` was observed from it.
- No live `input.pointer-lock` or NPC atlas success event was observed yet.

## Pending

- Re-run pointer-lock/look through the correct client command queue and compare
  yaw on tick N/N+1.
- Observe `performance.fps` at normal window state and normal FPS.
- Build/activate the render-boundary atlas retry and observe
  `avatar.texture.bind` for NPCs.
- Observe `map-authored-geometry.applied` and GLB positions on the next
  generation swap.
- Verify `--command`, `--status`, and `--ensure-running` create no dashboard or
  Python/Terminal window after the control-path change.
- Physical mouse look, shooting, cart visibility, NPC collision, authored NPC
  animation, and freeze/down-dash effects still require live acceptance.

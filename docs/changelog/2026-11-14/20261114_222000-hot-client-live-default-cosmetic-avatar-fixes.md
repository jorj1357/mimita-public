# Hot client live default, frame stabilization, cosmetic and NPC avatar fixes

# Task

- Task ID: agent-c-20261115-hot-client-live
- Owner: agent-c (with hot-client build/runtime evidence from the root dev-loop)
- Focus: hot-client live default, frame stall, viewport, cosmetic rendering, NPC avatar names
- Status: implementation built; hot client live activation observed; visible world still unresolved
- Date, time, timezone: 2026-11-14T22:20:00-05:00 (2026-11-15T03:20Z)
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `2f2cd0ff277272717187e67923ead9abe2d113fa`

# Pre-existing changes

`git status` contained changes from concurrent work, including dev-loop
scripts, packet schema files, server runtime, hot-reload docs, and the earlier
state-domain/PostFX changelog. This record claims only the files and behavior
listed below. It does not claim the earlier packet-split or client-retirement
work.

# Requested behavior

- Make the full client use the hot path by default immediately.
- Fix the hot-client frame stall and 1x1 viewport so the client can be played
  while code is fixed live.
- Add diagnostics for the frame, cosmetic, and NPC-avatar paths.
- Fix black cosmetic geometry, especially the Jason mask.
- Ensure NPCs can resolve their avatar names.
- Write the Phase 0-3 live-fix plan in the appropriate specification.

# Exact implementation changes

## File: `docs/specs/hotreload/hot-client-live-fix.md`

- New desired-behavior specification for Phases 0-3:
  - Phase 0: full client uses the published generation by default.
  - Phase 1: stable hot-client frame, no main-thread file polling, no 1x1
    viewport, visible fallback.
  - Phase 2: same-PID edit/build/activate/rollback while playing.
  - Phase 3: live cosmetic and avatar fixes, including untextured cosmetic
    fallback and NPC avatar resolution.
- Linked from `docs/specs/hotreload/hotreload.md`.

## File: `src/main.cpp`

Before:

```cpp
// Hot-client command queue polled with file_size/last_write_time on every loop.
processAgentCommandQueueForDll(...);
```

After:

```cpp
// A resident background watcher reads appended queue lines.
// The hot main thread only drains an in-memory deque.
```

- Removed per-frame filesystem stat/open/read from the hot main thread.
- The watcher uses a 100 ms file-signature poll on a detached resident thread.
- Main-thread command execution remains ordered and unchanged.
- The normal client already configured the hot frame by default in
  `configureWholeGameHost`; this was verified rather than re-added.

## Files: `src/renderer/renderer.cpp`, `src/render/post-fx.cpp`

- `Renderer::beginFrame` no longer converts a zero-size framebuffer to 1x1.
  It retains the last valid presentation size and skips the frame only when no
  valid size exists.
- Added rate-limited `renderer.frame-size` logging with framebuffer,
  presentation, scene, visibility, and iconified state.
- `PostFX::initFBO` rejects non-positive dimensions.
- `PostFX::bindFBO` refuses degenerate scene sizes and binds the default
  framebuffer.
- `PostFX::render` binds the default framebuffer when shader/quad resources are
  invalid.
- If the scene FBO exists but the post shader/quad is invalid, the scene is
  blitted to the default framebuffer instead of leaving a black image.

## File: `src/gui/ui-system.cpp`

- `uiBeginFrame` falls back to the renderer width/height when the window
  framebuffer size is zero or the window handle is unavailable.
- Added rate-limited `ui.framebuffer-size` logging.

## File: `src/avatar/cosmetic-system.cpp`

- Cosmetic texture paths now use the owning player's avatar name instead of the
  local client's current avatar.
- Untextured/missing-texture cosmetics now use a solid color path:
  - `uUseColor = 1`
  - `uColor` from the slot color/texture color
  - no texture bind
- Added rate-limited `cosmetic.render` logging with avatar, GLB, anchor,
  texture path, texture id, fallback flag, color, brightness, opacity, and UV
  transform.
- `uUseColor` is reset after the cosmetic pass.

## Files: `src/avatar/avatar.h`, `src/avatar/avatar-atlas.cpp`

- Added `AvatarSystem::resolveAvatarName`.
- Truncated `remote_<prefix>` names resolve to a unique local avatar folder.
- Added `npc.avatar.resolve` logging for successful or ambiguous resolution.
- Async avatar application now stores absolute cosmetic image paths using the
  resolved avatar base path.

## Files: `src/network/schema/packet-common.h`, `packet-snapshot.h`

- `MAX_AVATAR_NAME_BYTES`: 24 -> 32.
- `CompactEntityData`: 179 -> 187 bytes.
- `SnapshotChunkPacket`: 1106 -> 1154 bytes, still under the 1200-byte limit.
- `server.npc-avatar.snapshot-summary` now records the wire avatar limit.

## Files: `src/host/abi/game-host-abi.h`

- `EntitySnapshotV1.avatarName`: 24 -> 32 bytes.
- `kGameHostAbiVersion`: 1 -> 2.

# Build evidence

- EXE artifact `2026-11-15T03:33:35.573919Z` built successfully.
- Whole-game generation **170** built successfully with the hot queue, UI, and
  cosmetic/avatar changes.
- Whole-game generation **171** built successfully with the PostFX fallback.
- Whole-game generation **172** built successfully with PostFX pass diagnostics.
- Whole-game generation **173** built successfully with `postfx.fbo-probe`.
- The following EXE build failed for an environment reason, not a source
  diagnostic: `No space left on device` while writing
  `build/obj-debug/main-systems.o`. C: reported 0 bytes free.
- Removed 169 old `build/game/generation-*` directories below generation 170,
  freeing 17.87 GB. C: then had about 25 GB free.
- A fresh EXE rebuild was requested and was still compiling when this record was
  written.
- Build success proves compilation/linking only.

# Runtime evidence

Run `20261115_030654`, client PID `22488`:

- Loaded generation 169 with ABI 2.
- `game.render.path: dll`.
- `renderer.frame-size`: 1024x768 framebuffer, presentation, and scene.
- `ui.framebuffer-size`: 1024x768 resolved.
- `presentation.framebuffer_probe`:
  - startup: 1024x768, non-black 66,907
  - second startup: 1024x768, non-black 76,005
  - late: 1024x768, non-black 97,572
- Frame count exceeded 4,500; the prior frame-1 stall no longer stops the hot
  client.
- Live same-PID activation observed across multiple generations:
  - 169 -> 170 in PID `22488`
  - 170 -> 171 in PID `22488`
  - 171 -> 172 in PID `27240`
- The client remained connected and continued rendering after activation.
- Generation 172 diagnostics show a valid PostFX state:
  - mode full, bypass false
  - FBO 3, color texture 38, quad VAO 36
  - shader valid, program 21
  - FBO and viewport 1024x768
- Generation 173 adds an FBO center-pixel probe before PostFX presentation, but
  no client was running generation 173 before the disk filled, so that result
  is not yet captured.

# Not proved / unresolved

- The live hot client still presents a black or HUD-only world in the captured
  framebuffer probe. HUD pixels are visible, but the world is not proven
  visible. This remains open and is likely in the PostFX/world presentation
  path, not the frame loop.
- No human visual acceptance of the Jason mask, cosmetics, or NPC avatars yet.
- No runtime proof that all NPCs resolve and load avatars.
- No steady-state FPS acceptance; current frame budget remains over target.
- No rollback test after a deliberately failed generation.

# Regression records

- `docs/regressions/gameplay/black-cosmetic-glb-REG-2026-11-15.md`
- `docs/regressions/gameplay/npc-avatar-name-truncation-REG-2026-11-15.md`
- `docs/regressions/performance/fps-frame-budget-REG-2026-11-15.md`

# Next exact step

Determine why the hot client's world pass is still black/HUD-only after the
PostFX fallback. Capture the PostFX shader/FBO state and PostFX pass completion,
then verify a visible world before claiming Phase 1 complete.

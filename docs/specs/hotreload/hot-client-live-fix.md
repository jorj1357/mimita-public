# Hot Client Live-Fix Phases 0-3

## Purpose

This specification defines how the full client becomes the primary live-editing
surface: the client owns its frame from `game.dll`, survives generation swaps,
and accepts cosmetic, avatar, and presentation fixes without closing the process.

It is a desired-behavior specification, not a status report. Current build/run
facts belong in generated status and changelogs.

## Phase 0 - Hot client is the default

The normal full client loads the published `game.dll` and owns its frame by
default. It does not require an opt-in environment flag.

Rules:

- If a valid published generation exists, the full client loads it.
- The full client requests frame ownership immediately.
- `MIMITA_GAME_DISABLE=1` remains an explicit emergency fallback only.
- If no valid generation exists, the resident debugging path is used and the
  fallback is journaled.
- The server continues to use the same replaceable generation model.
- The resident host keeps process lifetime, window/GL context/device, logging,
  input/network primitive pumps, loader/barrier, and resident state storage.

Acceptance:

- A normal launch reports `game.render.path: dll`.
- The same EXE PID serves the client frame and generation activation.
- No separate opt-in flag is required for normal client play.
- A missing candidate fails closed to the previous generation or the resident
  path, never to a half-loaded frame.

## Phase 1 - Stable hot-client frame

The client frame must remain live and visible while edits, builds, activation,
and rollback happen.

Rules:

- No filesystem stat, open, or read runs on the hot main thread.
  - Dev-loop command files are watched and read by the resident host.
  - Parsed commands cross to the main thread through a bounded in-memory queue.
- A zero-size or minimized framebuffer never becomes a `1x1` viewport or FBO.
  - `Renderer` retains the last valid presentation size.
  - PostFX skips degenerate targets and binds the default framebuffer.
- A missing/invalid PostFX shader, FBO, or quad must not leave a scene FBO bound.
- The probe records enough evidence to distinguish:
  - queue/loop stall,
  - zero-size window,
  - PostFX failure,
  - world/menu not yet loaded,
  - render/swap failure.
- First frame may be a loading frame, but it must not be presented as an
  unexplained permanent black frame.

Required diagnostics:

- `renderer.frame-size`
- `host.loop.phase`
- `game.frame.complete`
- `game.render.path`
- `presentation.framebuffer_probe`

Acceptance:

- Visible hot client completes more than 1200 frames.
- Viewport matches the real window/scene size.
- Startup and late probes contain non-black pixels.
- No stall watchdog after frame 1.
- Client remains connected while playing.

## Phase 2 - Same-PID live fix

Once the hot client frame is stable, a normal gameplay presentation fix must be
editable and visible in the same process.

Rules:

- The live loop follows `hotreload.md`:
  source edit -> candidate build -> validation -> publish -> activation at a
  safe boundary -> exercise -> journal -> accept/reject/rollback.
- The EXE PID does not change.
- The active game session/world is preserved where the changed behavior allows.
- A failed build, validation, migration, or activation leaves the previous
  generation active.
- Old generations are quiesced and unloaded without old callbacks, jobs,
  textures, VAOs, VBOs, or return addresses entering them.
- Cosmetic and avatar state that must survive a swap lives in resident domain
  records or is rebuilt safely by the new generation.

Acceptance:

- Edit a cosmetic/avatar presentation rule while connected.
- Request a generation build through the root dev-loop.
- Observe activation in the same PID.
- Observe the changed visual result without relaunching.
- Force a failed build and observe the old generation remain active.
- Roll back deliberately and observe the previous behavior return.

## Phase 3 - Cosmetic and avatar live fixes

After Phases 0-2 pass, cosmetic and actor-avatar behavior is fixed through live
generations.

Rules:

- Cosmetic texture resolution uses the owning actor's avatar, not the local
  client's avatar.
- An untextured cosmetic uses a visible solid-color fallback instead of sampling
  a missing/default texture.
- Missing textures, bad UV transforms, and invalid GLB materials are journaled
  and never render as unexplained black geometry.
- Local first-person and third-person visibility rules follow the actor body
  rules; a hidden head must not leave a head cosmetic in front of the camera.
- NPCs receive an avatar:
  - wire names must be long enough for `remote_<id>` names;
  - truncated names resolve to the unique full local avatar folder while the
    wire format is transitional;
  - an NPC with no authored avatar receives a deterministic fallback.
- Cosmetic rendering remains rate-limited and bounded; no per-frame filesystem
  work or unbounded logging.

Required diagnostics:

- `cosmetic.render`
- `cosmetic.texture.missing`
- `cosmetic.fallback.solid`
- `npc.avatar.wire`
- `npc.avatar.resolve`

Acceptance:

- Jason mask renders visibly for local and remote actors.
- A payload map with many NPCs does not produce many black cosmetic objects.
- NPCs visibly load avatars where an avatar is available.
- The fix is proven in a visible hot client with the same PID.

## Evidence

A phase is not complete because code compiles. Each phase requires:

- exact source revision and changed files;
- candidate generation and DLL;
- unchanged EXE PID;
- activation/rollback journal events;
- relevant `events.jsonl` records;
- before/after behavior;
- human visual acceptance for presentation changes.

Build evidence, runtime evidence, and human acceptance are separate claims.

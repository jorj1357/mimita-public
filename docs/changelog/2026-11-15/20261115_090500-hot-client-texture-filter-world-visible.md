# Hot client texture filter fix and visible world

# Task

- Task ID: agent-c-20261115-hot-client-visible-world
- Owner: agent-c with root dev-loop runtime evidence
- Focus: black hot-client world, PostFX presentation, texture upload bridge, avatar worker
- Status: world and HUD visibly render in the hot client; large dark spawn object still unidentified
- Date, time, timezone: 2026-11-15T09:05:00-05:00 (2026-11-15T14:05Z)
- Branch: `checkpoint/one-checkout-condensation-20261111`

# Pre-existing changes

The working tree already contained the packet/domain split, hot-client default,
PostFX mode work, cosmetic fallback, NPC avatar wire changes, and earlier
diagnostics. This record claims only the follow-up changes below.

# Requested behavior

- Determine why the hot client still presented a black or HUD-only world after
  the earlier PostFX fallbacks.
- Verify the cosmetic and avatar worker fixes in the live client.
- Keep the hot client build/run path authoritative; no competing loop or stale
  executable.

# Exact implementation changes

## File: `src/game/render-bridge-module.cpp`

Before:

```cpp
void texParameteri(TextureTarget target, TextureParameter parameter,
                   TextureValue value) {
    texParameteri(target, parameter, static_cast<std::int32_t>(value));
}
```

After:

```cpp
std::int32_t textureValueToken(TextureValue value) {
    switch (value) {
        case TextureValue::Nearest: return 0x2600;
        case TextureValue::Linear: return 0x2601;
        case TextureValue::LinearMipmapLinear: return 0x2703;
        case TextureValue::ClampToEdge: return 0x812F;
        case TextureValue::ClampToBorder: return 0x812D;
        case TextureValue::Repeat: return 0x2901;
        case TextureValue::MirroredRepeat: return 0x8370;
    }
    return 0x2601;
}

void texParameteri(TextureTarget target, TextureParameter parameter,
                   TextureValue value) {
    texParameteri(target, parameter, textureValueToken(value));
}
```

The bridge had been forwarding semantic enum numbers to the host. `Linear` is
semantic `2`, but `GL_LINEAR` is `0x2601`, so the host executed an invalid min
filter and level-0-only textures sampled black.

## Files: `src/render/post-fx.cpp`, `src/render/post-fx.h`

- Added and used a scene-FBO probe to compare pre-PostFX and post-PostFX pixels.
- Added explicit full-screen-quad state reset before the post pass: disable
  depth test, blend, cull, scissor, and stencil; disable depth writes.
- Added a temporary forced-blit diagnostic, then removed it after proving the
  FBO path was correct.
- Removed the heavy full-FBO readbacks and temporary uniform logging; the
  retained `postfx.fbo-probe` reads one center pixel per second.
- Restored the original `shaders/post.frag` after the magenta/raw-texture tests.

## File: `src/game/replay-worker-bridge-module.cpp`

The DLL-side `ReplaySaveWorker::enqueue` had queued jobs without a draining
worker, producing `no_worker` / `worker_queue_full` avatar failures. It now
submits through the resident host jobs capability:

```cpp
bool ReplaySaveWorker::enqueue(std::function<void()> job) {
    if (!job) return false;
    return mimita::game::runDetached(std::move(job), "replay-save");
}
```

# Runtime evidence

- Generation 178 (before the filter fix):
  - scene FBO: 271,579 non-black pixels
  - default framebuffer after PostFX: 0 non-black pixels
  - magenta shader: all 786,432 pixels written
  - raw `texture(uScene, vUV)` shader: 0 non-black pixels
- Generation 179: forced FBO blit showed world geometry, isolating the break to
  texture sampling.
- Generation 182 (after the filter fix):
  - scene FBO and default framebuffer both 302,531 non-black pixels
- Generation 183:
  - run `20261115_140259`, client PID `13716`
  - visible world geometry and readable HUD text captured with `PrintWindow`
  - no `no_worker` or `worker_queue_full` avatar events
  - normal `avatar.cache.pending-or-failed` load states completed without a
    worker failure

Build identity: `2026-11-15T14:00:04.521484Z:52bf0e129d603c400bd591888d7f79e9acf1047bdb3871a0754e17aaaf060e4e`,
generation 183.

# Not proved / unresolved

- A large dark silhouette can fill the centre of the spawn view. It is not yet
  proven whether it is map geometry, a nearby actor body, or a cosmetic.
- No human visual acceptance of the Jason mask, every cosmetic, or every NPC
  avatar.
- No steady-state FPS acceptance. Current frame slices still exceed the 4 ms
  budget and show packet/snapshot spikes.
- No rollback test after a deliberately failed generation.

# Regression records

- `docs/regressions/gameplay/black-world-texture-filter-REG-2026-11-15.md`
- `docs/regressions/gameplay/black-cosmetic-glb-REG-2026-11-15.md`
- `docs/regressions/gameplay/npc-avatar-name-truncation-REG-2026-11-15.md`
- `docs/regressions/performance/fps-frame-budget-REG-2026-11-15.md`

# Next exact step

Identify the large dark spawn object by actor/name from the live render state,
then verify whether it is map geometry, a nearby actor, or a cosmetic before
changing any render path.

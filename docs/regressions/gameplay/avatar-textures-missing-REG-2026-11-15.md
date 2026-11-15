# Avatar body textures missing

Date: 2026-11-15
Status: FIX IN PROGRESS — root cause identified as static-GLFW context detection in the hot DLL
Owner: avatar texture / player body rendering

## Changelog link

- [Hot input bridge, cosmetic state, hot-status tips](../../changelog/2026-11-15/20261115_162500-hot-input-bridge-cosmetic-state-hot-status.md)

## Symptom

World object textures render, but NPC and player avatar bodies appear
untextured/missing.

## Current evidence

- Cosmetic textures have nonzero ids and render with texture.
- Body texture binds are journaled as `avatar.texture.bind` or
  `avatar.texture.missing`.
- Generation 206, run `20261115_172313`: NPC body batches report
  `source_texture_id=0` and `avatar_atlas_texture_id=0` for head, torso, arms,
  and legs. The renderer falls back to `assets/textures/default.png`.
- This proves missing avatar atlas loading/binding, not world texture upload.

## Root cause

Generation 217/218 journals show `avatar.atlas.finalize.failed` with
`gl_context=false` even while the client renders. The hot game DLL's static
GLFW copy cannot see the resident window/context, so
`glfwGetCurrentContext()` is always null there. That made
`buildAtlasForInstance` return success without creating `atlasTexture`, leaving
every body batch at texture 0.

## Fix

- Avatar GL availability now uses the resident `Renderer*` (`gRenderer`) rather
  than the hot DLL's static GLFW context query.
- `npc-avatar.cpp` no longer skips `applyAvatarToPlayer` when the hot DLL's
  static GLFW has no current context. This is required for NPC avatar identity,
  body atlas, and animation-set binding.
- `player-loader.cpp` uses the resident renderer for the shared white fallback
  texture instead of the static-GLFW context probe.
- Model-ready atlas finalization still runs early; the visible render boundary
  retries until finalization succeeds, rather than permanently marking the
  attempt as done.
- `avatar.atlas.finalize.failed` remains as bounded evidence.

## Next trace

After activation, `avatar.texture.bind` must report nonzero
`avatar_atlas_texture_id` and `avatar.texture.missing` must stop for NPCs.

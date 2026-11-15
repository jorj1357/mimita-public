# Unlit cosmetic GLBs render black

Date: 2026-11-15
Time last updated: 2026-11-15T02:20:00Z
Status: OPEN — source fix and logging implemented; visible Jason-mask acceptance pending
Owner: cosmetic rendering / avatar presentation

## Symptom

Head and torso cosmetics appeared as solid black geometry in the middle of the
view. The user reported the Jason mask and other actor cosmetics. A 1v1 showed
one black object; a payload map with many NPCs showed many black cosmetic
objects.

## Evidence

- `assets/objects/things/cosmetics/*.glb` contain geometry, UVs, and normals but
  no materials, images, or textures.
- `assets/avatars/jason/avatar.json` enables `jason1.glb` on the head with an
  external texture image.
- `assets/avatars/jorj1357/avatar.json` enables `clown1.glb` on the torso with an
  empty texture image.
- `src/avatar/cosmetic-system.cpp` used
  `AvatarSystem::instance().currentName()` to build remote cosmetic texture
  paths, so remote actors could look in the local avatar's folder.
- The same renderer always took the texture path (`uUseColor = 0`) and had no
  solid-color fallback when the cosmetic had no usable texture.

## Incorrect code

Before correction:

```cpp
return "assets/avatars/" + AvatarSystem::instance().currentName() + "/" +
       slot.texture.image;
```

```cpp
r::uniform1i(useColorLoc, 0);
r::bindTexture(r::TextureTarget::Texture2D,
               overrideTexture ? overrideTexture : batch.texture);
```

An untextured cosmetic therefore sampled a missing/default texture. It could
render black even though the authored color was white.

## Corrected code

- Resolve the texture path with the owning actor's `player.avatarName()`.
- If no usable override texture exists, set `uUseColor = 1` and use
  `slot.color` / `slot.texture.color` as `uColor`.
- Do not bind a texture in the solid-color path.
- Reset `uUseColor` after cosmetic rendering.
- Emit rate-limited `cosmetic.render` records with actor avatar, GLB, anchor,
  texture path, texture id, fallback flag, color, brightness, opacity, and UV
  transform.

## Acceptance

- Jason mask renders visibly for local and remote actors.
- No black untextured cosmetic geometry appears.
- A payload map with many NPCs does not produce many black objects.
- Visible hot-client acceptance must be recorded; source alone is not acceptance.


## Follow-up root cause (2026-11-15)

The black geometry was not only a cosmetic override problem. The hot DLL
`texParameteri(TextureValue)` bridge forwarded semantic enum numbers to the
host instead of GL tokens. `Linear` became `2` instead of `0x2601`, so min
filters were invalid and textures without mipmaps sampled black. The cosmetic
path, HUD font, and other textured meshes shared that failure.

See `black-world-texture-filter-REG-2026-11-15.md` for the exact owner,
evidence, and corrected bridge code. After generation 182, scene FBO and
default framebuffer probes match and the hot client presents visible world
geometry. Human Jason-mask acceptance remains pending.

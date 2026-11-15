# Black boxes around font glyphs

Date: 2026-11-15
Status: RESOLVED / not currently reproducing — document-only regression record
Owner: game render bridge / font texture upload
Related: `black-world-texture-filter-REG-2026-11-15.md`,
`black-cosmetic-glb-REG-2026-11-15.md`

## Symptom

Text rendered as dark rectangular boxes at the glyph positions. The boxes were
most visible on HUD labels, chat text, and terminal text. The symptom was
reported together with the hot-client black world and the black Jason cosmetic.

## Changelog evidence

- [Hot client texture filter fix and visible world](../../changelog/2026-11-15/20261115_090500-hot-client-texture-filter-world-visible.md)
- [Hot client live default, cosmetic and NPC avatar fixes](../../changelog/2026-11-14/20261114_222000-hot-client-live-default-cosmetic-avatar-fixes.md)

The second record documents the cosmetic/avatar path that first exposed black
sampling. The first record contains the actual texture-filter correction.

## Root cause

`src/game/render-bridge-module.cpp` forwarded the semantic `TextureValue`
number to the host as a raw GL integer:

```cpp
texParameteri(target, parameter, static_cast<std::int32_t>(value));
```

`TextureValue::Linear` is semantic `2`; `GL_LINEAR` is `0x2601`. The host
therefore executed an invalid `GL_TEXTURE_MIN_FILTER` update. The texture kept
its default mipmap minification filter, and textures uploaded with only level 0
were incomplete for sampling.

The font atlas uses the same path in
`src/gui/font-stuff/font-loader.cpp`:

```cpp
r::texParameteri(r::TextureTarget::Texture2D,
                 r::TextureParameter::MinFilter, r::TextureValue::Nearest);
```

Page 1 contains lowercase ASCII glyphs; page 0 contains uppercase and other
glyphs. An incomplete page sampled black, so each glyph quad became a dark box.

The same failure explained black cosmetic geometry and the black world; the
world and font symptoms were two consumers of one broken texture-state bridge.

## Corrected code

`src/game/render-bridge-module.cpp` now maps semantic values to stable GL
tokens before forwarding:

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
```

## Runtime evidence

- Generation 178: default framebuffer after PostFX had 0 non-black pixels while
  the scene FBO had 271,579.
- Generation 182: scene FBO and default framebuffer both had 302,531 non-black
  pixels.
- Generation 183/189/194: HUD text, chat text, and terminal text are readable;
  the black-box symptom is not reproducing in the current hot client.
- Font atlas pages were inspected: page 0 and page 1 are 256x256 RGBA with
  white glyph RGB and alpha coverage.

## Prevention

- Never forward a semantic render enum number as a GL token.
- Keep texture-value mapping in one bridge helper and test it with a known
  `MinFilter=Nearest`/`Linear` upload.
- Add/retain a font-page probe (`font.pages`, texture ids, page count) before
  claiming text is correct.
- Treat black world, black cosmetics, and black font as the same texture-state
  regression class until proven otherwise.

## Current status

This record is documentation of the observed failure and correction. It does
not claim a new font bug is active. If black boxes return, first verify the
active game DLL contains the texture-token mapping and that both font pages have
valid texture ids.

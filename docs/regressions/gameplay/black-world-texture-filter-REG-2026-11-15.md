# Hot client world and text sample black after texture upload

Date: 2026-11-15
Time last updated: 2026-11-15T09:05:00-05:00
Status: FIXED — runtime evidence in generation 182/183; human visual acceptance
of every avatar/cosmetic still pending
Owner: game render bridge / texture upload
Related: `black-cosmetic-glb-REG-2026-11-15.md`

## Symptom

The hot client presented a black world with only HUD geometry. HUD text rendered
as dark rectangles. Cosmetic GLBs and other textured meshes could also render
black even though their textures loaded and the scene FBO contained non-black
pixels.

## Evidence

- Generation 178 scene FBO probe: 271,579 non-black pixels of 786,432 before
  PostFX.
- Generation 178 default framebuffer after PostFX: 0 non-black pixels.
- A magenta fragment shader filled the full screen, proving the quad, VAO, and
  draw call reached the default framebuffer.
- A raw `texture(uScene, vUV)` fragment shader still output black, proving the
  break was texture sampling, not PostFX math.
- Moving the sampler to texture unit 1 did not change the black result.
- The same texture could be read back from its FBO attachment, proving storage
  existed and the FBO was complete.

## Incorrect code

`src/game/render-bridge-module.cpp` forwarded the semantic `TextureValue` enum
number directly as a GL integer:

```cpp
void texParameteri(TextureTarget target, TextureParameter parameter,
                   TextureValue value) {
    texParameteri(target, parameter, static_cast<std::int32_t>(value));
}
```

`TextureValue::Linear` is semantic value `2`, but `GL_LINEAR` is `0x2601`.
The host therefore executed effectively:

```cpp
glTexParameteri(target, GL_TEXTURE_MIN_FILTER, 2);
```

That raises `GL_INVALID_ENUM` and leaves the default mipmap minification filter.
Textures uploaded with only level 0 are incomplete under a mipmap filter, so
sampling returns black. The EXE legacy path did not show the bug because it
mapped the enum through `toGl(TextureValue)`.

## Corrected code

The DLL bridge maps semantic texture values to the stable GL tokens expected by
the host:

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

## Runtime acceptance

- Generation 179: forced FBO blit presented the world, isolating the failure to
  the shader-sample path.
- Generation 182: after the filter fix, default framebuffer probe matched the
  scene FBO (302,531 non-black pixels before and after PostFX).
- Generation 183: hot client launched and rendered visible world geometry with
  readable HUD text; screenshots captured from the running `mimita.exe`.
- No `avatar` worker failures (`no_worker` / `worker_queue_full`) appeared in the
  generation 183 run; avatar requests were normal pending loads.

## Remaining visual question

A large dark object can still fill the centre of the current spawn view. It is
not proven whether that is map geometry, a nearby actor body, or a cosmetic.
This regression closes the broken texture filter. It does not claim that every
avatar/cosmetic has passed human visual review.

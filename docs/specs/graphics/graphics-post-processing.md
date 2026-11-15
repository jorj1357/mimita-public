# Graphics Post-Processing (PostFX)

## Purpose

PostFX is the final screen-space color and presentation pass. It is owned by
the replaceable game generation (`src/render/post-fx.cpp`, `shaders/post.*`,
`src/render/postfx-commands.cpp`). The resident renderer owns only the window,
GL context, and buffer swap.

This specification is the source of truth for what PostFX is, what each setting
does, what it costs, and how to disable it without pretending it is still active.

## Pipeline

Normal and stretched modes differ.

Normal mode:

1. Render world/actors/effects into the scene color+depth FBO.
2. Unbind the FBO.
3. Run the fullscreen post shader sampling the scene color texture.
4. Draw GUI/HUD on the default framebuffer.
5. Swap buffers.

Full-frame stretched modes 5/6:

1. Render the complete frame, including GUI, at 1024x768.
2. Run PostFX against that full-frame target.
3. Stretch the result to the display and swap.

The FBO is therefore required for output scaling even when all effects are
identity. `full` and `minimal` keep the FBO; `off` does not.

## Modes

`config/postfx.json` is authoritative:

```json
{
  "mode": "full",
  "brightness": 0.8,
  "gamma": 2.2
}
```

| Mode | Behavior | Cost |
|---|---|---|
| `full` | Scene FBO + fullscreen post shader with configured effects. | FBO + fullscreen pass + effect ALU. |
| `minimal` | Scene FBO + fullscreen pass limited to brightness/contrast/saturation/gamma/hue/color-temperature. Optional screen effects are forced off. | FBO + tiny fullscreen pass. |
| `off` | No PostFX FBO and no post pass. World renders directly to the default framebuffer. | Zero PostFX cost; image is intentionally raw/dark because brightness/gamma are not applied. |

Default: `full`.

Mode precedence:

1. `config/postfx.json` on load/reload.
2. `postfx_mode <full|minimal|off>` is a temporary runtime override.
3. A JSON reload replaces the temporary override.

`MIMITA_DISABLE_POSTFX=1` is a test-only alias for `off`; it must not be used as
the normal configuration path.

## Why identity is not free

`PostFX::isActive()` reports whether every setting is identity. The shader has
an identity early-out, but unless the host skips the pass, the frame still pays:

- a full-resolution color texture,
- a depth renderbuffer,
- an FBO clear,
- a fullscreen texture sample and draw.

`skip_when_identity` is the intended optimization: when all settings are
identity and output scaling is not required, skip the FBO/pass entirely.

## Settings

Core color:

| Setting | Default | Meaning |
|---|---:|---|
| `brightness` | 0.8 | Multiplies the scene color. |
| `contrast` | 1.0 | Expands color around 0.5. |
| `saturation` | 1.0 | Mixes luma with color. |
| `gamma` | 2.2 | Gamma exponent. |
| `hueShift` | 0.0 | Rotates hue. |
| `colorTemperature` | 0.0 | Warms/cools red vs blue. |

Screen effects:

| Setting | Default | Meaning | Extra cost |
|---|---:|---|---|
| `vignette` | 0.0 | Darkens screen edges. | ALU |
| `filmGrain` | 0.0 | Adds animated noise. | ALU |
| `chromaticAberration` | 0.0 | Splits red/blue samples. | 2 extra samples |
| `lensDistortion` | 0.0 | Radial lens warp. | ALU + optional blur taps |
| `scanlines` | 0.0 | Horizontal line modulation. | ALU |
| `pixelation` | 0.0 | Quantizes UVs. | ALU |
| `posterize` | 0.0 | Quantizes color levels. | ALU |

Artistic modes:

| Setting | Default | Meaning |
|---|---:|---|
| `dreamStrength` | 0.0 | Dreamy bloom/tint. |
| `voidStrength` | 0.0 | Void/darkness treatment. |
| `psychedelicStrength` | 0.0 | Animated color distortion. |
| `retroStrength` | 0.0 | Retro/CRT look. |
| `glitchStrength` | 0.0 | RGB split and displacement. |

Procedural modifiers:

| Setting | Default | Meaning |
|---|---:|---|
| `worldWave` | 0.0 | World-space wave modulation. |
| `screenWave` | 0.0 | Whole-screen wave warp. |
| `screenShakeFx` | 0.0 | Screen shake modifier. |
| `edgeGlow` | 0.0 | Edge highlight. |
| `outlineBoost` | 0.0 | Outline emphasis. |
| `shadowBoost` | 0.0 | Shadow emphasis. |

GUI color settings (`guiBrightness`, `guiContrast`, `guiSaturation`,
`guiHueShift`) are applied to GUI presentation by the GUI/render owners, not by
the world PostFX pass.

Lens distortion also has camera-owned fields (`curve`, `zoom`, edge mode,
radius, softness, darkness, peripheral blur). These are forwarded from the
camera owner and must stay finite and bounded; invalid live values must not
black the frame.

## Hot reload

- `PostFX::pollReload()` reloads `config/postfx.json` on file change.
- `postfx_reload` forces a reload.
- `postfx_mode` changes the current session only.
- `postfx_preset`, `postfxrand`, and `postfx_debug` are development tools.
- Leaving/relogging must not silently change the configured mode.

## Acceptance

- `full` preserves the current approved look.
- `minimal` is visibly usable and runs only the display transform.
- `off` runs no PostFX FBO and no post pass.
- A JSON mode change applies live.
- `postfx_mode` changes the current session and is replaced by the next JSON
  reload.
- Journal/status evidence distinguishes mode, FBO allocation, and pass execution.

# issue-3c1894ca50ec: Opaque black world surfaces must not become pass-through

## Human report (verbatim, immutable)

> still the black colors are treated as pass thru dont look at them


## Agent suggestion

### 2026-11-24T22:27:04.337Z — build-wait-reviewer

Read-only source trace lead from build-wait-reviewer (hypothesis, not a confirmed cause):
- shaders/basic.frag:330-335 computes finalAlpha = texel.a * proximityAlpha (* uOpacity) and unconditionally discards when finalAlpha <= 0.0. uAlphaCutoff is set to 0.0 for world meshes (src/render/render-world-mesh.cpp:218-219), so it never gates this; any world texel with alpha 0 is dropped regardless of its RGB.
- src/render/render-world-mesh.cpp:399-412 decides transparentBatch only from batch.opacity/batch.tint.a; it never reads a material alphaMode and does not force alpha=1 for opaque batches.
- src/map/map-loader-material.cpp:136-138 uploads GLB images with the source component count into an RGBA internal format: 3-component RGB uploads get alpha=1.0 (opaque), 4-component images keep authored alpha. So an opaque-looking black region can pass through only if its texture is 4-component with alpha~0 there, or the batch/material path sets opacity/tint alpha <1.
Falsifying next check: identify the exact sword/map material, inspect that GLB image's component count and alpha at the black texels, and note whether pass-through persists when the texture is treated as 3-component / alpha forced to 1. If a 4-component black texture has alpha 0, the decision is whether opaque materials should ignore texture alpha or the asset alpha is intended. This narrows the path between texture alpha and material opacity but does not prove the cause.

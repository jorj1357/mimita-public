# Black impact ghost — 2026-11-24

## Symptom

Black, jagged shapes appeared on walls and floors after Bloodtest bullets hit
the world. They looked like a second world shader or a black-color blending
failure. The 3D shockwave model was not visible at the same time.

## Investigation

- The black shapes were **surface decals**, not a second world pass.
- `src/combat/explosion-fx.cpp` called `EffectPartSystem::spawnExplosionMark`
  for `bloodtest_ak47` and `bloodtest_judgement` world impacts.
- `config/impact_decals.json` defines `explosionMarks.radius` as **4.0 m**.
  That profile is intended for rocket explosions, not bullets.
- `bulletHoles.textureScale` was **7.0**, so even a bullet hole rendered about
  **1.4 m** wide.
- The 3D shockwave was drawn (`explosion.shockwave.draw` with
  `render_path=model_mesh`), but it was normalized to about 0.5 m and sat
  inside the 4 m black mark.

## What did not fix it

- Changing UI layering did not fix the world shapes.
- Restoring shader opacity uniforms fixed state leaks but did not remove the
  black decals.
- Textures already had valid alpha; the black pixels were part of the decal
  content, not a missing alpha channel.

## Fix

- Bullets use the small bullet-hole owner, never `spawnExplosionMark`.
- Bullet-hole radius/texture scale are small, readable world values.
- Shockwave models have an explicit `base_scale` and near-camera fade.
- One surface-impact profile is shared by rockets and bullets, with per-weapon
  values.

## Lesson

Check the **decal owner and world size** before investigating shader blending.
A black texture at 4 m is indistinguishable from a render bug in a screenshot.
Add `impact.decal.spawn` / `impact.decal.render` evidence so the kind, texture,
radius, and alpha are visible without guessing.

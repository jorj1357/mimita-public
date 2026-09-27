# White hit-impact sphere presentation

## Change

The existing entity-hit effect in `config/hitfx.json` now presents as a full
opaque white sphere at the hit point:

- `damageImpactSphere.local_dimensions` is `[1, 1, 1]` instead of a long thin
  directional ellipsoid.
- `damageImpactSphere.length` is nearly zero so its center remains at the hit.
- Color is `[1, 1, 1]` and alpha is `1`.

## Runtime path

`HitEffects::onHit()` calls
`EffectPartSystem::spawnDamageImpactSphere()`. That creates an effect with
`replayType = "damage_impact_sphere"`. The renderer recognizes that type in
`src/effects/effect-part-render.cpp` and emits the filled-sphere triangles via
`DebugVis::drawFilledSphereOriented()`.

`config/hitfx.json` is hot-reloaded by the existing hit-effects config owner,
so these presentation values are intended to update while the game runs.

## Not included

Blackening only the limb or object geometry that intersects the white sphere
requires a separate masked intersection render pass. The current filled-triangle
pass uses depth testing but does not re-render actor meshes with a black shader.
No such pass was added here.

## Validation

- JSON parsing: passed previously for the repository configuration; the edited
  values remain valid JSON.
- Build and runtime visual acceptance: not completed because the repository
  C++ compiler is unavailable on PATH.

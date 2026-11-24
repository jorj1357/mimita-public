# World impact feedback

**Purpose:** define the shared surface-impact presentation used by Bloodtest
and other projectile weapons.
**Audience:** effects, weapon, render, and gamemode contributors.
**Does not:** create a second projectile, damage, or decal owner.

## Contract

A projectile world contact emits exactly one surface-impact presentation:

1. a material-aware bullet hole placed at the contact point along the surface
   normal;
2. an optional short-lived surface shockwave;
3. an optional small debris/spark burst;
4. no rocket/grenade fireball, smoke, or explosion mark for bullets.

The same profile schema must be configurable per weapon and shared by
projectiles and rockets. A profile describes either an authored model or a
primitive:

```json
"world_impact": {
  "kind": "model",
  "model": "assets/objects/things/effects-objects/shockwave-v1.glb",
  "texture": "assets/textureshq/rocket-shockwave-v1.png",
  "normalize_model": true,
  "base_scale": 1.0,
  "start_scale": 0.5,
  "end_scale": 2.0,
  "start_alpha": 0.5,
  "end_alpha": 0.0,
  "start_color": [1.0, 1.0, 1.0],
  "end_color": [1.0, 1.0, 1.0],
  "lifetime_seconds": 0.10,
  "start_rotation_degrees": 0.0,
  "end_rotation_degrees": 360.0,
  "random_rotation_min_degrees": 0.0,
  "random_rotation_max_degrees": 0.0,
  "surface_offset_m": 0.05,
  "near_camera_fade_m": 1.5
}
```

`kind` may be `model`, `cylinder`, `ring`, `plane`, or `cube`. The renderer
aligns local +Z to the surface normal and spins around that normal. Scale and
alpha interpolate linearly over the profile lifetime.

## Bullet holes

Bullet holes are separate from shockwaves. They use the shared bullet-hole
decal owner with a small world radius. The rocket `explosionMarks` profile is
not valid for bullets.

## Blood splatter

A body hit may also place blood on the surface behind the victim. The splatter
uses the shot/exit direction, a central large decal, and a cone of smaller
decals. It must not replace the bullet hole or shockwave.

## Evidence

| Event | Required fields |
|---|---|
| `impact.surface.spawn` | weapon, kind, model/primitive, position, normal, start/end scale, start/end alpha, lifetime |
| `impact.surface.draw` | render path, position, scale, alpha, age fraction |
| `impact.decal.spawn` | kind, position, normal, radius, texture, alpha, lifetime |
| `impact.decal.render` | kind, radius, alpha, texture, camera distance |

Visual acceptance still requires a human to observe the effect in a live run.

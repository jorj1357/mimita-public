# Weapon Collision Visuals & JSON Config

## Commands

| Command | Description |
|---------|-------------|
| `weapon_collision_visuals [0\|1]` | Toggle weapon collision debug visuals. Draws actual runtime collision shapes. |
| `weapon_collision_debug [0\|1]` | Legacy toggle for collision debug system (separate from visuals). |

## Color Meanings

| Color | Element | Meaning |
|-------|---------|---------|
| Turquoise (0.0, 1.0, 1.0) 50% | Filled wire sphere | Current-frame collision sphere |
| Dark blue (0.0, 0.5, 1.0) 50% | Capsule wireframe | Current-frame weapon collision capsule |
| Red (1.0, 0.0, 0.0) 50% | Filled wire sphere | Previous-frame collision sphere |
| Dark red (0.5, 0.0, 0.0) 40% | Capsule wireframe | Previous-frame weapon collision capsule |
| Yellow (1.0, 1.0, 0.0) 90% | Line | Sweep path from previous center to current center |

All visuals are rendered with depth test disabled so they are visible through geometry.

## Source of Truth

The debug visuals draw from `Player::weaponCollisionDebug` (type `WeaponCollisionRuntimeDebug`).

This struct is populated by `recomputeWeaponCapsule()` in `physics-collision-body.cpp`, which is the same function that feeds the collision solver. The JSON config-driven shapes override the capsule-based debug data via `WeaponCollisionJsonConfig::applyCollisionConfig()`.

What you see = what collides.

## JSON Schema

File: `config/weaponcollisions.json`

### Top-level keys

```
weapon_id (string) -> {
    "enabled": bool,
    "collides_with_world": bool,
    "collision_skin": float (>= 0),

    "capsule": { ... },
    "spheres": [ ... ],
    "generated_spheres": { ... }
}
```

Supported weapon IDs: `revolver`, `shotgun`, `rocket_launcher`, `grenade_launcher`.

### Capsule

```json
"capsule": {
    "enabled": true,
    "start": [x, y, z],
    "end": [x, y, z],
    "radius": 0.08,
    "scale": [1.0, 1.0, 1.0],
    "rotation_degrees": [0.0, 0.0, 0.0]
}
```

- `start` and `end` are in weapon-local space
- `radius` must be > 0
- The capsule connects `start` → `end` with the given radius

### Spheres (explicit)

```json
"spheres": [
    {
        "name": "barrel_0",
        "enabled": true,
        "position": [x, y, z],
        "radius": 0.08,
        "scale": [1.0, 1.0, 1.0],
        "rotation_degrees": [0.0, 0.0, 0.0]
    }
]
```

### Generated Spheres (editable count)

```json
"generated_spheres": {
    "enabled": true,
    "count": 8,
    "start": [x, y, z],
    "end": [x, y, z],
    "radius": 0.12
}
```

- `count` can be changed at runtime (hot reload). Must be >= 1.
- Spheres are evenly sampled along the line from `start` to `end`
- Changing `count` from 8 to 10 immediately creates 10 collision spheres

## Hot Reload

`config/weaponcollisions.json` is polled every 250ms. When the file changes:

1. The new JSON is parsed and validated
2. If parsing succeeds, the config is applied immediately
3. If parsing fails, an error is logged and the previous valid config is kept
4. No game restart required

Example logs:
```
[WEAPON COLLISIONS JSON] reload ok weapons=4
[WEAPON COLLISIONS JSON] shotgun.generated_spheres.count=10
[WEAPON COLLISIONS JSON] ERROR revolver.capsule.radius must be > 0
```

## Debugging Invisible Visuals

If `weapon_collision_visuals 1` shows nothing:

1. **Is the command registered?** Type `help weapon_collision_visuals` to confirm
2. **Is a weapon equipped?** The feature requires `player.collision.hasWeaponCollisionCapsule == true`
3. **Are positions valid?** NaN or zero positions won't draw. Check the per-frame log:
   `[WEAPON COLLISION VISUALS] draw weapon=<id> spheres=<n> capsule=<n>`
4. **Is the weapon supported?** Only weapons with entries in `config/weaponcollisions.json` get config-driven visuals
5. **Are debug visuals active?** The weapon collision visuals use a separate rendering path independent of `DebugVis::enabled()`, so they should work without any other debug flags enabled

## Confirming Visuals Match Real Collision Data

The `WeaponCollisionRuntimeDebug` struct is populated at the same point where collision spheres are generated. To verify:

1. Enable `weapon_collision_visuals 1`
2. Move the weapon against a wall
3. The turquoise spheres should contact the wall exactly where collision pushback occurs
4. Yellow sweep lines show capsule movement between frames

## Examples

### Revolver with custom sphere
```json
"revolver": {
    "enabled": true,
    "capsule": {
        "enabled": true,
        "start": [0.0, 0.0, 0.0],
        "end": [0.0, 0.0, 0.45],
        "radius": 0.08
    },
    "generated_spheres": {
        "enabled": true,
        "count": 8,
        "start": [0.0, 0.0, 0.0],
        "end": [0.0, 0.0, 0.45],
        "radius": 0.08
    }
}
```

### Shotgun with 10 generated spheres
```json
"shotgun": {
    "enabled": true,
    "generated_spheres": {
        "enabled": true,
        "count": 10,
        "start": [0.0, 0.0, 0.0],
        "end": [0.0, 0.0, 1.0],
        "radius": 0.10
    }
}
```

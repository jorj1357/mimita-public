# Hot-reloadable collision skin

- Date: 2026-09-27
- Scope: shared collision margin ownership and runtime configuration
- Status: implemented and self-tested

## Changes

- Moved the shared actor/world collision skin out of the hardcoded physics constants and into `config/collision.json` as `collisionSkin`.
- Added `CollisionConfig::collisionSkin()` as the single runtime owner.
- Replaced collision call sites that directly read the old constant, including actor triangle broadphase, mesh contact skin, GLB contact/safety, block collision, and legacy GLB setup.
- Kept the existing collision config hot-reload polling path; changing `collisionSkin` now applies without rebuilding or restarting.
- Removed the old `COLLISION_SKIN` and `COLLISION_GATHER_EXPANSION` duplicate values from `src/physics/config.h`.

## Runtime configuration

```json
"collisionSkin": 0.05
```

The value is clamped to `0.0` through `0.25` when loaded.

## Validation

- Unique executable build/link: PASS
- Actor triangle solver self-test: PASS
- Moving crate self-test: PASS
- Collision stress self-test: PASS

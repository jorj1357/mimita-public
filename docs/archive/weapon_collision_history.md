# Weapon Collision History

## Old Architecture (before 4d37a13)

Weapon collision was a single capsule:

```
grip point → tip point → radius
```

5 sphere samples along capsule axis → swept vs world triangles → position correction.

No configurable colliders.
No JSON collision shapes.
No box/capsule collider types.
No manifold complexity.

**Files involved:**
- `physics-collision-body.cpp` (`recomputeWeaponCapsule()`, `collectBodyWeaponSpheres()` — 5 sphere samples)
- `physics-collision-glb-body.cpp` (`runBodyWeaponPass()` — simple sweep + correct)
- `weapon-viewmodel.cpp` (set `weaponGripLocal`, `weaponMuzzleLocal`, `weaponRadiusLocal` from model bounds)

**Performance:** 5 spheres × ~50 world triangles = 250 tests per frame. No lag spikes.

## Commit 4d37a13 — Configurable Colliders Introduced

Added:
- `WeaponColliderConfig` struct (name, shape Box/Capsule, position, rotation, size, flags)
- `WeaponCollisionConfig` struct (enabled, authoritative, colliders array)
- `collectWeaponConfigSpheres()` — generates 5 spheres per collider along dominant axis
- JSON parsing for collision shapes (`applyWeaponCollisionJson()`)
- Fallback collider generation per weapon type

**Why:** Allow per-weapon collision shape tuning (e.g., HAFS needs a long blade collider).

**Result:** For a weapon with 3 config colliders + 1 capsule + 6 body parts = (3×5) + 5 + (6×5) = 15 + 5 + 30 = 50 spheres → 50 × 50 world triangles = 2500 tests per frame. At 60fps for 10 NPCs = 15,000 tests. Lag spikes at 5 seconds after spawn (first broadphase).

## Current Architecture (HEAD)

```
recomputeWeaponCapsule()           — single capsule (grip→tip→radius)
collectBodyWeaponSpheres()         — 3 sources:
  1. Body part spheres (6 parts × 5 each = 30)
  2. Weapon capsule spheres (5 samples)
  3. Config collider spheres (N colliders × 5 each)
  4. Mesh vertex spheres (M vertices — added later)
```

Each sphere is swept against world triangles. The solver uses Gauss-Seidel with 6 passes.

## Complexity Added Total

| Feature | Lines | Status |
|---------|-------|--------|
| `WeaponColliderConfig` struct | ~10 | REMOVED |
| `WeaponCollisionConfig` struct | ~5 | REMOVED |
| `WeaponColliderShape` enum | ~4 | REMOVED |
| `collectWeaponConfigSpheres()` | ~70 | REMOVED |
| `applyWeaponCollisionJson()` | ~40 | REMOVED |
| Fallback collider generation | ~50 | REMOVED |
| JSON collision shapes in weapons.json | ~20 per weapon | REMOVED |
| Mesh vertex spheres | ~30 | REMOVED |
| **Total** | **~230 lines** | **REMOVED** |

## Target Architecture

Single capsule only:

```
Capsule(grip, tip, radius)
```

5 sphere samples.
Swept against world triangles.
Continuous collision with TOI.
4 sweep iterations max.
Weapon determines legal root position.

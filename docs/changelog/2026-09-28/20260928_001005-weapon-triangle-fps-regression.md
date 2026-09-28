# Weapon triangle FPS regression

- Date: 2026-09-28
- Scope: active actor collision weapon initialization and JSON weapon approximation
- Status: implemented and self-tested; live badhouse FPS acceptance remains open

## Root cause

- The active actor solver synchronized the equipped weapon render mesh before initializing the JSON weapon collider.
- That left `weaponCollisionDebug.valid` false during collection, so the render-mesh triangle fallback was included in the authoritative actor query.
- Dense weapon meshes multiplied triangle narrowphase work near slopes and other dense geometry.

## Fix

- The active actor step now calls `recomputeWeaponCapsule` before collecting actor meshes.
- Configured weapons therefore use JSON spheres/capsules/generated samples, while render triangles remain only for unconfigured/test fallback cases.
- The JSON weapon approximation still enters the same authoritative contact manifold and preserves the global bounce behavior.
- The shared actor/world `collisionSkin` remains hot-reloadable from `config/collision.json`.

## Validation

- `mimita-20260928T001000.exe --actor-triangle-solve-selftest`: PASS
- `mimita-20260928T001000.exe --moving-crate-selftest`: PASS
- `mimita-20260928T001000.exe --collision-selftest`: PASS
- Build: unique executable linked successfully.

## Follow-up

- If corner snagging remains after the weapon triangle cost is removed, capture the actor contact labels, normals, penetration, and candidate count at the exact block seam. The next change should stabilize shallow edge/vertex contacts by provenance rather than increasing the global skin to `0.1`.

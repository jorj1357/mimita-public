# Rounded actor triangle feature shell

- Date: 2026-09-28
- Scope: limb edges, vertices, block seams, and wall penetration
- Status: implemented and deterministic tests pass; live acceptance remains open

## Change

- Added a code-owned `0.025` movement feature radius.
- Actor triangle movement now checks a rounded shell made from vertex spheres,
  edge capsules, and the solid triangle face.
- Exact triangle contacts remain the depenetration safety path.
- Rounded contacts use closest-feature normals for movement response, reducing
  hard edge/vertex snagging without making `collisionSkin` physical penetration.
- Broadphase queries include the movement radius even when JSON skin is smaller.
- Added a conservative AABB rejection before the rounded feature narrowphase to
  avoid doing the expensive feature calculation for distant triangles.

## Validation

- `python build_agent.py`: SUCCESS; `Compiled: 2`, `Skipped: 486`.
- `mimita.exe --actor-triangle-solve-selftest`: PASS.
- `mimita.exe --moving-crate-selftest`: PASS.
- `mimita.exe --collision-selftest`: PASS.
- Live wall/seam/corner acceptance still needs to be performed in the game.

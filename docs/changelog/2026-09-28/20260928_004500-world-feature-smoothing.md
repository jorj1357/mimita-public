# World collision feature smoothing

- Date: 2026-09-28
- Scope: static world/block vertices, edges, and face response
- Status: implemented and deterministic tests pass; live acceptance remains open

## Change

- Renamed the code-owned movement radius to
  `MOVEMENT_FEATURE_SMOOTHNESS` in `src/physics/movement/physics-collision.h`.
- Static world contacts now use closest world vertex/edge/face features for
  rounded response even when the actor is already overlapping the triangle.
- Exact triangle normals remain responsible for depenetration, so smoothing
  cannot authorize penetration into the world.
- The static-world broadphase includes the smoothness radius.
- Moving physical-entity contacts retain their exact support-normal contract
  until support-feature smoothing is migrated separately.

## Validation

- `python build_agent.py`: SUCCESS; `Compiled: 2`, `Skipped: 486`.
- `mimita.exe --actor-triangle-solve-selftest`: PASS.
- `mimita.exe --moving-crate-selftest`: PASS.
- `mimita.exe --collision-selftest`: PASS.
- Current code-owned smoothness value tested: `0.1`.

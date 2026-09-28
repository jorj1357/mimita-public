# JSON weapon collision approximation

- Date: 2026-09-27
- Scope: active actor-triangle movement collision and weapon contacts
- Status: implemented and self-tested; live badhouse FPS and corner acceptance remain open

## Changes

- Configured weapons no longer add their render-mesh triangles to the active actor triangle solver.
- Active weapon collision now uses the existing JSON-defined spheres, capsule samples, multiple capsules, and generated spheres.
- Weapon approximation contacts still enter the actor solver's single authoritative manifold and use the global bounce toggle, so weapons continue to bounce the player when enabled.
- Kept render-mesh weapon triangles available for unconfigured/test actors instead of deleting the fallback before human acceptance.
- Added a per-sphere swept AABB rejection before weapon sphere/triangle narrowphase tests. This reduces wasted tests when a weapon's union gather overlaps dense slope geometry.
- Reduced the revolver JSON contact skin from `0.05` to `0.02` to reduce wall penetration before rebound.

## Validation

- `mimita-20260927T203500.exe --actor-triangle-solve-selftest`: PASS
- `mimita-20260927T203500.exe --moving-crate-selftest`: PASS
- `mimita-20260927T203500.exe --collision-selftest`: PASS
- Unique executable build/link: PASS

## Follow-up

- Test the revolver and slopes in badhouse with weapon collider debug visibility enabled if needed. If the FPS dip remains, capture actor collision candidate/contact counts there; the remaining likely cost is dense slope body-triangle narrowphase, not weapon render triangles.

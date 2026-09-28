// 2026-09-28T22:10:18Z
/* purpose
* record the fix for destructible-crate rendering (inverted winding/backfaces),
* the collision/rendering performance collapse near a crate, and projectiles
* failing to cut the crate
* does NOT claim live multiplayer acceptance
*/

# Task

- Summary: A spawned destructible crate showed backfaces, dropped the game to
  ~1 FPS when approached, and was never cut by the projectile rifle. All three
  shared one root cause: the isosurface mesher emitted triangles whose vertex
  winding did not match the outward surface normal.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VALIDATION_REQUIRED / HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-28T22:10:18Z ISO 8601 UTC; display timezone
  America/New_York (2026-09-28 18:10 EDT).

# Root cause

- `emitTriangle` (`src/impact/isosurface.cpp`) flipped only the stored normal to
  face outward; it left the vertex order (`a,b,c`) unchanged. Rendering then
  culled the true front faces and showed the interior (backfaces).
- The shared projectile kernel recomputes an entity triangle's normal from the
  vertex winding (`cross(b-a, c-a)` in `queryEntityTrianglesSwept`) and uses it
  for the face approach test in `sweepSphereTriangle`. With inward winding the
  face test saw a positive approach speed and was skipped, so a bullet aimed at
  a face tunnelled through the crate. Edge/vertex tests do not catch a
  face-centre hit, so the crate was never hit and no cut was created.
- Performance: a destructible crate carries thousands of generated triangles
  (about 8k at the current 3x3x3 / 8-cells-per-chunk grid). Hot paths scanned or
  copied every one of them:
  - `entityWorldAABB` transformed every triangle on each call and is called per
    entity per actor per tick.
  - `collectActorEntityContacts` copied and transformed the whole mesh into a
    temporary `World` when the actor's AABB overlapped the entity.
  - `queryEntityTrianglesSwept` transformed the whole mesh per projectile
    substep.

# Changes

- `src/impact/isosurface.cpp`: `emitTriangle` now swaps the vertex winding when
  the geometric normal opposes the outward SDF gradient, so
  `cross(b-a, c-a)` always agrees with the stored outward normal. Fixes
  rendering culling and the projectile face-hit test together.
- `src/impact/destructible-geometry.h/.cpp`: added
  `DestructionTriangleRange` and `DestructibleGeometry::chunkTriangleRanges`,
  filled in `refreshCachedArrays`. Added
  `DestructibleGeometrySystem::collectWorldTriangles(geometry, transform,
  queryWorld, out)`, which appends only the triangles of chunks whose
  transformed bounds overlap a world-space query.
- `src/physics/physical-entity.cpp`:
  - `entityWorldAABB` is O(1) for destructible entities (transforms the
    `halfExtents` box, which always contains the generated geometry).
  - `collectActorEntityContacts` uses `collectWorldTriangles` for destructible
    entities so only nearby chunks are transformed/copied.
- `src/network/server-projectiles.cpp`: `queryEntityTrianglesSwept` uses
  `collectWorldTriangles` (chunk broadphase) instead of transforming the whole
  mesh for every projectile substep.
- `src/impact/destructible-render.cpp`: the generated-mesh VBO upload is cached
  by entity id + `geometryRevision` + vertex count and only re-uploaded when the
  mesh actually changes.
- `src/impact/destructible-selftest.cpp`: added checks for outward-consistent
  winding, the chunk broadphase subset, and a real
  `simulateProjectileTick` sweep against the crate producing `EntityImpact`
  (the exact path that routes a rifle hit into the cut).

# Validation

- `python build.py build-only` linked `C:\mimita-v9\mimita.exe` (18:09 local),
  no compile errors.
- `mimita.exe --destructible-selftest` reported PASS for all 22 checks,
  including `generated triangles have outward-consistent winding`,
  `chunk broadphase limits a near query to nearby triangles`, and
  `projectile sweep detects the crate as EntityImpact`.
- `mimita.exe --moving-crate-selftest` still reported PASS (no motion
  regression).
- Runtime validation remains open: spawn a crate with `crate_spawn`, shoot it
  with the projectile rifle, and confirm (1) no backfaces, (2) a visible hole
  appears on each hit, and (3) FPS stays normal while standing on/next to the
  crate.

# Residual notes

- The temporary `World` bridge in `collectActorEntityContacts` still exists
  (marked TODO-DELETE Phase 2); the chunk ranges reduce its cost but a future
  pass should query cached world triangles directly.
- Client-side projectile prediction does not yet test physical entities, so a
  predicted bullet can still visually pass through a crate until the server's
  explode/cut arrives. Server authority is unchanged.

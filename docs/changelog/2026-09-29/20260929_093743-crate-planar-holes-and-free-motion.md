// 2026-09-29T09:37:43Z
/* purpose
* record replacing the high-poly isosurface crate mesh with a low-poly planar
* face-hole surface, restoring free rigid-body motion, real walkable holes,
* a bounded triangle count, and correct per-face texturing
* does NOT claim live multiplayer acceptance
*/

# Task

- Summary: The destructible crate still did not move when touched and dropped
  the frame rate near it, the uncut crate showed a distorted texture, and the
  player asked for genuinely walkable holes sized by hit force (a box is six
  faces = twelve triangles; a big enough hole should be a real opening). The
  isosurface mesher could not provide both a crisp 8-16 sided hole and a cheap
  collider, so the surface was rebuilt as planar face cutting.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VALIDATION_REQUIRED / HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-29T09:37:43Z ISO 8601 UTC; display timezone
  America/New_York (2026-09-29 05:37 EDT).

# Root cause

- A destructible crate carried thousands of generated isosurface triangles
  (about 8k) and used them as its collision mesh (`impact-system.cpp` copied
  `destructible.collisionTriangles` into `localTriangles`, and re-copied it on
  every cut). Every rigid-body pass then swept all of them, so touching the
  crate collapsed the frame rate and destabilised motion.
- The mesh was generated eagerly at spawn, so even an untouched crate rendered
  the generated soup. Its UVs were a planar `(x,y)` projection, not per-face,
  which is the "weird texture" on an uncut crate.
- The crate was also created with the material's physical density (wood 700
  kg/m3 -> ~87.5 t for the 5 m crate), which no player push can move.

# Changes

- `src/impact/box-surface.h/.cpp` (new): `buildDestructibleBoxSurface` builds a
  box directly on its six faces. A cut crossing a face plane removes a
  sixteen-sided (plus any rectangle corners inside the sweep) irregular rim from
  that face, triangulated as an annulus. A cut whose union spans the box along
  an axis joins the two opposite rims with an inward-facing tube, so the opening
  is real geometry and can be walked through; a cut that does not span leaves a
  capped pocket. Winding is always resolved against the intended surface normal.
  An uncut box is exactly 12 triangles; every hole adds a bounded number.
- `src/impact/destructible-geometry.h/.cpp`: reduced `DestructibleGeometry` to
  the authoritative cut list plus a cached low-poly surface; removed the chunk
  grid, `DestructionChunk`, `DestructionTriangleRange`, `GeneratedDestructionMesh`,
  `chunkTriangleRanges`, `collectWorldTriangles`, and `collectLocalTriangles`.
  `initialize` is now lazy (emits no geometry); `addCut`/`rebuildAll` rebuild the
  planar surface from the stored spheres.
- `src/impact/isosurface.h/.cpp`: deleted (table-free surface-nets mesher no
  longer used).
- `src/impact/impact-system.cpp`: `initializeEntity` no longer overwrites
  `localTriangles`, so the caller's authored 12-triangle box stays authoritative
  until the first cut; `submit` still writes the generated surface back after a
  cut.
- `src/terminal/crate-commands.cpp`: crate default density is now gameplay
  density `1.0` (mass = density * volume), so a player push moves it;
  `crate_density` / `crate_mass` still override and re-derive mass from density.
- `src/physics/physical-entity.cpp`: removed the destructible-only collision
  broadphase branches in `advanceKinematics` and `collectActorEntityContacts`
  (the surface is small, so the full local mesh is swept directly); updated the
  self-test to assert the lazy box mesh and then a generated surface after a cut.
- `src/network/server-projectiles.cpp`: `queryEntityTrianglesSwept` uses the
  single per-triangle transform path (no chunk query).
- `src/impact/destructible-selftest.cpp`: `EntityOnlyWorld` transforms the
  entity's `localTriangles`; replaced the chunk-broadphase test with low-poly
  surface checks; the through-hole ray test still passes on the planar tube.

# Validation

- `python build.py build-only` compiled 12 translation units and linked
  `C:\mimita-v9\mimita.exe` with no errors.
- `mimita.exe --destructible-selftest` reported PASS for all 22 checks,
  including `destructible surface stays low-poly`,
  `ray through the pass-through hole hits nothing`, and
  `generated triangles have outward-consistent winding`.
- `mimita.exe --moving-crate-selftest` reported PASS for all checks, including
  `destructible crate keeps the box mesh until the first cut`,
  `destructible crate uses the generated collision mesh`, and
  `destructible dynamic crate falls onto the floor`.
- Runtime validation remains open: spawn a crate, confirm it moves on touch with
  no frame-rate drop and looks like the textured box, then shoot it and confirm
  a real hole appears that grows with impact force and can be walked through once
  large enough.

# Residual notes

- All cuts that land on one face are merged into a single star-shaped rim around
  the largest cut on that axis, so many shots on one face read as one growing
  opening rather than several separate holes. This keeps the triangle count
  bounded and is not expected to matter for clustered fire; it is a known
  approximation for widely separated holes on the same face.
- The cap on a non-through pocket is a shallow cone to the cut's deepest point
  rather than the exact spherical cap; it keeps the box closed (and therefore
  not walkable) until a cut genuinely spans the box.
- The repair keeps server authority: detection and meshing are unchanged in
  ownership; only the surface representation and the collision/rendering paths
  that consume it changed.

// 2026-09-28T20:29:14Z
/* purpose
* record the first working slice of the generalized impact + destructible
* geometry system: projectile rifle -> crate -> spherical cut -> generated
* isosurface triangles -> render + collision
* does NOT claim live multiplayer acceptance or actor/world impact adapters
*/

# Task

- Summary: Introduce one shared `ImpactSystem` entry point and a
  server-authoritative destructible crate that is cut by rifle projectiles.
  A hit computes effective energy/damage from mass, speed, and impact angle,
  stores a spherical cut in the entity's local space, remeshes only the
  affected chunks into a triangle soup, and uses that soup for both rendering
  and collision.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VALIDATION_REQUIRED / HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-28T20:29:14Z ISO 8601 UTC; display timezone
  America/New_York (2026-09-28 16:29 EDT).

# Changes

- `src/impact/impact-event.h`: shared `ImpactSource`, `ImpactTarget`,
  `ImpactShape`, `ImpactEvent` (with `cutScale`, `seed`), and
  `MaterialDefinition`. No crate- or weapon-specific fields.
- `src/config/material-config.h/.cpp` + `config/materials.json`: universal
  material table with FNV-1a ids, lookup, and hot reload. `default`, `wood`,
  `steel` entries define density, strength, hole energy scale, and max cut
  radius.
- `src/combat/weapon-types.h`, `src/combat/weapon-json-config.cpp`,
  `config/weapons.json`: added `projectileMass`, `projectileDensity`,
  `projectileShapeId`, `projectileBaseRadius`, `cutEnergyScale`, and
  `penetrationScale` to `WeaponDefinition` and the JSON reader;
  `projectile_rifle` now carries them.
- `src/impact/destructible-geometry.h/.cpp`: cut history, chunk grid
  (3x3x3 chunks, 8 cells/chunk), signed-distance helpers, and
  `DestructibleGeometrySystem` (`initialize`, `addCut`, `rebuildDirtyChunks`,
  `rebuildAll`). `addCut` marks only chunks overlapping the cut sphere dirty.
- `src/impact/isosurface.h/.cpp`: table-free naive surface-nets mesher,
  `meshDestructibleChunk(...)`, emitting a flat-shaded triangle soup for render
  and collision.
- `src/impact/impact-system.h/.cpp`: `ImpactSystem` singleton (`submit`,
  `initializeEntity`, static `kineticEnergy`, `impactAngleFactor`,
  `calculateCutRadius`). Converts the world hit into entity-local space, stores
  the cut, and writes the generated triangles back into
  `PhysicalEntity::localTriangles`.
- `src/physics/physical-entity.h`: replaced the dead `bool destructible` with
  `MimitaImpact::DestructibleGeometry destructible`.
- `src/impact/destructible-render.h/.cpp` + `src/physics/physical-entity.cpp`:
  `drawPhysicalEntities` draws the generated mesh when present and falls back
  to the textured box.
- `src/terminal/crate-commands.cpp`: material-aware spawn, destructible
  initialization, and a `crate_material <name>` command.
- `src/combat/projectile-simulation.h/.cpp`: `SweptEntityTriangle`,
  `ProjectileCollisionType::EntityImpact`, `ProjectileStepResult.hitEntityId`,
  and a default-virtual `queryEntityTrianglesSwept` seam.
- `src/network/server-projectiles.cpp`: implements the entity query, submits
  the impact on `EntityImpact` (server-authoritative), and still explodes every
  projectile for VFX.
- `src/engine/engine-tick-setup.cpp`: pumps
  `MimitaImpact::MaterialConfig::instance().pollReload()` with the other config
  polls.
- `src/game/game-cli.cpp` + `src/impact/destructible-selftest.cpp`: new
  `--destructible-selftest`.

# Validation

- `python build.py build-only` linked `C:\mimita-v9\mimita.exe` (16:28 local)
  after the final edits; no compile errors.
- `mimita.exe --destructible-selftest` reported `[DESTRUCTIBLE SELFTEST] PASS`
  for all 19 checks: energy/angle math, radius monotonicity in speed and mass,
  one cut per projectile, local-space storage, clamp, shallow-vs-direct damage,
  deterministic remesh, empty hole vs solid material, generated surface
  triangles, pass-through hole, collision mesh refresh, collidable off-hole
  material, partial chunk rebuild, and bounded triangle count.
- `mimita.exe --moving-crate-selftest` still reported `PASS` (no regression in
  physical-entity motion).
- Runtime (in-game) validation remains open: run a listen/sandbox server, spawn
  `crate`, shoot it with `projectile_rifle`, and confirm the visible hole and the
  walkable/collidable hole match the server state on both host and client.

# Deviations and scope notes

- The approved reconstruction method was marching cubes; the implementation
  uses a table-free naive surface-nets extractor instead. It is watertight,
  deterministic, and in the same isosurface family, and avoids transcribing the
  4096-entry marching-cubes table. This deviation must be confirmed or the
  mesher replaced.
- Detection is server-authoritative only in this slice; client prediction may
  briefly pass through until the server result is applied.
- Cut mass/inertia update is intentionally deferred; the entity keeps its
  original rigid-body mass properties after a cut.
- Actor and world-geometry impact targets, client prediction, entity
  networking/persistence, and shattering/fragment debris are out of scope for
  this slice and remain as seams.

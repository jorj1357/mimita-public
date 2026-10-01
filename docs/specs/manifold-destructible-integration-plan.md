// 2026-09-30T18:16:01Z (2026-09-30 14:16:01 EDT)
/* purpose
* Record the vendored Manifold dependency, the verified API, the MiMITA boolean
* wrapper, the local-space cut pipeline, and the exact next-agent steps for true
* 3D boolean destruction.
* Serve as the single handoff for continuing boolean destruction work.
* Does NOT define gameplay behavior; see docs/specs/destructible-world and
* docs/specs/moving-physical-objects for desired behavior.
* Does NOT replace AGENTS.md or docs/ROUTER.md.
*/

# Manifold Destructible Integration Plan

Status: dependency vendored, wrapper implemented, gameplay slice wired and
self-tested. Mass/centre-of-mass/inertia follow the cut surface, the rebuild is
incremental, arbitrary watertight GLBs can be imported as destructible objects,
and one server replicates destruction to all clients. Automatic fracture and
runtime/human acceptance remain.

Related documents:

- `docs/specs/destructible-world/destructible-world.md`
- `docs/specs/moving-physical-objects/moving-physical-objects.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/performance/performance.md`
- `docs/operations/build-and-exe/build-and-exe.md`

---

## 1. Desired behavior

1. A projectile hits the crate.
2. The client immediately shows a hole at the actual hit location.
3. The hole is a real empty volume, not a surface mark.
4. Shooting the same location again deepens or widens the hole.
5. A hole can eventually pass completely through.
6. Players, projectiles, NPCs, and other clients all use the changed geometry.
7. The crate keeps its movement, tipping, pushing, collision, and physics.
8. The crate is not recreated as a new physics object after every shot.

The old surface-nets path (`src/impact/box-surface.cpp`, deleted) sampled a
box-minus-spheres field on a grid bounded exactly to the box. It had no
outside-air border, so the boundary shell eroded and the whole crate shrank; the
whole-box grid was too coarse for small holes, so hits were lost or misplaced.

Manifold replaces that with a true CSG subtraction.

---

## 2. Vendored dependency record

- Location: `external/manifold` (git submodule).
- Repository: https://github.com/elalish/manifold
- Pinned release: **v3.5.4**
- Pinned commit: **`ce50d78021d64507f89e8c9fc2c2e51018117857`**
  (tag object message "Release v3.5.4")
- License: **Apache License 2.0**, `external/manifold/LICENSE`.
- NOTICE file: none. `external/manifold/AUTHORS` lists contributors.
- Local modifications: none (submodule working tree clean at the pinned commit).
- `.gitmodules` entry added:
  `[submodule "external/manifold"] path = external/manifold url = https://github.com/elalish/manifold.git`

Build configuration used (static, Release, C++ API only):

```text
-G Ninja
-DCMAKE_BUILD_TYPE=Release
-DBUILD_SHARED_LIBS=OFF
-DMANIFOLD_PAR=OFF
-DMANIFOLD_TEST=OFF
-DMANIFOLD_CBIND=OFF
-DMANIFOLD_PYBIND=OFF
-DMANIFOLD_DEBUG=OFF
-DMANIFOLD_CROSS_SECTION=OFF
-DCMAKE_CXX_COMPILER=C:/important/msys64/mingw64/bin/g++.exe
```

Toolchain actually used:

- CMake 4.2.0 (winlibs distribution).
- Ninja (winlibs distribution).
- Compiler: the same g++ build.py uses, **GCC 16.2.0**
  (`C:\important\msys64\mingw64\bin\g++.exe`, resolved by `build_toolchain.py`).
  Building Manifold with the game's compiler keeps the static lib ABI-compatible
  instead of mixing the winlibs GCC 15.2.0 runtime.

Installed output:

- `external/manifold-prebuilt/lib/libmanifold.a` (about 2.0 MB).
- Headers at `external/manifold-prebuilt/include/manifold/`.
- Required consumer define: `MANIFOLD_PAR=-1` (Manifold emits this as a PUBLIC
  compile definition when the parallel backend is off; it must match or the
  header/ABI contract is inconsistent).

The prebuilt directory is Git-ignored (regenerable). Reproduce with:

```text
python tools/build_manifold.py
```

`build.py` fails with a clear message if the prebuilt library is missing.

Windows note: Manifold documents `BUILD_SHARED_LIBS=OFF` for Windows builds.
Parallelization (`MANIFOLD_PAR=ON`) requires TBB and is intentionally disabled
so TBB cannot add another runtime or performance variable.

---

## 3. Verified Manifold API (as of the pinned commit)

Include only in the wrapper:

```cpp
#include <manifold/manifold.h>
#include <manifold/mesh.h>
#include <manifold/version.h>   // MANIFOLD_VERSION_MAJOR/MINOR/PATCH
```

Core operations confirmed present:

- `manifold::MeshGL` (`MeshGLP<float, uint32_t>`) and `manifold::MeshGL64`
  (`MeshGLP<double, uint64_t>`).
- `Manifold(const MeshGL&)` and `Manifold(const MeshGL64&)`.
- `MeshGL GetMeshGL(int normalIdx = -1) const`, `GetMeshGL64(...)`.
- `Manifold::Cube(vec3 size, bool center)`, `Manifold::Sphere(double radius,
  int circularSegments)`, `Manifold::Cylinder(double height, double radiusLow,
  double radiusHigh, int circularSegments, bool center)`.
- `Manifold Boolean(const Manifold&, OpType) const`, and `operator-`, `operator+`,
  `operator^`; `BatchBoolean`.
- `Error Status() const`; `bool IsEmpty() const`; `size_t NumTri()/NumVert()`;
  `double Volume()`; `std::vector<Manifold> Decompose()`;
  `Manifold Simplify(double tolerance = 0) const`;
  `Manifold CalculateNormals(int normalIdx = 0, double minSharpAngle = 52.5) const`.
- `int OriginalID()`, `AsOriginal(int)`, `static uint32_t ReserveIDs(uint32_t)`.
- `Error` values include `NoError`, `NonFiniteVertex`, `NotManifold`,
  `VertexOutOfBounds`, `PropertiesWrongLength`, `MissingPositionProperties`,
  `MergeVectorsDifferentLengths`, `MergeIndexOutOfBounds`,
  `TransformWrongLength`, `RunIndexWrongLength`, `FaceIDWrongLength`,
  `InvalidConstruction`, `ResultTooLarge`, `InvalidTangents`, `Cancelled`.

`MeshGL` fields used or inspected:

- `numProp` (properties per vertex, >= 3), `vertProperties` (interleaved floats,
  positions first), `triVerts` (3 indices per triangle, CCW from outside).
- `mergeFromVert` / `mergeToVert`: declare manifold topology across property
  seams (duplicated positions with different UV/normals).
- `runIndex`, `runOriginalID`, `runTransform`, `runFlags`, `faceID`,
  `halfedgeTangent`, `tolerance`.

Verified behavioral facts (see `tools/manifold_probe.cpp` output):

- A hand-built 12-triangle cube imports with `Status()==NoError` and
  `Volume()==1.0`.
- `cube - Sphere(0.3, 32)` returns `NoError`, volume `0.8894` (true sphere
  `0.8869`; the polyhedral cutter removes slightly less).
- **`Decompose()` splits by connected surface shell, not solid region.** An
  interior cavity returns two shells: the outer solid (positive volume) and the
  cavity wall (negative volume). Solid pieces = shells with `Volume() > 0`.
- Output `runOriginalID` distinguishes surfaces: the original base keeps its
  reserved ID; surfaces created by the cutter carry the cutter's ID. This is how
  materials/UVs can be re-applied.
- **A `GetMeshGL()` output has property seams and cannot be re-imported without
  merge vectors.** Never feed a previous output back in as the base; always
  replay the ordered cut history against the canonical base and chain in-memory
  (`booleanSubtractAll`).
- Manifold's automatic segment count (`circularSegments = 0`) is too coarse for
  small cutters; the wrapper scales segments with radius.
- Manifold has **no `Capsule` primitive**; the wrapper builds one as
  `Cylinder + Sphere + Sphere`.
- Manifold is compiled with `-ffp-contract=off` and
  `-fexcess-precision=standard`, so boolean results are reproducible for the
  same inputs.

---

## 4. Current MiMITA architecture map

### 4.1 Ownership

- `PhysicalEntity` (`src/physics/physical-entity.h:44`) owns identity, transform,
  orientation, linear/angular velocity, mass, density, center of mass, inertia,
  half extents, friction/restitution, sleep state, `localTriangles`, and the
  embedded `MimitaImpact::DestructibleGeometry`.
- `PhysicalEntitySystem` (`src/physics/physical-entity.cpp:186`) is the singleton
  owner. `advanceKinematics` runs the fixed 60 Hz integration, gravity, righting
  torque, sleeping, player push, and entity pair contacts.
- `ImpactSystem` (`src/impact/impact-system.cpp`) is the single impact entry
  point. It computes energy, angle factor, cut radius, converts the hit to local
  space, and stores the cut.
- `DestructibleGeometrySystem` (`src/impact/destructible-geometry.cpp`) owns the
  canonical base mesh + cut history and rebuilds the cached surface.
- `drawPhysicalEntities` (`src/physics/physical-entity.cpp:862`) prefers
  `drawGeneratedEntityMesh` (`src/impact/destructible-render.cpp`) and falls back
  to the textured box.

### 4.2 Cut pipeline (current)

```text
projectile sweep (src/combat/projectile-simulation.cpp)
  -> EntityImpact { hitEntityId, hitPosition, hitNormal, impactSpeed }
  -> server: src/network/server-projectiles.cpp submitEntityImpact -> ImpactSystem::submit
  -> client prediction: src/network/multiplayer-projectiles.cpp -> ImpactSystem::submit
  -> ImpactSystem: worldPoint -> inverse(entity.transform) -> local center
  -> DestructibleGeometrySystem::addCut(canonical base + history)
  -> booleanSubtractAll -> BooleanMesh
  -> renderVertices + CollisionTriangle cache + geometryRevision++
  -> PhysicalEntitySystem::localTriangles = collision cache
```

Entity-local triangles are consumed as world space by `collectActorEntityContacts`
(`src/physics/physical-entity.cpp:696`) and the dynamic collision sweep
(`src/physics/physical-entity.cpp:317`).

### 4.3 Networking today

- The server is authoritative (`src/network/server-projectiles.cpp:1773`), and
  the local client predicts (`src/network/multiplayer-projectiles.cpp:1828`).
- **There is no physical-entity network identity and no cut/impact packet.**
  `EntityType` is only `NONE/PLAYER/NPC`. `ProjectileExplodeEventPacket` reports
  a crate hit with reason `"entity"` but carries no crate id or cut data.
- In a listen server both sides share one in-process `ImpactSystem`, and
  `ImpactSystem::submit` de-duplicates by `(sourceEntityId, local center)`.
  Separate-process clients will diverge because each computes cuts only from its
  own projectile simulation.

---

## 5. Ownership rules (do not break)

The boolean geometry system may only:

- Replace the entity's local render and collision geometry.
- Increment `geometryRevision` exactly once per applied cut.
- Update cut-derived diagnostics (volume, component count).

It must NOT:

- Delete and respawn the entity.
- Reset velocity, rotation, or tipping state.
- Recalculate movement from scratch.
- Create a second physics owner.

A boolean cut after `submit` keeps the same `PhysicalEntity` pointer/id,
position, orientation, linear velocity, angular velocity, density, and (for this
milestone) mass and inertia.

A through-hole stays one valid closed shell with an empty tunnel. If the result
has multiple positive-volume components, they remain one geometry owner for now;
the count is reported and logged, and fracture is a later milestone.

---

## 6. MiMITA-owned boolean wrapper

Files: `src/impact/boolean-mesh.h` (plain MiMITA types) and
`src/impact/boolean-mesh.cpp` (**the only translation unit that includes
Manifold headers**).

Types:

- `BooleanMeshVertex { position, normal, uv, materialId }`.
- `BooleanMesh { vertices, indices }` (triangle soup; indices wind CCW outward).
- `BooleanCutterType { Sphere, Capsule }`.
- `BooleanCutter { type, localCenter, localDirection, radius, length }`.
- `BooleanError { None, InvalidTarget, NotManifold, MissingPositionProperties,
  PropertiesWrongLength, InvalidCutter, EmptyResult, ResultTooLarge, Internal }`.
- `BooleanCutResult { success, mesh, remainingVolume, triangleCount, shellCount,
  componentCount, error, message }`.

Functions:

- `BooleanMesh buildBooleanBoxMesh(halfExtents, materialId)` — canonical
  textured box (8 shared vertices, 12 triangles, planar XY UVs).
- `BooleanCutResult booleanSubtractAll(base, cutters)` — authoritative replay:
  imports the base once, subtracts each cutter in order **in memory**, then
  converts once. Use this, not repeated `GetMeshGL` round-trips.
- `BooleanCutResult booleanSubtract(base, cutter)` — single-cut convenience.
- `BooleanError booleanValidate(mesh, reason)` — pre-flight manifold check.

Implementation rules:

- Game code knows only MiMITA structures.
- Only the wrapper includes Manifold headers.
- The wrapper converts MiMITA meshes to `MeshGL`, subtracts, and converts back.
- Manifold errors map to `BooleanError`; the wrapper does not log.
- Cutters are built in local space; capsules are synthesized from primitives.
- Cutter segment count scales with radius (clamped 8..48).
- Source UVs are preserved for base-run triangles; new cavity/tunnel triangles
  get a deterministic triplanar fallback UV.

---

## 7. Local-space cut flow

All boolean operations happen in the crate's local coordinate system:

```text
world hit point/direction
  -> local point  = inverse(entity.transform) * (worldPoint, 1)
  -> local dir    = inverse(mat3(entity.transform)) * worldDirection   (capsule)
  -> cutter size  = existing energy/angle/material math (ImpactSystem)
  -> BooleanCutter (local space)
  -> booleanSubtractAll(canonical base, history)
  -> render + collision triangles
  -> geometryRevision++
```

Transform audit (verified): the crate uses a standard right-handed transform
with `transform[3]` = world position, quaternion orientation in `orientation`,
no non-uniform scale; `inverse` is used for point conversion exactly as the
projectile query transforms local triangles to world. Winding is outward CCW on
input (matches `buildBoxCollisionTriangles`) and preserved by Manifold. Projectile
hits are swept, not point-tested, so tunneling is already handled.

Client and server share this exact conversion through the shared `ImpactSystem`.

---

## 8. First gameplay slice (implemented)

- One spawned movable crate (`crate_spawn`), one projectile rifle, sphere cutter.
- Server-authoritative cut and local client prediction both route through
  `ImpactSystem::submit`.
- Listen-server double-apply prevented by `(sourceEntityId, local center)`.
- Preserves the moving crate: same entity, same rigid-body state; only geometry
  and `geometryRevision` change.
- Diagnostics: successful cuts rate-limited (every 4th), failures always logged
  via `Debug::error` with entity, source, radius, cut count, tri count, volume,
  component/shell counts, and boolean milliseconds.

Not yet in this slice: separate-process cut replication, capsule gameplay use,
mass/inertia recomputation, fracture, imported meshes, simplification tuning.

---

## 9. Prediction and replication determinism

Client (immediate prediction):

```text
local projectile hit
  -> build the same BooleanCutter and call the same ImpactSystem::submit
  -> display changed geometry immediately
  -> send impact intent (projectile/hit claim)
```

Server (authority): validates projectile and hit, applies the cut, broadcasts an
authoritative cut event. All clients apply the same cut once.

Implemented (2026-09-30) as four packets in `src/network/packets.h`, driven by
`serverReplicatePhysicalEntities` and applied by
`src/network/multiplayer-physical-entities.cpp`:

1. `PACKET_PHYSICAL_ENTITY_SPAWN` (reliable): network id, motion, material,
   half extents, pose, density, and either a box source or a GLB `modelPath`.
   The client rebuilds the same canonical base — no triangles are sent.
2. `PACKET_ENTITY_CUT_EVENT` (reliable): network id, ordered `cutId`, prediction
   key, cutter type/center/direction/radius/length, damage, energy. Applied in
   order; a re-sent `cutId` is ignored, matching section 9.3's explicit identity
   `(networkId, cutId)` instead of approximate position.
3. `PACKET_PHYSICAL_ENTITY_STATE` (unreliable, 30 Hz): transform/velocity for
   visible motion between reliable events.
4. `PACKET_PHYSICAL_ENTITY_DESPAWN` (reliable): removes the mirror.

Mirrors are created through `PhysicalEntitySystem::addReplicated`, marked
`serverDriven`, and are skipped by the fixed tick, entity-vs-entity contacts,
player push, and `ImpactSystem::submit`, so a client can never become an
authority for the cut history. Entities use their runtime id as the network id.

Remaining: a dedicated server owns no `PhysicalEntity` crates, so server-side
destruction there needs a server spawn path. Do not send generated triangle
meshes over the network; do not allow clients to send arbitrary geometry.

---

## 10. Texture and triangle handling

- The crate base mesh carries planar XY UVs so the uncut box matches the current
  textured look.
- Manifold preserves base-run UVs; the wrapper reads them for triangles whose
  `runOriginalID` is the reserved base run.
- New cavity/tunnel surfaces have no meaningful source UV and use a deterministic
  triplanar fallback based on the face normal.
- Material is currently a single crate material; per-run material mapping is
  possible via `runOriginalID` once a material system needs it.
- Normals are recomputed per output triangle (flat, matching the current
  PSX-style surface). `CalculateNormals` is available if smooth shading is
  wanted later.
- Do not blind-weld triangles across a hole. If triangle count must drop, use
  Manifold `Simplify(tolerance)` only when the result stays closed and the
  tolerance cannot erase a hole or move a hole away from its hit point.

---

## 10.1 Cut size (force × projectile size)

The generated hole radius is config-driven from two independent contributions,
so a small projectile with huge force can cut a big hole and a big projectile
with little force a small one:

```text
sourceRadius = max(weapon.projectile_radius, weapon.projectile_base_radius)
forceRadius  = cbrt(energy * angle * cut_energy_scale * material.holeEnergyScale)
               * kCutRadiusEnergyScale
sizeRadius   = sourceRadius * weapon.cut_radius_scale
radius       = clamp(sourceRadius + forceRadius + sizeRadius,
                     sourceRadius, material.maxCutRadius)
```

- `cut_energy_scale` (force) and `cut_radius_scale` (size) live in
  `config/weapons.json` per weapon and are hot-reloadable.
- `cut_radius_scale = 0` makes the hole force-only.
- `ImpactEvent::sizeScale` carries the size term into the shared ImpactSystem so
  no weapon-specific logic leaks in.
- Both the server and the local prediction build the same event, so the hole is
  identical on all clients.

Penetration: `penetration_count` (default 1) lets a bolt cut through that many
surfaces in one shot before stopping, boring a tunnel. Default 1 preserves the
single-surface behavior; raising it is the "shoot through" mode.

---

## 11. Diagnostics and performance

Implemented diagnostics: per cut — entity id, source, radius, cut count, input
and output triangle counts, remaining volume, component/shell counts, boolean
duration; failures recorded separately (invalid target, invalid cutter, boolean
error, empty result, excessive triangle count).

Destruction queue + budget (2026-09-30, second pass):

- `ImpactSystem::submit` no longer rebuilds synchronously; it appends the cut to
  the entity's authoritative history (`enqueueCut`) and returns immediately.
- `ImpactSystem::flushPendingCuts` runs once per fixed tick from
  `PhysicalEntitySystem::advanceKinematics` and applies **one batched rebuild
  per entity** regardless of how many cuts are queued. It enforces
  `kMaxCutsPerEntityPerTick` and a wall-clock `kCutBudgetMsPerTick`, so a burst
  can never blow a frame; leftover cuts drain on later ticks.
- A cut that removes no material (`BooleanCutResult::changed == false`) skips the
  O(triangle) mesh export, surface conversion, mass integration, and revision
  bump entirely.
- Diagnostics: `[DESTRUCTION BUDGET]` when a flush pauses at the budget,
  `[DESTRUCTION QUEUE]` when cuts are still pending after a flush.

Known costs and guidance (2026-09-30 status):

- `DestructibleGeometrySystem::rebuild` is incremental now
  (`booleanSubtractIncremental` caches the running in-memory `Manifold`), so a
  new cut only subtracts the new cutter. A full replay still happens after a
  rollback or a mesh re-initialize.
- `collectActorEntityContacts` caches each entity's world-space expansion per
  `(id, geometryRevision, transform)` *and* builds an `AabbTree` over it, passed
  to `collectActorMeshContactsInto`, so the per-iteration narrowphase prunes by
  the tree instead of scanning every crate triangle.
- The projectiles' `queryEntityTrianglesSwept` (client and server) no longer
  allocate an intermediate world-triangle vector; each triangle is transformed
  and AABB-rejected inline.
- Still linear/costly: the swept projectile query still transforms all entity
  triangles of an overlapping entity (no per-entity tree for projectiles yet).
  A per-entity projectile AabbTree and triangle simplification are the next perf
  steps if the batched flush is not enough.
- The generated-mesh render path uploads only when `(entity id, geometryRevision,
  vertex count)` changes. The box fallback re-uploads every frame.
- Do not allocate large temporary arrays inside fixed-tick collision loops. Keep
  the 60 Hz fixed gameplay rule.

Target: a single human-scale crate boolean should complete within the agreed
budget (measure `bool=...ms`); if not, move the CSG to a worker and swap at a
safe simulation boundary without changing authority.

---

## 12. Acceptance tests

Implemented / passing (`mimita.exe --destructible-selftest`,
`mimita.exe --moving-crate-selftest`):

- Impact math and cut radius monotonicity.
- One projectile stores exactly one local-space cut.
- Localized cut does **not** shrink the whole crate (regression for the old bug).
- Hole interior is empty; material away from the hole stays solid.
- Pass-through hole is genuinely empty along the tunnel axis.
- Collision mesh updates after a cut; crate stays collidable away from the hole.
- Generated triangles stay low-poly and within budget.
- Outward-consistent winding.
- Projectile sweep detects the crate (`EntityImpact`).
- Same `PhysicalEntity` falls, rests, collides; sleep and player push still work.

Still to add:

- Multiple separated cuts; repeated same-spot deepening.
- No-op when a shot misses.
- Component count reported for a fractured/disconnected result (fracture still
  off).
- Networking (deterministic self-test
  `mimita.exe --destruction-replication-selftest`, passing): mirror is built from
  the spawn event and marked `serverDriven`; mirror geometry, volume, and mass
  match the server after the ordered cut events; a re-sent cut is ignored; the
  state packet moves the mirror; despawn removes only the mirror.
- Import (`mimita.exe --destructible-selftest`): an authored octahedron and a
  real watertight GLB import, cut, and lose mass; a non-watertight GLB and a
  missing file are rejected with a reason.
- Human visual/multiplayer acceptance (not performed): two real clients must see
  the same hole, and a GLB object must render and cut in-game.

---

## 13. Follow-up implementation notes

### 13.1 Mass, center of mass, and inertia

Implemented 2026-09-30. Owner: `src/physics/mesh-mass-properties.{h,cpp}`
integrates volume, center of mass, and the unit-density inertia diagonal from the
closed cut surface; `DestructibleGeometrySystem::rebuild` caches them per
`geometryRevision`; `refreshMassProperties` (`src/physics/physical-entity.cpp`)
reads the cache (`mass = density * remainingVolume`) and `ImpactSystem::submit`
refreshes the entity immediately after a cut. Option (a), the diagonal
approximation, was chosen; principal axes are still not modeled.

Original notes (still the reference for option (b)):

- `mass = density * volume` is set at spawn; `refreshBoxMassProperties`
  (`src/physics/physical-entity.cpp:50`) derives a diagonal box inertia from
  `halfExtents` and `mass`. `centerOfMass` stays at the local origin.
- Impulses use `centerOfMass` and a diagonal `inverseInertia` in local space
  (`applyImpulseAtPoint`, `resolveWorldContactVelocity`).

How to update after a cut:

1. `mass = density * result.remainingVolume` (volume already returned by the
   wrapper). Volume is exact for the boolean output, so this is free.
2. Center of mass and inertia require integrating over the closed output mesh.
   With a closed, consistently-wound triangle mesh, use the standard
   tetrahedron/signed-volume integrals (sum over triangles of the tetrahedron
   formed with the origin): volume, first moment (for the centroid), and the
   3x3 inertia tensor about the centroid. These are ~30 lines of arithmetic and
   run over the existing output triangles.
3. `PhysicalEntity::inertia` is a diagonal vector applied in local space; if the
   computed tensor has significant off-diagonal terms, either (a) keep only the
   tensor diagonal as an approximation, or (b) store the principal axes and
   rotate into them. Option (a) is acceptable for the first milestone; option (b)
   needs a `principalInertiaRotation` on the entity and updates to
   `inverseInertiaWorld`.
4. Recompute only when `geometryRevision` changes, once per applied cut, and
   clamp to the existing minimums.
5. Keep gameplay density (not material density) unless the design changes; only
   the volume factor should change mass.

Do NOT change movement, velocity, or orientation when applying mass properties;
only mass/COM/inertia.

### 13.2 Performance

- Make rebuild incremental: keep the running in-memory `Manifold` for an entity
  (opaque to game code; owned by the wrapper) and subtract only the new cutter,
  while retaining the authoritative cut history for reconciliation/rebuild.
- If a full rebuild is ever required, replay against the canonical base only.
- Add `(geometryRevision, transform)` caching for the entity world-triangle
  expansion used by `collectActorEntityContacts`.
- Cap work per tick: budget the boolean duration, defer or reject cuts beyond
  the budget, and log the category (`ResultTooLarge`, excessive time).
- For 999-shot bursts, batch multiple pending cutters into one `booleanSubtractAll`
  call before converting, rather than one subtraction+conversion per shot.
- Consider a worker thread only after measuring; the swap must stay synchronized
  with the 60 Hz tick and must not change authority.

### 13.3 Geometry breadth (arbitrary GLB triangles)

Goal: import arbitrary triangles from GLB files and let them be destructible.
The wrapper is already shape-agnostic (`BooleanMesh`); the missing piece is a
GLB -> `BooleanMesh` converter and mesh-derived physics, not the CSG.

Facts found:

- `loadGLB` (`src/map/map_loader.cpp:134`) returns a `Mesh` but always uploads
  textures, so it requires a GL context.
- `walkGLBScene` (`src/map/map-loader-gltf-nodes.cpp:187`) is the CPU, GL-free
  path that yields positions, normals, UVs, and per-primitive batches. It is
  already used off-thread (with a zero texture table) by `WeaponModelCache`.
- `loadActorWeaponTriangles` (`src/physics/movement/actor-collision-mesh.cpp:297`)
  is a path-cached, GL-free loader that returns `CollisionTriangle`s and is a
  good template for a cached model loader.
- The loader is a triangle soup with **no welding**, and coordinates are baked to
  scene space; `Mesh::Batch` associates a material/texture per primitive.
- `drawPhysicalEntities` ignores `modelPath` today; only the generated mesh and
  the fallback box are drawn.

Implementation path:

1. Add a GL-free "load model to BooleanMesh" function using `walkGLBScene`
   (positions + UVs + per-batch material id), recentering to local space and
   recording the offset/half-extents. Cache by path and share across entities
   (mirror `WeaponModelCache`).
2. Handle manifoldness: GLB solids are often not watertight at seams. Either
   require watertight authored meshes, or weld coincident vertices into
   `mergeFromVert`/`mergeToVert` on the base `MeshGL` and validate with
   `booleanValidate`. Reject and log meshes that are not closed instead of
   feeding them into gameplay.
3. Preserve per-vertex UVs and per-run material through the boolean. The base
   input must carry `numProp >= 5` (position + UV) and a reserved
   `runOriginalID`; extend the wrapper to carry a material id per vertex/run
   rather than a single base material.
4. Generalize the box-only assumptions: the SDF fallback and
   `buildBooleanBoxMesh` are box-specific; a general base mesh needs its own
   bounds instead of `halfExtents`, and fracture/mass use the general mesh
   integrals from 13.1.
5. Render arbitrary meshes: add a GLB-backed entity render path (like
   `drawGeneratedEntityMesh` but for imported geometry) and bind the model's
   texture; currently entity rendering has no imported-model path.

Constraints: Manifold import requires an oriented, closed, 2-manifold mesh.
Invalid input must be rejected and logged, never passed into gameplay.

---

## 14. Risks and explicit limits

- This milestone targets human-scale crates. It does not claim that 0.01 m
  projectiles, planets, or galaxies are solved.
- Boolean accuracy depends on float precision, mesh scale, cutter resolution,
  triangle density, tolerances, collision resolution, and render/network budgets.
- Mass, center of mass, and diagonal inertia now follow the material that
  remains after each cut (diagonal approximation; the full tensor and principal
  axes are not modeled).
- Automatic fracture is implemented but was not runtime-tuned. The balance
  heuristic is geometric (volume + horizontal center-of-mass drift), not a real
  support/contact polygon, so expect to retune `FractureTuning` after human
  testing. See 13.4.
- Multiplayer cut replication now exists for one server; a dedicated server still
  does not own any `PhysicalEntity` crates, so destruction there needs a
  server-side spawn path.
- Manifold's auto segment count is coarse; the wrapper overrides it, but very
  small cutters still need enough segments to look round.

---

## 13.4 Fracture (implemented — best attempt, needs runtime tuning)

Fracture splits one object into several independent rigid bodies when a cut
disconnects or unbalances it. Implements `destructible-world.md` sections 13, 14,
20, 21, 24, 27, 28, and 31.

Detection and ownership:

1. `BooleanCutResult.componentCount` (from `Manifold::Decompose()`, positive
   shells) drives the disconnected case: `> 1` means the object broke apart.
2. `evaluateFracture` (`destructible-geometry.{h,cpp}`) also fires on an
   unbalanced single piece: material was removed and the horizontal center of
   mass drifted past `comOffsetFraction` of the half extent. Tuning lives in
   `FractureTuning` (aggressive by default; retune after human testing).
3. `ImpactSystem::applyFracture` is the single owner. `booleanDecomposePieces`
   returns each closed shell largest-volume-first; the hit entity keeps
   `pieces[0]` (stable runtime and network id), and every other accepted piece
   becomes a new Dynamic `PhysicalEntity`.
4. Material inheritance (`destructible-world.md` 13/14): children copy material
   id, density, friction, restitution, damping, texture, and collidesWithActors.
   Mass comes from the piece's own mesh volume via `initializeEntityFromMesh`.
5. Velocity seeding: `v = v_parent + omega_parent x (r - com_parent)` at the
   piece centroid, so off-center pieces fly outward.
6. Budget (`destructible-world.md` 44): `maxFragmentsPerEvent` caps bodies per
   event; `minPieceVolumeFraction` keeps tiny slivers welded.

Networking (`destructible-world.md` 28, "replicate causes"):

- Fragment network ids are deterministic:
  `0x40000000 | (parentNetworkId << 4) | pieceIndex`. The high bit avoids
  colliding with small runtime ids.
- The server and every client rebuild the same base and apply the same ordered
  cuts, so they derive the same pieces in the same order with no extra packet.
- A client runs the same `applyFracture(..., serverDriven=true)` after applying a
  replicated cut, so mirrors match the server; the server's own spawn packets
  reconcile by network id.

Known limits to tune after human acceptance: the balance heuristic is
geometric only (no real support/contact polygon); a fragment is still one mesh
shell (no further chipping); dedicated-server crate spawn is still absent;
`FractureTuning` is code-level, not yet hot-reloadable config.

---

## 15. Exact next-agent instructions

1. Read `AGENTS.md`, `docs/ROUTER.md`, and this document.
2. Do not remove or replace `PhysicalEntity`; do not delete existing physics.
3. Rebuild the dependency if needed: `python tools/build_manifold.py`.
4. Keep Manifold headers inside `src/impact/boolean-mesh.cpp` only.
5. Preserve the canonical-base + cut-history model; never feed a previous output
   back in as the base.
6. Mass/COM/inertia (13.1), incremental performance (13.2), GLB breadth (13.3),
   one-server multiplayer replication (section 9), and automatic fracture (13.4)
   are implemented. Runtime/human acceptance and fracture tuning remain.
7. Separate source, dependency-build, MiMITA-build, selftest, runtime, and human
   acceptance evidence. Do not claim visual or multiplayer success until the
   running client and server visibly demonstrate it.
9. Record a changelog for every repository-touching session.

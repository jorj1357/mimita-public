// 2026-09-27
/* purpose
* inventory every active collision owner before the actor-triangle migration
* record input geometry, world geometry, correction, velocity response, and order
* give the migration one authoritative map of what must be kept, merged, or removed
* does NOT delete or change any collision code
* does NOT decide the final architecture; see the migration plan and changelog
* does NOT describe rendering, audio, damage, or networking transport
*/

# Actor-triangle collision owner inventory

Date: 2026-09-27
Status: Phase 1 analysis (no code deleted)
Scope: local Player collision and every path reachable from it, plus the
non-Player paths that share the same primitives.

## How collision is reached

```text
physics-mini.cpp:237  physicsMainUpdate_Internal
  -> doCollisions(...)                 physics-collision-dispatch.cpp:101
       world.collisionMesh non-empty  -> doGLBTriangleCollisions(...)   physics-collision-glb-main.cpp:126
       else                           -> legacy block path             physics-collision-dispatch.cpp:134
```

`doCollisions` is shared by:

- local Player (`physics-mini.cpp`)
- NPC bodies (`npc.cpp:1144`, `npc.body` is a `Player`; body/weapon phase is
  skipped for NPCs at `glb-main.cpp:151`)
- replay simulation (`sim/simulate-tick.cpp:96`)

Fixed 60 Hz. `doCollisions` decays `p.collision.bounceCooldown` once per call
(`physics-collision-dispatch.cpp:112`) and calls
`recoverInvalidPlayerCollisionState` (NaN/inf recovery only) before and after.

## Owner table (GLB path)

Legend: `pos` = position correction, `vel` = velocity response, `zeroes` =
can set velocity to exactly zero, `order` = call order within the frame.

| # | Owner | File:function | Input geometry | World geometry | pos | vel | zeroes | order |
|---|-------|---------------|----------------|----------------|-----|-----|--------|-------|
| 1 | Body+weapon phase | `physics-collision-glb-body.cpp:189` `doBodyWeaponCollisionPhase` | body **mesh triangles** (`collectBodyMeshContacts`, `physics-collision-mesh.cpp:220`) when `bodyMeshCollision()`; else one AABB sphere per part (`collectBodyWeaponSpheres`); weapon JSON spheres; optional weapon **capsule** (`p.weaponCollisionCapsule`) | GLB triangles | `solveBatchedCorrection` (`physics-collision-core.cpp:207`), clamp 0.5/pass, 1.5 total | `respondVelocityAgainstNormal` on last pass (`physics-collision-shared.h:80`) | no (projects) | before sweep |
| 2 | Root capsule sweep/slide | `physics-collision-glb-sweep-slide.cpp:60` `doGLBSweepSlide` | root `Capsule` + body sample points (`collectPlayerBodyCollisionSamples`, `physics-collision-body.cpp:263`) | GLB triangles via `gatherGLBTriangles` | move to TOI, step-up, seam, `+normal*SURFACE_SLOP` | `applyCollisionContact` (`physics-collision-core.cpp:126`) | `vel.z=0` on step and ground | after body |
| 3 | Batched depenetration | `physics-collision-glb-main.cpp:184` | root `Capsule` | GLB triangles | `solveBatchedCorrection`, 4 iters, clamp 2.0 | `applyCollisionContact` | no | after sweep |
| 4 | Floor recovery | `physics-collision-glb-safety.cpp:128` `doFloorRecovery` | root `Capsule` | GLB triangles | lift `pos.z` | clamps `vel.z<=0`, `externalImpulse.z<=0` | clamps z only | after depen |
| 5 | Emergency stuck escape | `physics-collision-glb-main.cpp:227` | root `Capsule`, radial search | GLB triangles | teleport `p.pos` | **`p.vel = glm::vec3(0)`** (`:318`) | yes, fully | after floor |
| 6 | Stuck tracking | `physics-collision-glb-main.cpp:331` | root `Capsule` | GLB triangles | none | none | no | after emergency |
| 7 | Debug visuals | `physics-collision-glb-main.cpp:373` | root `Capsule` + body samples | GLB triangles | none | none | no | last |

### Owner 1 detail: body `collectBodyMeshContacts`

The only real actor-triangle path today. Per `physicalBody.parts`:

- transforms `part.collider.triangles` by `part.worldTransform` (current) and
  `part.previousWorldTransform` (sweep start)
- requires triangle-plane agreement before barycentric containment
  (`pointInTriangle`, `physics-collision-mesh.cpp:34`)
- accepts a current overlap as penetration; accepts swept crossings strictly
  after the sweep starts (`segmentTriangleIntersectAfterStart`, `:96`)
- ignores a contact that exists only at the old pose
- normal points from the world surface toward the actor
- dedups by `(label, world triangle)`, keeps deepest
- budgets: `kMaxContactsPerPart = 64`, `kMaxTriangleTests = 200000`
- broadphase: one `appendChunkTrianglesForAABB` per part over the merged
  current+previous AABB

It does **not** include the weapon, headless actors, or remote actors, and it
does not own the velocity response.

### Owner 1 detail: weapon sphere/capsule

- `collectBodyWeaponSpheres` (`physics-collision-body.cpp:97`) emits one sphere
  per body part (only when mesh body collision is off) plus JSON weapon spheres.
- `recomputeWeaponCapsule` (`physics-collision-body.cpp:37`) builds
  `p.weaponCollisionCapsule` from `weaponCollisionWorld` and grip/muzzle/radius.
- `collectBodyWeaponContacts` (`physics-collision-body.cpp:160`) tests spheres
  against a union AABB of candidates (`appendChunkTrianglesForAABB`).
- The weapon has **no triangle geometry**; collision is data-driven from
  `config/weaponcollisions.json` and weapon model bounds.

## Owners that are defined but not called (dead)

| Function | File | Evidence |
|----------|------|----------|
| `doGroundSnap` | `physics-collision-glb-safety.cpp:51` | no caller outside this file/header |
| `doRotationSafetyPass` | `physics-collision-glb-safety.cpp:185` | no caller |
| `doFinalSafetyPass` | `physics-collision-glb-safety.cpp:240` | no caller |

`glb-main.cpp` calls only `doFloorRecovery`. These three are candidates for
removal after the triangle owner is proven.

## Non-Player / out-of-frame consumers of the capsule primitives

| Consumer | File | Uses | Notes |
|----------|------|------|-------|
| Block world | `physics-collision-dispatch.cpp:134-507` | AABB blocks, capsule sweep/depen/snap | active only when `collisionMesh` empty; **out of scope (kept)** |
| Remote geometry safety | `network/multiplayer-interpolation.cpp:941` `resolveRemoteBodyAgainstGeometry` | `collectCapsuleRecoveryContacts` | client render-side clamp for remote bodies |
| Ragdoll | `physics/physical-body.cpp:337` `depenetrateStatic` | `collectCapsuleRecoveryContacts` | rigid-body vs world |
| Player vs player | `physics-collision-block.cpp:412` `resolveCapsuleVsCapsule` | root capsules | player/NPC separation |
| Stress/selftest | `physics-collision-stress.cpp` | capsule gather + recovery | deterministic tests |
| Server body template | `network/server-body-template.cpp` | CPU GLB parse -> **AABB** body parts | headless hit validation, "no invisible capsules" |

## Broadphase / allocation constraints

- `gatherGLBTriangles` (`physics-collision-glb-setup.cpp:287`) is the cached path
  (per-frame AABB cache + superset). Use it or `appendChunkTrianglesForAABB` with
  reusable buffers.
- `gatherGLBTrianglesForSphere` (uncached, allocates) must not enter a
  per-sphere/per-part hot loop (`docs/architecture/collision/collision.md:42`).
- `PhysicsScratch` (`physics-scratch.h`) provides reusable `ints`, `contacts`,
  `vec3s`.

## Phase 2 — one generic actor-triangle input (added)

`src/physics/movement/actor-collision-mesh.{h,cpp}` now owns the single actor
collision representation:

- `ActorCollisionMesh { label; localTriangles; previousTransform;
  desiredTransform; affectsMovement }`.
- `collectActorCollisionMeshes(Player&)` returns body parts (in
  `physicalBody.parts` order, node-local triangles + part world/previous
  transforms) followed by the weapon, all through the same representation.
- `commitActorCollisionMeshes(Player&)` advances the weapon sweep-start
  transform.
- GL-free loaders: `loadActorBodyMeshParts` (node-local body triangles),
  `loadActorWeaponTriangles` (weapon render-mesh triangles in the same
  model-local space the renderer uses, node hierarchy baked), and the
  `ensureActor*` populators.
- `Player::loadModelColliders(path)` builds the skeleton + colliders with no GL,
  no render mesh, and no cache write, so NPC/headless/replay actors get real
  body triangles. `Player::loadModel` was refactored to call the same extracted
  CPU builders (`buildNodeHierarchyFromModel`,
  `syncPoseSkeletonFromRestLocal`, `buildBodyPartCollidersFromModel`) so the
  client and headless paths cannot drift.
- `Player` gained `weaponColliderMesh`, `weaponColliderMeshPath`,
  `previousWeaponModelTransform`.

The active collision path is still unchanged; the collector is not yet called by
`doCollisions`. Wiring it in is Phase 3+.

## Phase 3 — the single triangle solver (added)

`src/physics/movement/actor-triangle-solver.{h,cpp}` owns:

- `ActorWorldContact { normal; point; impactVelocity; penetration;
  timeOfImpact; worldTriangle; actorPart }`.
- `ActorTriangleCollisionResult`.
- `solveActorTriangleCollision(Player&, const World&, desiredMovement,
  ActorTriangleCollisionResult&)`.

Design and rules:

- Captures the actor meshes **once** (safe previous + desired). Corrections
  accumulate as a translation and are applied once at the end, so the safe pose
  is never overwritten mid-solve. This is what stops a floor depenetration from
  being re-read as a fresh downward sweep (the bug that flipped the floor
  normal during development).
- Broadphase: one union swept AABB (`makeSweptActorMeshAABB`) plus
  `appendChunkTrianglesForAABB`; candidates are gathered once and reused.
- Contacts: `collectActorMeshContacts` (shared with the legacy body path) tests
  every mesh triangle swept from safe to desired plus current-pose penetration,
  rejects t=0 old-pose hits, and orients normals toward the actor pose center.
- Manifold: `mergeContactsByNormal` combines contacts whose normals agree within
  0.95, keeping the deepest penetration and strongest part impact — one response
  per surface, not per part.
- Position: `solveBatchedCorrection` per iteration, clamped; final penetration
  validation runs over up to 4 iterations.
- Grounding: from the actor's own pose box lowest point, not the capsule. A
  walkable normal near the lowest point grounds; a walkable contact high on the
  body (hand on a ledge) does not.
- `physics-collision-mesh.cpp` was refactored: the triangle math and budget loop
  moved into `collectActorMeshContacts`; `collectBodyMeshContacts` now builds
  body meshes, gathers candidates, and calls it, so the legacy path and the
  solver share one contact implementation.

`doCollisions` still does not call the solver; it runs alongside the legacy
pipeline and is proven by `--actor-triangle-solve-selftest`.

## Grounded / contact facts

- `applyCollisionContact` (`physics-collision-core.cpp:126`) sets
  `realWorldContactThisFrame`, `hasWorldContact`, `worldContactLostTimer`, and
  for walkable normals near the feet sets `groundedThisFrame` and emits
  `MovementContactKind::Ground`. This is already triangle-oriented and can be
  driven by the actor solver.
- `appendPlayerMovementContact*` (`physics-collision-core.cpp:71,107`) writes
  `p.movementContacts`, consumed as `MovementCollisionFeedback` in
  `physics-mini.cpp:245`.

## Spark boundary (must not change)

- `physics-collision-glb-body.cpp:108` calls
  `EffectPartSystem::instance().spawnBodyContactSpark(p.pos, c.point, p.vel, 0.1f)`
  once per tick via `p.bodySparkTick`. The spark only needs the final contact
  point. `config/hitfx.json` and `src/effects/hit-effects*` are out of scope.

## Open questions carried into the migration

1. ~~Headless/CPU-only body triangles for NPC and server actors (Phase 0 spike 1).~~
   Resolved GO — see `actor-triangle-phase0-spike.md`.
2. ~~Weapon render-mesh triangles exposed per actor (Phase 0 spike 2).~~
   Resolved GO — see `actor-triangle-phase0-spike.md`.
3. ~~Remote/NPC weapon transform + mesh for the solver (Phase 0 spike 3).~~
   Resolved GO — see `actor-triangle-phase0-spike.md`.
4. Whether server hit-validation AABBs (`server-body-template.cpp`) must become
   the same triangle representation or stay a documented boundary.
5. Whether the dead safety passes are removed in Phase 7 cleanup.

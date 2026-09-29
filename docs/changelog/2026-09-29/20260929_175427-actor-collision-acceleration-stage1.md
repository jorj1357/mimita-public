# Actor collision acceleration: cached world AABBs, zero hot-path allocation, and measurement

Time (UTC): `2026-09-29T21:54:27Z`
Time (local): `2026-09-29 17:54:27 EDT`
Branch: `afad20a-rebuild`
Base commit: `0d0999fa`

## Summary

First implementation increment of the accelerated actor-collision plan
(Stages 0, 1, 3 of the agreed plan). Behavior is unchanged: every existing
collision self-test passes. The change removes two proven hot-path costs and
adds the counters needed to decide whether a BVH is required.

- **Stage 1 — cached world triangle AABBs.** `CollisionMeshCache` now carries a
  `triangleAABBs` array parallel to `triangles`, filled once by
  `buildCollisionChunks`. The actor narrowphase and the broadphase overlap
  filters read the cache instead of recomputing `makeTriangleAABB` inside a
  per-pair loop.
- **Stage 3 — no per-iteration allocations in the actor solver.** The
  correction loop reuses thread-local scratch vectors for the shifted meshes,
  contacts, accumulated contacts, and manifold. `collectActorMeshContacts` gained
  a scratch-buffer form (`collectActorMeshContactsInto`).
- **Stage 3b — crate/entity fix.** `collectActorEntityContacts` no longer
  allocates a `World`, a triangle vector, and a candidate vector per entity per
  correction iteration; it refills reused per-thread storage and fills the
  cached triangle AABBs so the entity narrowphase is accelerated too.
- **Stage 0 — instrumentation.** `gActorNarrowphase` counts candidate pairs,
  triangle tests, and rounded-feature calls per solve; the existing
  `collision.solve.summary` event now reports candidates, these counters, and
  `solve_ms`.

Stages 2 (BVH) and 6 (comparison mode) are intentionally deferred until the new
counters are read, per the agreed "measure first" decision.

## Files and exact changes

### `src/physics/physics-types.h`

- `CollisionMeshCache` gained `std::vector<AABB> triangleAABBs;`; `clear()` clears
  it. This changes `sizeof(World)`, so the dependency-tracked build recompiles
  every dependent translation unit (verified: the `.d` dependency files drive
  recompilation).

### `src/physics/movement/physics-collision-shared.h`

- Added `inline AABB collisionTriangleAABB(const CollisionMeshCache&, int)` with a
  live `makeTriangleAABB` fallback for callers that build triangles without the
  cache (tests, direct pushes).
- Added `struct ActorNarrowphaseStats` and `extern ActorNarrowphaseStats
  gActorNarrowphase`.
- Added the `collectActorMeshContactsInto(...)` declaration (with default
  `filterCandidatesByMeshAabb = true`, `contactSkin = -1.0f`).

### `src/map/map-loader-collision.cpp`

- `buildCollisionChunks` resizes `world.collisionMesh.triangleAABBs` and fills it
  from the triangle bounds already computed for chunk assignment. This covers the
  GLB loader, the live re-decimate path, and every self-test that calls
  `buildCollisionChunks`.

### `src/physics/movement/physics-collision.cpp`

- The no-chunk fallback, `visitTriangle`, and `visitLarge` now read
  `collisionTriangleAABB` instead of recomputing bounds.

### `src/physics/movement/physics-collision-mesh.cpp`

- Defined `gActorNarrowphase`.
- Converted `collectActorMeshContacts` into the scratch-buffer form
  `collectActorMeshContactsInto` (accumulates into `out`, merges via a reused
  thread-local buffer), and kept the returning `collectActorMeshContacts` as a
  thin wrapper.
- Uses `collisionTriangleAABB` for candidate bounds.
- Increments `candidatePairs`, `triangleTests`, and `roundedFeatureCalls`.

### `src/physics/movement/physics-collision-body.cpp`

- The body/weapon sphere narrowphase uses `collisionTriangleAABB`.

### `src/physics/movement/actor-triangle-solver.cpp`

- Added `<chrono>`; resets `gActorNarrowphase` and starts a solve timer.
- The correction loop uses thread-local `s_meshes`, `s_contacts`, `s_allContacts`,
  and `s_manifold` instead of allocating per iteration; the stale
  `TODO-DELETE [Phase 2]` mesh-copy comment was removed because it is done.
- `collision.solve.summary` now includes `candidates`, `candidate_pairs`,
  `triangle_tests`, `rounded_feature_calls`, and `solve_ms`.

### `src/physics/physical-entity.cpp`

- `collectActorEntityContacts` uses reused per-thread `s_entityWorld` and
  `s_entityCandidates`, fills `triangleAABBs` for the entity triangles, and
  removed the temporary `World` allocation and its `TODO-DELETE [Phase 2]`
  comment.

## Reasoning

The chunk + 4^3 subgrid broadphase already narrows static-world candidates, so
the dominant per-tick costs are the per-pair bounds recomputation and the
per-iteration allocations, not the candidate gather. Caching bounds and reusing
scratch is semantics-neutral and removes those costs without changing the
narrowphase, response, rounding shell, or contact-merge rules. The entity path
was the worst offender because it allocated a temporary `World` per entity per
correction iteration; that is the reported "FPS drop when touching a crate".

## Documents and skills

- Read: `docs/ROUTER.md`, `docs/architecture/collision/collision.md`,
  `docs/skills/efficiency-checker-v1.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/regressions/README.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- `docs/skills/efficiency-checker-v1.md`: applied. Confirmed findings:
  `makeTriangleAABB` recomputed per actor-triangle/candidate pair
  (`physics-collision-mesh.cpp` inner loop) and per-iteration allocations in
  `actor-triangle-solver.cpp`; the entity temp-`World` allocation
  (`physical-entity.cpp`). Smallest corrective direction implemented.
- Spec note: `docs/architecture/collision/collision.md` requires the cached
  broadphase path and no per-query allocations in hot loops; this change
  strengthens both. `MOVEMENT_FEATURE_SMOOTHNESS` and the response/seam rules are
  untouched.

## Evidence (separated)

Source evidence: files listed above.

Build evidence:

- `python build_agent.py` cold builds: one intermediate build failed with
  `too few arguments to function 'collectActorMeshContactsInto'` (defaults were
  declared after the first compile); defaults were added and the next build
  returned `Status: SUCCESS`, return code 0. Executable
  `C:\mimita-v9\mimita.exe`.

Focused test evidence (self-tests run against the built executable):

- `--actor-triangle-solve-selftest`: 26 PASS / 0 FAIL, exit 0.
- `--collision-selftest`: 19 PASS / 0 FAIL, exit 0.
- `--moving-crate-selftest`: 23 PASS / 0 FAIL, exit 0.
- `--collision-subgrid-selftest`: PASS, candidate sets still match brute force.

Runtime/log evidence and live gameplay acceptance: **not performed this
session.** The new `collision.solve.summary` counters still need to be read in
game to decide whether Stage 2 (BVH) is required.

## Human review still needed

- Play near the reported cylinder and crate. Confirm the crate FPS drop is gone
  or reduced and no collision behavior changed (grounding, bounce, slopes,
  seams).
- Read the `collision.solve.summary` counters (candidate pairs, triangle tests,
  rounded-feature calls, solve_ms) in a dense map to decide whether to build the
  BVH (Stage 2) or stop here.

## Pre-existing edits

This session's earlier aimbody/lifecycle and dev-loop changes, plus the prior
sessions' config/ragdoll/networking work, were committed by the repository owner
as `0d0999fa` ("pre of commit fixing for the idk colsiions too brute force and
clinders issue") during this session. Nothing was reverted or claimed. The
collision changes in this changelog remain uncommitted. The aimbody changelog and
cold-build debt file had pre-existing uncommitted edits that were preserved.

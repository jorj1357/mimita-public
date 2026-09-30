# Actor collision acceleration: cached AABBs, AABB-tree pair traversal, scratch, comparison mode

Time (UTC): `2026-09-29T21:54:27Z`
Time (local): `2026-09-29 17:54:27 EDT`
Branch: `afad20a-rebuild`
Base commit: `0d0999fa`

## Summary

Implements the accelerated actor-collision plan (Stages 0, 1, 2, 3, 6). The
narrowphase, response, rounded-feature shell, and contact-merge rules are
unchanged; only the bounds computation, allocation behavior, and pair discovery
changed. Every existing collision self-test passes with acceleration enabled.

- **Stage 1 — cached world triangle AABBs.** `CollisionMeshCache` carries a
  `triangleAABBs` array parallel to `triangles`, filled once by
  `buildCollisionChunks`. The narrowphase and broadphase overlap filters read the
  cache instead of recomputing `makeTriangleAABB` per pair.
- **Stage 2 — AABB-tree pair traversal.** New header-only `AabbTree`
  (`collision-aabb-tree.h`). `solveActorTriangleCollision` builds one tree over
  the gathered world candidates per solve and reuses it across correction
  iterations; `collectActorMeshContactsInto` queries it per swept actor triangle
  so only AABB-overlapping candidates reach the narrowphase.
- **Stage 3 — no per-iteration allocations.** The correction loop reuses
  thread-local scratch; `collectActorMeshContacts` gained a scratch-buffer form.
- **Stage 3b — crate/entity fix.** `collectActorEntityContacts` no longer
  allocates a `World` + triangle vector + candidate vector per entity per
  iteration; it refills reused per-thread storage and fills cached AABBs.
- **Stage 6 — comparison mode.** Hot-reloadable `actorCollisionAccelerated`
  (default true) and `actorCollisionComparison` (default false). Comparison runs
  the linear scan and the tree path on the same input, logs a structured diff
  (`collision.narrowphase.compare`), and keeps the linear result.
- **Stage 0 — instrumentation.** `gActorNarrowphase` counts candidate pairs,
  triangle tests, and rounded-feature calls; `collision.solve.summary` reports
  those plus `candidates` and `solve_ms`.

## Files and exact changes

### New: `src/physics/movement/collision-aabb-tree.h`

Header-only flat binary AABB tree. `build(primitives, aabbs, leafSize)` copies
the primitive bounds and permutes a position order with median split on the
largest centroid axis; `query(q, out)` traverses an explicit 128-entry stack and
tests each leaf primitive's own AABB before returning it. Deterministic for a
given input.

### New: `tests/collision-aabb-tree-test.cpp`

Standalone test: empty build, full/empty overlap, query-vs-brute-force parity on
a 400-primitive grid, no duplicates, and parity at leaf sizes 1/2/8.

### `src/physics/physics-types.h`

- `CollisionMeshCache` gained `std::vector<AABB> triangleAABBs;`; `clear()` clears
  it. This changes `sizeof(World)`, so the dependency-tracked build recompiles
  every dependent translation unit.

### `src/physics/movement/physics-collision-shared.h`

- `inline AABB collisionTriangleAABB(const CollisionMeshCache&, int)` with a live
  fallback for callers without the cache.
- `struct ActorNarrowphaseStats` + `extern gActorNarrowphase`.
- `collectActorMeshContactsInto(...)` now takes `const AabbTree* worldTree` and
  `bool comparison` (both defaulted); forward-declares `struct AabbTree`.

### `src/map/map-loader-collision.cpp`

- `buildCollisionChunks` fills `triangleAABBs` from the bounds already computed
  for chunk assignment, covering the loader, live re-decimate, and self-tests.

### `src/physics/movement/physics-collision.cpp` / `physics-collision-body.cpp`

- Overlap filters use `collisionTriangleAABB`.

### `src/physics/movement/physics-collision-mesh.cpp`

- Defined `gActorNarrowphase`; counts candidate pairs, triangle tests, and
  rounded-feature calls.
- `collectActorMeshContacts` is now a thin wrapper over
  `collectActorMeshContactsInto`; merges through a reused thread-local buffer.
- Added `logActorNarrowphaseComparison` (structured diff keyed by world triangle +
  actor part label: counts, missing/extra, max penetration delta, `set_equal`).
- The per-actor-triangle candidate loop queries the AABB tree when supplied;
  otherwise it scans the gathered candidate list (original behavior).

### `src/physics/movement/actor-triangle-solver.cpp`

- Added `<chrono>`, the tree include, a solve timer, and `gActorNarrowphase`
  reset.
- Builds one `AabbTree` over the candidates per solve (only when
  `actorCollisionAccelerated` and the cached AABBs are complete) and passes it,
  with the comparison flag, into the narrowphase.
- Correction loop uses thread-local `s_meshes`/`s_contacts`/`s_allContacts`/
  `s_manifold`; `collision.solve.summary` includes candidates, candidate pairs,
  triangle tests, rounded-feature calls, and `solve_ms`.

### `src/physics/physical-entity.cpp`

- `collectActorEntityContacts` uses reused per-thread `s_entityWorld` and
  `s_entityCandidates`, fills cached AABBs, and removed the temporary `World`
  allocation.

### `src/config/collision-config.{h,cpp}` and `config/collision.json`

- Added `actorCollisionAccelerated` (default true) and `actorCollisionComparison`
  (default false), parsed and hot-reloadable.

## Reasoning

The chunk + 4^3 subgrid already narrows static-world candidates, so the dominant
per-tick costs were per-pair bounds recomputation and per-iteration allocations,
not the candidate gather. Caching bounds and reusing scratch removes those. The
AABB tree prunes the remaining candidate scan to AABB-overlapping pairs while
preserving the exact narrowphase. The entity path was the worst offender because
it allocated a temporary `World` per entity per iteration — the reported crate
FPS drop. Comparison mode makes the acceleration falsifiable before it becomes
the sole owner.

## Documents and skills

- Read: `docs/ROUTER.md`, `docs/architecture/collision/collision.md`,
  `docs/skills/efficiency-checker-v1.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/regressions/README.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- `docs/skills/efficiency-checker-v1.md`: applied. Confirmed the per-pair
  `makeTriangleAABB`, per-iteration allocations, and the entity temp-`World`
  allocation; implemented the smallest corrective direction.
- `docs/architecture/collision/collision.md`: the cached broadphase rule and the
  no-hot-path-allocation rule are strengthened; `MOVEMENT_FEATURE_SMOOTHNESS` and
  the response/seam rules are untouched.

## Evidence (separated)

Source evidence: files listed above.

Build evidence:

- `python build_agent.py` cold builds: one failed with
  `too few arguments to function 'collectActorMeshContactsInto'` (defaults were
  declared after the first compile); after adding defaults and fixing the tree
  leaf test, builds returned `Status: SUCCESS`, return code 0, executable
  `C:\mimita-v9\mimita.exe`.

Focused test evidence:

- `tests/collision-aabb-tree-test.cpp`: `5 passed, 0 failed`
  (`g++ -std=c++17 -O2 -Iinclude -Isrc -DGLM_ENABLE_EXPERIMENTAL`).
- Game self-tests with acceleration enabled (default):
  `--actor-triangle-solve-selftest` 26/26, `--collision-selftest` 19/19,
  `--moving-crate-selftest` 23/23, `--collision-subgrid-selftest` 9/9, all exit 0.
- Comparison-mode smoke: with `actorCollisionComparison: true`,
  `--actor-triangle-solve-selftest` still 26/26, exit 0 (linear result kept);
  flag restored to false.

Runtime/log evidence and live gameplay acceptance: **not performed this
session.** The `collision.solve.summary` counters and
`collision.narrowphase.compare` diff still need to be read in a dense map.

## Human review still needed

- Play near the reported cylinder and crate. Confirm the FPS drop is gone or
  reduced and collision feel is unchanged (grounding, bounce, slopes, seams).
- Read `collision.solve.summary` (`candidate_pairs`, `triangle_tests`,
  `rounded_feature_calls`, `solve_ms`) in a dense map and confirm the tree
  reduced candidate pairs.
- Optionally enable `actorCollisionComparison` and confirm
  `collision.narrowphase.compare` reports `set_equal: true`.

## Pre-existing edits

This session's earlier aimbody/lifecycle and dev-loop changes, plus prior
sessions' config/ragdoll/networking work, were committed by the repository owner
as `0d0999fa` during this session. Nothing was reverted or claimed. The collision
changes in this changelog remain uncommitted. Pre-existing uncommitted edits to
the aimbody changelog and the cold-build debt file were preserved.

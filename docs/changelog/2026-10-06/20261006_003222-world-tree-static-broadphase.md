# Persistent world AABB tree replaces the always-large static broadphase

- Time (UTC): `2026-10-06T00:32:22Z`
- Display timezone: America/New_York; display time: `2026-10-05 20:32:22 EDT`
- Branch: `afad20a-rebuild`
- Base commit: `51a63c5a` (`npc behavior nto so good ugh ugh uhg whatever`)
- Scope: remove the `collisionAlwaysLargeTriangles` per-query scan and replace it
  with one persistent world-level AABB tree over every static collision
  triangle. Narrowphase, movement integration, substep limits, and contact
  semantics are unchanged.

## Summary

The Zombie Tower 4 static broadphase had an always-scanned list of 978 "large"
triangles tested on every collision query (`appendChunkTrianglesForAABB`) and on
every ray traversal. This patch replaces that list with one persistent
`AabbTree` on `World`, built from `collisionMesh.triangleAABBs` whenever
`buildCollisionChunks` runs. `appendChunkTrianglesForAABB` now queries the tree
as its primary path; the chunk/sub-grid/coarse-grid structures remain the
fallback for worlds whose tree is not ready and for one compatibility caller.
The exact `triangleTriangleIntersect` narrowphase and
`collectActorMeshContactsInto` are untouched.

## Files and exact changes

### `src/world/world.h`

- Added `#include "physics/movement/collision-aabb-tree.h"`.
- Replaced `std::vector<int> collisionAlwaysLargeTriangles;` with
  `AabbTree collisionTree;` (persistent static broadphase).

### `src/world/world.cpp`

- `World::clear()`: replaced `collisionAlwaysLargeTriangles.clear();` with
  `collisionTree.clear();`.

### `src/map/map-loader-collision.h`

- Added shared `inline constexpr int kMaxChunksPerTriangle = 256;` so the builder
  and the actor-solve diagnostics use one large-triangle threshold.

### `src/map/map-loader-collision.cpp` (`buildCollisionChunks`)

- Removed the file-local `MAX_CHUNKS_PER_TRIANGLE`; uses `kMaxChunksPerTriangle`.
- Removed `collisionAlwaysLargeTriangles` population. Triangles whose coarse span
  exceeds `MAX_COARSE_CHUNKS_PER_TRIANGLE` are simply not indexed in the coarse
  grid (the tree covers them).
- After `triangleAABBs` is filled, builds `world.collisionTree` over all triangle
  ids `0..N-1` with `AabbTree::build`.
- `[WORLD GLB COLLISION]` log now reports `treeNodes=` instead of `always=`.

### `src/physics/movement/physics-collision.h` / `.cpp`

- `appendChunkTrianglesForAABB(...)` gained a defaulted
  `bool preferWorldTree = true`.
- Primary path: when `preferWorldTree` and the tree indexes every triangle,
  query `collisionTree` with the clamped query box grown by `expansion`
  (equivalent to the old `overlaps(clamped, triBounds ± expansion)` filter) and
  return. Logs `[CHUNK TREE]` with query box size, candidate count, elapsed ms.
- Removed the `collisionAlwaysLargeTriangles` loops from the chunk fallback in
  `appendChunkTrianglesForAABB`.
- `rayTraverseGridCells` and `sweptSphereTraverseGridCells`: hoisted the
  `visitLarge` lambda; removed the always-large loops; when a world tree exists,
  query it over the ray/swept AABB so triangles beyond the coarse grid are still
  tested (`s_triGen` dedups what the DDA/coarse pass already tested).

### `src/network/server-npcs.cpp`

- `[SERVER NPC WORLD]` log now reports `treeNodes=` instead of `alwaysLarge=`.

### `src/physics/movement/actor-triangle-solver.h` / `.cpp`

- `ActorTriangleCollisionResult` gained `largeTriangles`, `queryBoxSize`,
  `solveMs`, and `totalMs`.
- `solveActorTriangleCollision` records the query box size and counts returned
  candidates that exceed `kMaxChunksPerTriangle` chunks (bounded pass over the
  candidates) and records `solveMs`.
- `runActorTriangleCollisionStep` records `totalMs` (broadphase query +
  narrowphase/solve + support carry + entity push + spark) and emits a second
  structured record `collision.solve.frame` (`cylinder-static-broadphase`) gated
  by the existing `StructuredLogger::shouldLog(Collision, Verbose)`.
- `collision.solve.summary` gained `large_triangles` and `query_box`.

### `src/physics/physical-entity.cpp`

- Dynamic entity-vs-world sweep and deep recovery now call
  `appendChunkTrianglesForAABB(..., false)` so they keep the established
  chunk/sub-grid broadphase. The world tree changed which candidates the entity
  sweep returned for a body fully below a floor (the tree correctly returned the
  floor, but the shared rounded-feature normal then points into the surface and
  the entity was pushed deeper instead of recovered). Keeping this caller on the
  legacy path preserves the tested embedded-body recovery without redesigning
  the dynamic entity broadphase.

### Tests

- `tests/collision-aabb-tree-test.cpp`: added `testLongPrimitive` (near-middle
  returns the long primitive, far query excludes it). 6/6.
- `src/physics/movement/physics-collision-stress.cpp`
  (`collisionSubGridSelfTest`): added a long narrow triangle and a huge flat
  triangle; asserts the tree indexes every triangle, the huge triangle is not in
  the coarse fallback grid, far queries exclude the long/huge triangles, the
  mid query returns them, and every query set matches brute force. 17/17.
- `src/physics/movement/actor-triangle-solver.cpp`
  (`actorTriangleSolverSelfTest`): added a long-wall middle hit case. 28/28.
- `src/physics/physical-entity.cpp` (`physicalEntitySelfTest`): added a very
  large moving entity case (far entity filtered by `entityWorldAABB`/AABB pad,
  overlapping entity produces an exact triangle contact, huge dynamic entity
  settles on the floor). 29/29.

## Reasoning

The investigation (`docs/changelog/2026-10-05/20261005_185020-...`) identified
the always-large list as the per-query cost. The `AabbTree` already existed and
was already used per-solve for actor candidates, so the smallest correct change
was to make it persistent and world-level and query it instead of scanning a
list. `buildCollisionChunks` already computes every `triangleAABB`, so building
the tree there is a one-time map-load cost and keeps one build owner for client,
NPC, and self-test worlds. The tree query is the exact AABB-overlap set the chunk
path computed, so the narrowphase sees the same or more-correct candidates; the
always-large scan is gone.

The dynamic entity sweep keeps the chunk fallback because the tree surfaced a
latent interaction: a body fully below a floor produced a rounded proximity
contact whose normal points deeper, which the old chunk path masked by returning
no candidate. That is out of scope to redesign in this cylinder-focused patch,
and the spec explicitly permits keeping the fallback for compatibility paths.

## Documents and skills

- Read: `AGENTS.md`, `docs/ROUTER.md`,
  `docs/specs/movement/movement.md`, `docs/architecture/collision/collision.md`,
  `docs/specs/performance/performance.md`,
  `docs/skills/efficiency-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/architecture/collision/actor-triangle-owner-inventory.md`,
  `docs/changelog/2026-10-05/20261005_185020-cylinder-proximity-fps-investigation.md`,
  `docs/changelog/2026-09-29/20260929_175427-actor-collision-acceleration.md`.
- `docs/skills/efficiency-checker-v1.md`: applied. The always-large scan was a
  confirmed per-query cost; the tree removes it while preserving the cached
  broadphase rule and adding no per-query heap allocation (the tree query uses a
  fixed stack and appends into the caller buffer).
- `docs/skills/spec-behavior-review-v1.md`: applied. No spec-code disagreement
  found in movement/collision; the movement spec keeps the existing local
  collision feel, which the unchanged narrowphase preserves.

## Evidence (separated)

Source evidence: files listed above.

Build evidence (separate claim):

- `python build_agent.py` (with `MIMITA_FORCE_LINK=1`) -> `Status: SUCCESS`,
  return code 0, executable `C:\mimita-v9\mimita.exe`.
- A background `devscripts/dev-loop.py` was also compiling/relinking during the
  session; the canonical agent build was run with a forced link so the reported
  executable reflects this change set.

Focused test evidence (separate claim):

- `tests/collision-aabb-tree-test.cpp` (standalone,
  `g++ -std=c++17 -O2 -Iinclude -Isrc -DGLM_ENABLE_EXPERIMENTAL`): `6 passed,
  0 failed`.
- `mimita.exe --actor-triangle-solve-selftest`: 28 PASS, 0 FAIL.
- `mimita.exe --collision-subgrid-selftest`: 17 PASS, 0 FAIL.
- `mimita.exe --moving-crate-selftest`: 29 PASS, 0 FAIL.
- `mimita.exe --collision-selftest`: PASS.
- `mimita.exe --actor-collision-mesh-selftest`: 8 PASS, 0 FAIL.
- `mimita.exe --canonical-contact-selftest`: 23 PASS, 0 FAIL.
- `mimita.exe --physical-perf-selftest`: 2 PASS, 0 FAIL.

Runtime/log evidence: **not performed.** The Zombie Tower 4 load log
(`[WORLD GLB COLLISION] ... treeNodes=`) and the `collision.solve.summary` /
`collision.solve.frame` counters (`query_box`, `candidates`,
`large_triangles`, `triangle_tests`, `solve_ms`, `total_ms`) still need to be
read in a dense map near `Cylinder.091`.

Human gameplay acceptance: **not performed.**

## Human review still needed

- Load Zombie Tower 4 and confirm the visible collision geometry is unchanged
  and the `[WORLD GLB COLLISION]` line reports no `always=` (it now reports
  `treeNodes=`).
- Play near `Cylinder.091` and confirm the reported FPS drop is gone.
- Enable structured `Collision` `Verbose` logging and capture
  `collision.solve.summary` / `collision.solve.frame` near the cylinder; confirm
  `large_triangles`, `candidates`, `triangle_tests`, and `solve_ms`/`total_ms`
  are reduced versus the pre-change behavior.
- Confirm collision feel is unchanged (grounding, slopes, seams, weapon
  contacts, dynamic entity recovery).
- Confirm weapon hitscan / NPC line-of-sight still hit very large triangles
  (the ray traversals now use the world tree for triangles beyond the coarse
  grid).

## Known limitations / follow-ups

- A very large triangle whose AABB contains the query is still returned as a
  candidate; long-triangle subdivision or local clipping remains future work,
  exactly as the task states.
- Dynamic entity-vs-world keeps the chunk/sub-grid broadphase, so entity
  collision against triangles beyond the coarse grid relies on the existing
  coarse path rather than the world tree. Revisiting this needs the embedded-body
  normal/recovery interaction to be designed first.
- `docs/ROUTER.md` routes to `docs/regressions/regressions-v1.md`, which does not
  exist at that path; the file lives at
  `docs/regressions/archive-ignore/regressions-v1.md`. No edit made (spec/doc
  TODO noted per router instruction).

## Pre-existing edits (not made by this session)

The working tree already contained unrelated modifications before this session:
`config/accounts/default.json`, `config/analytics.json`,
`config/behavior-profiles.json`, `config/gamemodes/sandbox.json`,
`config/npc-difficulty.json`, `config/weapons.json`, `devscripts/dev-loop.py`,
`src/duel/duel-queue.cpp`, and the untracked
`docs/changelog/2026-10-05/20261005_201304-normal-weapon-sounds.md`. None were
edited, reverted, or claimed by this session.

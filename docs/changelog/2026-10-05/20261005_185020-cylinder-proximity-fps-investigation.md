# Cylinder-proximity FPS investigation

- Time: 2026-10-05T18:50:20Z (display timezone: America/New_York; display time: 2026-10-05 14:50:20 EDT)
- Branch: current checkout branch (not changed)
- Scope: investigation only; no code, configuration, asset, or runtime changes were made.
- Pre-existing working-tree changes: multiple modified source/config/docs files and the untracked `assets/maps/zombietower4.glb`; none were edited by this investigation.

## Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/performance/performance.md`
- `docs/architecture/collision/collision.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- relevant prior memory entry for the 2026-09-29 cylinder-proximity investigation

## Evidence

- `logs/10-05-2026/Gameterminal_log_144458.txt:5656-5681` records the active `assets/maps/zombietower4.glb` load: 113,769 collision triangles, 4,435 large triangles, 978 always-large triangles, 277,944 collision chunks, 1,180,838 chunk references, and 20,705,156 sub-grid references. Collision-chunk construction took 2,605.1 ms and the full map load took 3,232.6 ms.
- `logs/10-05-2026/Gameterminal_log_144458.txt:2016-2045` gives a comparison load for `funworld3.glb`: 41,326 collision triangles, 445 large triangles, zero always-large triangles, 31,474 chunks, 238,721 chunk references, 3,403,579 sub-grid references, and 356.5 ms for collision chunks.
- Read-only GLB inspection found 16 cylinder-named nodes in Zombie Tower 4. Their source meshes are generally only 79-144 triangles, but several nodes have very large scales (including approximately 999 on horizontal axes), making their transformed collision triangles spatially large.
- `config/collision.json` enables `bodyMeshCollision`, `actorTriangleSolver`, and `actorCollisionAccelerated`.
- `src/physics/movement/actor-triangle-solver.cpp:253-307` gathers a swept player AABB each solve and tests the gathered world candidates against actor collision meshes. `src/physics/movement/actor-triangle-solver.cpp:572-607` runs this once per fixed-tick player collision step.
- `src/physics/movement/physics-collision.cpp:182-229` adds large-triangle candidates, including every `collisionAlwaysLargeTriangles` entry, to each overlapping query after the chunk/sub-grid query.
- No matching `actorTriangleSolve`, `collision.narrowphase`, `[COLLISION FRAME]`, or triangle-test stage metrics were present in the 2026-10-05 Zombie Tower 4 log, so the reported 10-FPS cylinder encounter is not runtime-stage-proven by the available logs.

## Result

The leading explanation is a collision broadphase/narrowphase spike caused by Zombie Tower 4's unusually large and spatially broad collision geometry, likely amplified when the player's swept AABB overlaps a cylinder or nearby large surfaces. This is more plausible than the cylinder's raw render polygon count. It is not yet confirmed that the cylinder itself is the exact trigger, and rendering/GPU cost has not been ruled out for the user's specific encounter.

No build or runtime acceptance was performed because the request was investigation-only.

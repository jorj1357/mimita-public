2026 10 11 1713 jorj todo explain

the colisions 

ok 
it snto good right n ow bc  cylinders  
and

like in zombietower all the maps
near cylinders that stretch alogn  long place its like
big big  fps drops

as well as
the reason i  even make this  is bc i  am doing the  zombie models importing 

ok well ignroe avatar rotaiton issues 
for now  right here we need to ficsu on how come when i ahve 

"C:\mimita-v9\assets\entity\player\monsters\boomer\mimita-boomer-v1.glb"
"C:\mimita-v9\assets\entity\player\monsters\boomer\mimita-boomer-v2.glb"
"C:\mimita-v9\assets\entity\player\monsters\boomer\mimita-boomer-v3.glb"
"C:\mimita-v9\assets\entity\player\monsters\boomer\mimita-boomer-v4.glb"
"C:\mimita-v9\assets\entity\player\monsters\boomer\mimita-boomer-v5.glb"
"C:\mimita-v9\assets\entity\player\monsters\boomer\mimita-boomer-v6.glb"
"C:\mimita-v9\assets\entity\player\monsters\boomer\mimita-boomer-v7.glb"

likek any of thes is m avtar, like low lof ps big fps drops
and thenn
on the map "Atdm"  i also have big big fps drops
near curved objects like psheres and cylinders etc
when ur around like normal nothingness objects its fine  no fps drops

but when u ahve curved objects then it ahs big big fps drops

need to investigate using events.jsonl  
C:\mimita-v9\docs\specs\debug-logging\AI-fix-live.md  according to this  use this 

2026 10 11 1829

Other games solve this while keeping the real triangles by using better acceleration:
- Build a tree around the real triangles, such as a BVH.
- First check big groups of triangles.
- Ignore whole groups that are nowhere near the player.
- Only test the few nearby real triangles exactly.
- Keep the original Blender triangle vertices and indices.
- Store the object scale separately instead of creating bad stretched collision data.
- Use special fast math for huge flat triangles.
- Cache the nearby triangle list while the player barely moves.
- Avoid repeating the same collision query six times when nothing meaningfully changed.
- Process triangle tests in batches instead of slow one-at-a-time work.
test this  this seems matbe good
without  makign it so  mesh is rendered and not triangles vs triangels
i love that the traingels are what u hit 

2026-10-11 investigation and proposed fix
============================================

Decision this document is trying to support
--------------------------------------------

The visible GLB must remain the source of truth. The proposed solution must not
replace a Blender mesh with a box, capsule, sphere, low-poly collision proxy, or
different visual mesh. A collision hit must still resolve against the original
collision triangles that came from the GLB. The optimization is allowed to add
indexes, cached bounds, node metadata, and pair caches around those triangles.
It is not allowed to change the triangle vertices or triangle identity merely to
make collision cheaper.

There is one important current-code qualification: the loader currently has an
optional collision LOD. `src/map/map-loader-collision.cpp:25-69` clusters render
vertices when `config/collision-lod.json` is enabled, and
`src/map/map-loader-collision.cpp:79-120` emits collision triangles from the
cluster representatives. Therefore “the exact visible triangles” is only true
when that LOD is disabled, or after the loader is changed so the exact-triangle
mode keeps the original render vertices and triangle indices. The new design
below assumes exact-triangle mode is selected and proves that choice in the
runtime journal.

What the live evidence proves
------------------------------

The AtDM measurements are not just a guess based on how the mesh looks:

* The map contains about 50,859 collision triangles and about 45,148 chunks.
  The render side reports about 56,500 triangles.
* Fly mode is a control case. Jason and Boomer in fly mode stayed around the
  normal 245-390 FPS range, with roughly 2-3 ms frames and no collision events.
  This rules out “the GLB is always too expensive to render” as the primary
  explanation.
* Normal Boomer at the AtDM problem location produced roughly 200,000-400,000
  candidate triangle pairs per solve, with solve times around 100-249 ms in
  individual calls. The sustained performance was about 0.2 FPS, and the
  physics side occupied seconds per frame while rendering was below 1 ms.
* The exact teleport Boomer run measured 3,680 actor triangles, about 127 world
  candidates, and about 126 candidates classified as large. The first solve
  tested about 400,000 pairs and took about 234 ms. Later solves still tested
  about 200,000 pairs.
* Jason is much smaller: the early run had 72 actor triangles, about 64 world
  candidates, and about 4,608 triangle tests in a solve. It was still slow in
  the hotspot because the same work is called repeatedly during a fixed tick.
* Turning the actor solver off did not remove the severe Boomer slowdown. This
  means the actor triangle solver is a very strong measured hotspot, but it is
  not safe to claim it is the only source of all slowdown until a complete
  PhysicsMainUpdate profile is captured for every path.
* Reducing collision substeps from the normal six to one improved the result
  but still left the Boomer case very slow. Therefore “it only happens because
  subSteps is six” is false. Six calls multiply the cost; they do not explain
  the original pair explosion by themselves.
* The frame-scope logs show rendering and aim/body work are small compared with
  PhysicsMainUpdate. This rules out ordinary avatar drawing, animation, and
  GPU rendering as the main cause of the AtDM freeze.

The simplest description is: a large, detailed actor is being compared against
many nearby world triangles, and the expensive exact checks are repeated across
actor triangles, correction iterations, movement substeps, and sometimes sweep
samples. Curved or long geometry makes the world candidate bounds overlap a
large region, so the bad cost is exposed there. The curve is not magically
expensive; it is the shape that makes the current broadphase return too many
possible pairs.

Current code path and exact inefficiencies
------------------------------------------

### 1. The map already has an AABB tree, but it is only a one-sided filter

`src/physics/movement/collision-aabb-tree.h:2-9` describes the current tree as
a flat binary AABB tree for finding primitives whose axis-aligned bounds
overlap. `:47-97` builds and queries it, and `:114-158` splits nodes by median
centroid spread. This is useful, but it is not a triangle-versus-triangle
acceleration structure:

* the tree stores triangle AABBs, not tight triangle geometry;
* it is a binary median tree, not a surface-area-optimized or wide tree;
* it is queried once per actor triangle later, rather than traversing the actor
  mesh tree against the world tree as a pair;
* a long triangle has a large AABB, so its node remains a candidate across a
  large volume even when the actual triangle occupies only a thin surface.

The world tree is built over every triangle in
`src/map/map-loader-collision.cpp:244-255`. The helper
`src/physics/movement/physics-collision.cpp:82-106` correctly uses that tree to
avoid scanning the entire map. That is better than the old always-large scan,
but it still returns a candidate list that can be large at the curved-object
hotspot.

### 2. Large triangles are still expensive because their AABBs are large

`src/map/map-loader-collision.cpp:172-204` classifies triangles that touch more
than `kMaxChunksPerTriangle` chunks as large and removes them from the normal
chunk references. `:210-242` adds a coarse large-triangle grid, while
`:244-255` puts every triangle into the persistent tree.

This prevents a giant triangle from being copied into thousands of fine-grid
cells, but it does not make the triangle itself cheap. Its AABB still covers a
large volume. A query near any part of that volume can return it, and the exact
narrowphase must still prove whether the real triangle intersects the actor.
This is why “just use the chunk list” is not enough and why deleting the large
triangle would violate the exact-GLB requirement.

### 3. The actor is tested triangle-by-triangle against the same world region

`src/physics/movement/actor-triangle-solver.cpp:240-271` collects the actor mesh,
counts its triangles, builds one swept actor AABB, and obtains one world
candidate list. For the measured Boomer case that was 3,680 actor triangles and
about 127 world candidates.

`src/physics/movement/actor-triangle-solver.cpp:309-321` then builds a temporary
world tree over those candidates. This tree is reused between correction
iterations, which is a good improvement, but the actor side has no matching
triangle BVH. The expensive work is therefore still approximately:

    actorTriangles * nearbyWorldCandidates

before exact rejection. In the measured case:

    3,680 * 127 = 467,360 possible pairs

The recorded 200,000-400,000 tests are consistent with that upper bound after
contact and test budgets stop some work.

### 4. Each actor triangle recomputes transformed geometry and queries the tree

`src/physics/movement/physics-collision-mesh.cpp:483-510` transforms three
desired vertices and three previous vertices for every actor triangle, computes
centroids, travel, sweep steps, and a swept AABB. The sweep step count is
`ceil(maxVertexTravel / 0.08)` clamped to 64.

`src/physics/movement/physics-collision-mesh.cpp:512-542` queries the temporary
world tree for every actor triangle, then still performs candidate AABB checks.
That is a useful correctness filter, but it repeats traversal overhead for
thousands of triangles whose parent actor region could have been rejected once.

### 5. Exact tests can be repeated many times

`src/physics/movement/physics-collision-mesh.cpp:549-585` performs exact
triangle intersection and swept interpolation. `:587-602` can then run rounded
feature distance math for a pair that did not hit the flat triangle test.
`src/physics/movement/physics-collision-mesh.cpp:726-747` merges contacts by a
linear search through the accumulated contacts.

The whole solve repeats correction work in
`src/physics/movement/actor-triangle-solver.cpp:334-345`, and
`src/physics/physics-mini.cpp:251-286` calls `doCollisions` once per movement
substep. At normal settings that is up to six collision calls per fixed tick.
The exact code is correct in spirit, but it is organized around many small
actor-triangle queries instead of a small number of coherent BVH pair queries.

What the better design should be
---------------------------------

Use two persistent acceleration structures over the original triangles:

1. A static world BVH containing every AtDM collision triangle and its original
   triangle ID.
2. One BVH per avatar collision mesh containing every original GLB triangle and
   its original local triangle ID.

The BVHs are indexes only. The leaf still points to the original triangle. No
triangle is deleted, simplified, replaced by a box, or changed for collision.
The world tree can be rebuilt when a map loads. An avatar tree can be built when
the GLB is loaded and reused while the avatar is animated. Per-frame transforms
update node bounds or use a swept transform, but do not rebuild the topology.

The preferred tree is a tight, cache-friendly BVH with a small fixed number of
children per node (for example 4 or 8) and a surface-area-based split. The exact
choice can be benchmarked. The important change is not the brand name “BVH”; it
is traversing actor nodes against world nodes, so a whole group of actor
triangles is rejected against a whole group of world triangles in one bounds
test.

Proposed pseudocode
-------------------

This is intentionally a design sketch. Real MiMITA names, ownership, threading,
and math helpers may differ during implementation.

    struct ExactTriangleRef {
        uint32_t originalTriangleId;
        Vec3 a, b, c;                 // unchanged GLB/world triangle
        Vec3 normal;
        Aabb localOrWorldBounds;
        uint32_t materialOrPartId;
    };

    struct BvhNode {
        Aabb bounds;
        uint32_t childOrLeafStart;
        uint16_t childCountOrLeafCount;
        bool isLeaf;
    };

    class TriangleBvh {
        vector<BvhNode> nodes;
        vector<uint32_t> triangleRefs;

        void build(span<const ExactTriangleRef> triangles) {
            // Reorder references, never triangle data. Store original IDs.
            // Use SAH or a measured wide-BVH builder, then store nodes in a
            // contiguous array for predictable cache access.
        }
    };

    void collideExactMeshes(ActorPose pose, WorldCollision world) {
        PairCache pairs;

        // Transform the actor root once and produce swept bounds for its BVH.
        // The leaf IDs still refer to the original GLB triangles.
        BvhBounds actorBounds = actorBvh.computeSweptBounds(pose.previous,
                                                             pose.desired,
                                                             movementMargin);

        traversePairs(actorBvh.root(), world.bvh.root(), actorBounds,
            [&](ActorNode an, WorldNode wn) {
                if (!tightSweptAabbOverlap(an.bounds, wn.bounds))
                    return;
                if (!pairs.insertUnique(an.id, wn.id))
                    return;
                descendOrEmit(an, wn);
            });

        for (Pair pair : pairs) {
            // Cheap rejection using cached bounds, planes, and edge data.
            if (!cheapSweptTriangleReject(pair.actor, pair.world))
                continue;

            // Only now do the exact original-triangle operation.
            ExactHit hit = exactTriangleSweep(pair.actor.originalTriangle,
                                              pair.world.originalTriangle);
            if (!hit.didHit && roundedFeatures &&
                cheapRoundedBoundsMayHit(pair))
                hit = exactRoundedFeatureTest(pair);

            if (hit.didHit)
                manifold.addOrMerge(pair.originalIds, hit);
        }

        solveManifoldWithoutRequerying(manifold);
    }

The fixed-tick loop should reuse the same pair/manifold information when the
actor swept bounds and world revision have not changed materially. A correction
iteration should update the actor pose and invalidate only pairs whose node
bounds changed, not throw away and rediscover the entire actor/world pair set.
The six-substep loop should likewise share immutable world nodes, cached triangle
data, and a bounded contact cache. Correctness still comes from the exact final
triangle test, not from trusting the broadphase.

Specific implementation changes that are likely to matter
----------------------------------------------------------

* Add cached triangle bounds, normal, edge vectors, and any safe precomputed
  plane terms to the world and avatar triangle arrays. The map loader already
  caches world AABBs at `src/map/map-loader-collision.cpp:162-177`; extend the
  same idea to actor triangles instead of reconstructing data in the hot loop.
* Build the actor BVH once per loaded GLB or collision mesh. Do not build an
  actor BVH every fixed tick.
* Replace the per-actor-triangle call to
  `worldTree->query()` at `src/physics/movement/physics-collision-mesh.cpp:515-522`
  with a BVH pair traversal. The actor node query must produce the same original
  world triangle IDs as the current code.
* Keep a per-solve pair cache keyed by original actor triangle ID and original
  world triangle ID. This prevents the same pair being tested once for every
  correction iteration or substep when its swept bounds are unchanged.
* Move rounded-feature work behind the cheapest safe rejection. The existing
  order already delays it until after the flat triangle test at
  `:587-602`; the new pair traversal should reduce the number of pairs reaching
  that point.
* Replace the linear contact merge at `:726-747` with a bounded open-addressed
  table or sorted compact key list. This is secondary to the pair explosion but
  removes another repeated hot-loop cost.
* Keep all hot-loop storage in reserved thread-local or solver-owned buffers.
  No per-query heap allocation, no temporary vector construction per triangle,
  and no rebuilding of topology in `doCollisions`.
* Keep the current world AABB tree as a safe fallback and comparison path until
  the exact-triangle BVH has passed matched runtime tests. This is important
  because a successful build does not prove equivalent collision behavior.

Why this can reach a 4 ms target, and what is not guaranteed
-------------------------------------------------------------

At 240 FPS the total frame budget is about 4.17 ms. Collision cannot consume all
of that budget; the practical target should be approximately:

    world/actor BVH traversal and pair creation       <= 0.5 ms
    exact triangle narrowphase                         <= 1.5 ms
    rounded features and contact manifold              <= 0.75 ms
    correction/substep bookkeeping                     <= 0.75 ms
    total collision target                             <= 3.5 ms

That leaves a small amount for the rest of physics and gives the whole frame a
chance to remain under 4 ms on the target machine. These are acceptance targets,
not promises. No collision algorithm can promise <=4 ms if the actor genuinely
intersects an enormous number of unique triangles in one tick. The design makes
the normal case cheap by proving non-intersection at node level, then reserves
expensive exact math for the small set of pairs that could actually touch.

The current measurements show enough avoidable work to make this a credible
target: the Boomer case is spending roughly 100-249 ms in individual solves and
testing 200,000-400,000 pairs, while fly mode shows the rest of the frame can
run near 2-3 ms when collision is absent. The required proof is a live A/B run:

    same AtDM map
    same teleport position
    same avatar and GLB
    same fixed tick and movement settings
    old broadphase versus exact-triangle BVH

The runtime journal must record, per fixed tick, frame time, PhysicsMainUpdate
time, collision time, actor triangle count, world candidate/pair count, exact
triangle tests, rounded-feature calls, contacts, substeps, BVH node tests, BVH
leaf pairs, and the executable build/hash. The fix is successful only if the
actual Boomer hotspot reaches the target without changing visible triangles and
without changing hit results. If the pair count remains high, improve the BVH
bounds/split or temporal cache before claiming the narrowphase is fixed.

What this proposal deliberately does not claim
-----------------------------------------------

* It does not claim the current median AABB tree is useless. It already removes
  full-map scans and should remain as a fallback.
* It does not claim every curved object is bad, or that triangle size alone is
  the cause. The evidence shows large candidate bounds and repeated exact pair
  work at the hotspot.
* It does not claim rendering is the cause. Fly-mode and frame-scope evidence
  point primarily at fixed-tick physics/collision.
* It does not claim substeps are irrelevant. Six substeps multiply the work,
  but one-substep evidence shows they are not the whole cause.
* It does not claim adding the word “BVH” automatically fixes the game. The
  meaningful change is actor-BVH versus world-BVH pair traversal, cached exact
  triangle data, pair reuse, and live proof against the same AtDM location.
* It does not promise a universal 4 ms result before implementation and live
  measurement. The 4 ms number is a hard performance acceptance target for the
  Boomer/AtDM reproduction, not a source-code assumption.

Library comparison
------------------

The repository can implement this internally because it already owns the world
AABB tree and exact triangle narrowphase. External references are useful for
design comparison, but they do not remove the need to preserve MiMITA's exact
triangle IDs and movement response. Bullet's `btBvhTriangleMeshShape`, Jolt's
collision architecture, and PhysX triangle mesh geometry all use acceleration
around triangle meshes. Embree is highly optimized for ray tracing, but it is
not automatically a drop-in character collision solver. The safest first step
is therefore a MiMITA-owned exact-triangle BVH that keeps the current narrowphase
as the final authority, with a library considered only after a benchmark proves
it preserves contacts and does not add a new ownership boundary.

Implementation boundary
-----------------------

This is a proposed architecture and evidence-backed direction, not a claim that
the implementation is already complete. The real code may use different class
names, node layouts, SIMD helpers, or a different BVH branching factor. The
non-negotiable contracts are: original GLB triangles remain the collision
geometry; broadphase indexes never change the answer; fixed-tick collision stays
bounded and allocation-free; and the live AtDM Boomer run proves the timing and
contact result before the old path is removed.

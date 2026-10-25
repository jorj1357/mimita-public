#include "map-loader-collision.h"
#include "map/map-static-union.h"
#include "world/world.h"
#include "world/world-gltf-loader.h"
#include "physics/physics-types.h"
#include "config/collision-lod-config.h"
#include "debug/structured-log.h"
#include <cstdio>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>

namespace {

glm::ivec3 collisionChunkCoord(const glm::vec3& p, float size)
{
    return glm::ivec3(
        (int)std::floor(p.x / size),
        (int)std::floor(p.y / size),
        (int)std::floor(p.z / size)
    );
}

} // anonymous namespace

void buildCollisionMeshFromRenderMesh(World& world)
{
    world.collisionMesh.clear();

    if (world.mesh.verts.size() < 3)
        return;

    // Collision decimation: cluster render vertices to a grid so dense objects
    // keep full render fidelity but collide cheaply. Large features (walls,
    // floors, big shapes) keep their triangles; tiny redundant triangles (sphere
    // segments, text glyphs, house trim) collapse away. Tuned live via
    // config/collision-lod.json cellSize.
    const CollisionLodConfig& lod = CollisionLodConfig::instance();
    const float cellSize = lod.enabled() ? lod.cellSize() : 0.0f;

    std::vector<uint32_t> vertRep;
    vertRep.resize(world.mesh.verts.size(), 0);
    if (cellSize > 0.001f)
    {
        std::unordered_map<glm::ivec3, uint32_t, IVec3Hash> clusterRep;
        clusterRep.reserve(world.mesh.verts.size() / 2);
        for (size_t vi = 0; vi < world.mesh.verts.size(); ++vi)
        {
            const glm::vec3& p = world.mesh.verts[vi].pos;
            glm::ivec3 key(
                (int)std::floor(p.x / cellSize + 0.5f),
                (int)std::floor(p.y / cellSize + 0.5f),
                (int)std::floor(p.z / cellSize + 0.5f));
            auto it = clusterRep.find(key);
            if (it == clusterRep.end())
            {
                clusterRep.emplace(key, (uint32_t)vi);
                vertRep[vi] = (uint32_t)vi;
            }
            else
            {
                vertRep[vi] = it->second;
            }
        }
    }
    else
    {
        for (size_t vi = 0; vi < world.mesh.verts.size(); ++vi)
            vertRep[vi] = (uint32_t)vi;
    }

    bool boundsSet = false;
    int skippedDegenerate = 0;
    int skippedNonFinite = 0;
    int skippedExtreme = 0;
    int skippedCollapsed = 0;
    constexpr float MAX_WORLD_EXTENT = kMaxWorldExtent;
    constexpr float MIN_TRI_AREA = 0.000001f;

    std::vector<uint32_t> vertexSolidIds(world.mesh.verts.size(), 0);
    uint32_t nextSolidId = 1;
    const auto assignObjectRange = [&](const MapGLBObject& object) {
        const size_t first = std::min(object.vertexFirst, world.mesh.verts.size());
        const size_t last = std::min(object.vertexFirst + object.vertexCount,
                                      world.mesh.verts.size());
        if (first >= last || (last - first) % 3 != 0)
            return;
        std::vector<CollisionTriangle> objectTriangles;
        objectTriangles.reserve((last - first) / 3);
        for (size_t ti = first; ti + 2 < last; ti += 3)
        {
            CollisionTriangle triangle;
            triangle.a = world.mesh.verts[ti + 0].pos;
            triangle.b = world.mesh.verts[ti + 1].pos;
            triangle.c = world.mesh.verts[ti + 2].pos;
            objectTriangles.push_back(triangle);
        }
        if (!isClosedSolidTriangleRange(objectTriangles, 0, objectTriangles.size()))
            return;
        const uint32_t solidId = nextSolidId++;
        for (size_t vi = first; vi < last; ++vi)
            vertexSolidIds[vi] = solidId;
    };
    for (const MapGLBObject& object : world.mesh.objects)
        assignObjectRange(object);
    for (const MapGLBObject& object : world.authoredObjects)
        assignObjectRange(object);

    for (size_t i = 0; i + 2 < world.mesh.verts.size(); i += 3)
    {
        const uint32_t ra = vertRep[i + 0];
        const uint32_t rb = vertRep[i + 1];
        const uint32_t rc = vertRep[i + 2];
        if (ra == rb || rb == rc || ra == rc)
        {
            ++skippedCollapsed;
            continue;
        }
        CollisionTriangle tri;
        tri.a = world.mesh.verts[ra].pos;
        tri.b = world.mesh.verts[rb].pos;
        tri.c = world.mesh.verts[rc].pos;

        if (!std::isfinite(tri.a.x) || !std::isfinite(tri.a.y) || !std::isfinite(tri.a.z) ||
            !std::isfinite(tri.b.x) || !std::isfinite(tri.b.y) || !std::isfinite(tri.b.z) ||
            !std::isfinite(tri.c.x) || !std::isfinite(tri.c.y) || !std::isfinite(tri.c.z))
        {
            ++skippedNonFinite;
            continue;
        }

        glm::vec3 triMin = glm::min(glm::min(tri.a, tri.b), tri.c);
        glm::vec3 triMax = glm::max(glm::max(tri.a, tri.b), tri.c);
        if (glm::any(glm::lessThan(triMin, glm::vec3(-MAX_WORLD_EXTENT))) ||
            glm::any(glm::greaterThan(triMax, glm::vec3(MAX_WORLD_EXTENT))))
        {
            ++skippedExtreme;
            continue;
        }

        glm::vec3 n = glm::cross(tri.b - tri.a, tri.c - tri.a);
        float nLen = glm::length(n);
        if (nLen < MIN_TRI_AREA)
        {
            ++skippedDegenerate;
            continue;
        }
        tri.normal = n / nLen;

        uint32_t solidId = 0;
        if (vertexSolidIds[i] != 0 &&
            vertexSolidIds[i] == vertexSolidIds[i + 1] &&
            vertexSolidIds[i] == vertexSolidIds[i + 2])
            solidId = vertexSolidIds[i];

        world.collisionMesh.triangles.push_back(tri);
        world.collisionMesh.solidIds.push_back(solidId);

        if (!boundsSet)
        {
            world.collisionMesh.boundsMin = triMin;
            world.collisionMesh.boundsMax = triMax;
            boundsSet = true;
        }
        else
        {
            world.collisionMesh.boundsMin = glm::min(world.collisionMesh.boundsMin, triMin);
            world.collisionMesh.boundsMax = glm::max(world.collisionMesh.boundsMax, triMax);
        }
    }

    printf(
        "[WORLD GLB COLLISION] triangles=%zu bounds min=(%.2f %.2f %.2f) max=(%.2f %.2f %.2f)"
        " skipped: degenerate=%d nonFinite=%d extreme=%d collapsed=%d cellSize=%.2f\n",
        world.collisionMesh.triangles.size(),
        world.collisionMesh.boundsMin.x,
        world.collisionMesh.boundsMin.y,
        world.collisionMesh.boundsMin.z,
        world.collisionMesh.boundsMax.x,
        world.collisionMesh.boundsMax.y,
        world.collisionMesh.boundsMax.z,
        skippedDegenerate, skippedNonFinite, skippedExtreme, skippedCollapsed, cellSize
    );

    glm::vec3 size = world.collisionMesh.boundsMax - world.collisionMesh.boundsMin;
    if (glm::any(glm::greaterThan(size, glm::vec3(MAX_WORLD_EXTENT * 0.5f))))
    {
        printf("[WORLD GLB WARNING] Suspiciously large collision bounds size=(%.1f %.1f %.1f) — "
               "may contain extreme triangles\n",
               size.x, size.y, size.z);
    }
}

namespace {

void rebuildCollisionTriangleCaches(World& world)
{
    const size_t count = world.collisionMesh.triangles.size();
    world.collisionMesh.triangleAABBs.resize(count);
    world.collisionMesh.triangleOBBs.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const CollisionTriangle& tri = world.collisionMesh.triangles[i];
        const glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c);
        const glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c);
        world.collisionMesh.triangleAABBs[i] = AABB{mn, mx};
        CollisionTriangleOBB obb;
        obb.origin = tri.a;
        const glm::vec3 uRaw = tri.b - tri.a;
        const glm::vec3 nRaw = glm::cross(tri.b - tri.a, tri.c - tri.a);
        const float uLen = glm::length(uRaw);
        const float nLen = glm::length(nRaw);
        if (uLen > 1e-6f && nLen > 1e-6f) {
            obb.axisU = uRaw / uLen;
            obb.axisN = nRaw / nLen;
            obb.axisV = glm::normalize(glm::cross(obb.axisN, obb.axisU));
            obb.localMin = glm::vec3(std::numeric_limits<float>::max());
            obb.localMax = glm::vec3(std::numeric_limits<float>::lowest());
            for (const glm::vec3& p : {tri.a, tri.b, tri.c}) {
                const glm::vec3 d = p - obb.origin;
                const glm::vec3 q(glm::dot(d, obb.axisU),
                                  glm::dot(d, obb.axisV),
                                  glm::dot(d, obb.axisN));
                obb.localMin = glm::min(obb.localMin, q);
                obb.localMax = glm::max(obb.localMax, q);
            }
            obb.valid = true;
        }
        world.collisionMesh.triangleOBBs[i] = obb;
    }
    if (count == 0) {
        world.collisionMesh.boundsMin = glm::vec3(0.0f);
        world.collisionMesh.boundsMax = glm::vec3(0.0f);
        return;
    }
    world.collisionMesh.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    world.collisionMesh.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    for (const AABB& bounds : world.collisionMesh.triangleAABBs) {
        world.collisionMesh.boundsMin = glm::min(world.collisionMesh.boundsMin, bounds.min);
        world.collisionMesh.boundsMax = glm::max(world.collisionMesh.boundsMax, bounds.max);
    }
}

bool boxOverlaps(const AABB& a, const AABB& b)
{
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

} // namespace

bool updateCollisionForMovedObject(World& world, const AABB& oldBounds,
                                   const AABB& newBounds)
{
    const auto started = std::chrono::steady_clock::now();
    const size_t previousTriangles = world.collisionMesh.triangles.size();
    buildCollisionMeshFromRenderMesh(world);
    if (world.collisionMesh.triangles.empty() ||
        world.collisionMesh.triangles.size() != previousTriangles) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Performance, StructuredLevel::Important,
            "map-editor.collision-refresh", "map-editor",
            "collision refresh deferred after object move", 0,
            {{"incremental", false}, {"deferred", true},
             {"triangles", world.collisionMesh.triangles.size()},
             {"duration_ms", std::chrono::duration<double, std::milli>(
                 std::chrono::steady_clock::now() - started).count()}});
        return false;
    }
    rebuildCollisionTriangleCaches(world);

    std::unordered_set<glm::ivec3, IVec3Hash> affected;
    auto addBoundsChunks = [&](const AABB& bounds) {
        const glm::ivec3 c0 = collisionChunkCoord(bounds.min, world.collisionChunkSize);
        const glm::ivec3 c1 = collisionChunkCoord(bounds.max, world.collisionChunkSize);
        for (int x = c0.x; x <= c1.x; ++x)
        for (int y = c0.y; y <= c1.y; ++y)
        for (int z = c0.z; z <= c1.z; ++z)
            affected.insert(glm::ivec3(x, y, z));
    };
    auto boundsChunkCount = [&](const AABB& bounds) {
        const glm::ivec3 c0 = collisionChunkCoord(bounds.min, world.collisionChunkSize);
        const glm::ivec3 c1 = collisionChunkCoord(bounds.max, world.collisionChunkSize);
        return (int64_t)(c1.x - c0.x + 1) * (c1.y - c0.y + 1) * (c1.z - c0.z + 1);
    };
    constexpr int64_t kMaxIncrementalCandidateChunks = 4096;
    if (boundsChunkCount(oldBounds) > kMaxIncrementalCandidateChunks ||
        boundsChunkCount(newBounds) > kMaxIncrementalCandidateChunks) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Performance, StructuredLevel::Important,
            "map-editor.collision-refresh", "map-editor",
            "collision refresh deferred for map-spanning object", 0,
            {{"incremental", false}, {"deferred", true},
             {"duration_ms", std::chrono::duration<double, std::milli>(
                 std::chrono::steady_clock::now() - started).count()}});
        return false;
    }

    addBoundsChunks(oldBounds);
    addBoundsChunks(newBounds);

    // A large or map-spanning node can touch thousands of chunks. The set
    // maintenance would then cost more than one full chunk rebuild, so bail to
    // the bounded full path.
    constexpr size_t kMaxIncrementalChunks = 512;
    if (affected.size() > kMaxIncrementalChunks) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Performance, StructuredLevel::Important,
            "map-editor.collision-refresh", "map-editor",
            "collision refresh deferred for large affected chunk set", 0,
            {{"incremental", false}, {"deferred", true},
             {"affected_chunks", affected.size()},
             {"duration_ms", std::chrono::duration<double, std::milli>(
                 std::chrono::steady_clock::now() - started).count()}});
        return false;
    }

    if (!world.collisionLargeTriangles.empty()) {
        for (int index : world.collisionLargeTriangles) {
            if (index < 0 || index >= (int)world.collisionMesh.triangleAABBs.size())
                continue;
            const AABB& bounds = world.collisionMesh.triangleAABBs[(size_t)index];
            if (boxOverlaps(bounds, oldBounds) || boxOverlaps(bounds, newBounds)) {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Performance, StructuredLevel::Important,
                    "map-editor.collision-refresh", "map-editor",
                    "collision refresh deferred for large triangle", 0,
                    {{"incremental", false}, {"deferred", true},
                     {"duration_ms", std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - started).count()}});
                return false;
            }
        }
    }

    for (const glm::ivec3& coord : affected)
        world.collisionChunks.erase(coord);

    for (size_t i = 0; i < world.collisionMesh.triangles.size(); ++i) {
        const AABB& bounds = world.collisionMesh.triangleAABBs[i];
        const glm::ivec3 c0 = collisionChunkCoord(bounds.min, world.collisionChunkSize);
        const glm::ivec3 c1 = collisionChunkCoord(bounds.max, world.collisionChunkSize);
        const int64_t touched = (int64_t)(c1.x - c0.x + 1) *
            (c1.y - c0.y + 1) * (c1.z - c0.z + 1);
        if (touched > kMaxChunksPerTriangle)
            continue;
        for (int x = c0.x; x <= c1.x; ++x)
        for (int y = c0.y; y <= c1.y; ++y)
        for (int z = c0.z; z <= c1.z; ++z) {
            const glm::ivec3 coord(x, y, z);
            if (affected.count(coord) != 0)
                world.collisionChunks[coord].push_back((int)i);
        }
    }

    world.collisionTree.clear();
    std::vector<int> allTriangleIds(world.collisionMesh.triangles.size());
    for (size_t i = 0; i < allTriangleIds.size(); ++i)
        allTriangleIds[i] = (int)i;
    world.collisionTree.build(allTriangleIds, world.collisionMesh.triangleAABBs);
    // Sub-grids are an optional second-level optimization; invalidate them
    // rather than rebuilding every chunk on a single-object edit.
    world.collisionSubGrids.clear();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Performance, StructuredLevel::Important,
        "map-editor.collision-refresh", "map-editor",
        "incremental collision refresh after object move", 0,
        {{"incremental", true},
         {"affected_chunks", affected.size()},
         {"triangles", world.collisionMesh.triangles.size()},
         {"duration_ms", std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - started).count()}});
    return true;
}
void buildCollisionChunks(World& world, MapLoadMetrics* metrics)
{
    world.collisionChunks.clear();
    world.collisionLargeTriangles.clear();

    // Cache per-triangle bounds once. The actor narrowphase and broadphase
    // overlap filters read these instead of recomputing makeTriangleAABB for
    // every candidate/actor-triangle pair.
    world.collisionMesh.triangleAABBs.resize(world.collisionMesh.triangles.size());
    world.collisionMesh.triangleOBBs.resize(world.collisionMesh.triangles.size());

    uint64_t totalRefs = 0;
    uint64_t maxChunks = 0;
    double maxBounds = 0;
    int largeTriCount = 0;

    for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i)
    {
        const CollisionTriangle& tri = world.collisionMesh.triangles[i];
        glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c);
        glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c);
        world.collisionMesh.triangleAABBs[i] = AABB{mn, mx};
        CollisionTriangleOBB obb;
        obb.origin = tri.a;
        const glm::vec3 uRaw = tri.b - tri.a;
        const glm::vec3 nRaw = glm::cross(tri.b - tri.a, tri.c - tri.a);
        const float uLen = glm::length(uRaw);
        const float nLen = glm::length(nRaw);
        if (uLen > 1e-6f && nLen > 1e-6f)
        {
            obb.axisU = uRaw / uLen;
            obb.axisN = nRaw / nLen;
            obb.axisV = glm::normalize(glm::cross(obb.axisN, obb.axisU));
            obb.localMin = glm::vec3(std::numeric_limits<float>::max());
            obb.localMax = glm::vec3(std::numeric_limits<float>::lowest());
            for (const glm::vec3& p : {tri.a, tri.b, tri.c})
            {
                const glm::vec3 d = p - obb.origin;
                const glm::vec3 q(glm::dot(d, obb.axisU),
                                  glm::dot(d, obb.axisV),
                                  glm::dot(d, obb.axisN));
                obb.localMin = glm::min(obb.localMin, q);
                obb.localMax = glm::max(obb.localMax, q);
            }
            obb.valid = true;
        }
        world.collisionMesh.triangleOBBs[i] = obb;
        glm::ivec3 c0 = collisionChunkCoord(mn, world.collisionChunkSize);
        glm::ivec3 c1 = collisionChunkCoord(mx, world.collisionChunkSize);

        int chunksX = c1.x - c0.x + 1;
        int chunksY = c1.y - c0.y + 1;
        int chunksZ = c1.z - c0.z + 1;
        int64_t chunksTouched = (int64_t)chunksX * chunksY * chunksZ;

        glm::vec3 triSize = mx - mn;
        double maxDim = std::max({(double)triSize.x, (double)triSize.y, (double)triSize.z});
        if (maxDim > maxBounds) maxBounds = maxDim;

        if (chunksTouched > kMaxChunksPerTriangle)
        {
            world.collisionLargeTriangles.push_back(i);
            ++largeTriCount;
            continue;
        }

        totalRefs += chunksTouched;
        if ((uint64_t)chunksTouched > maxChunks) maxChunks = chunksTouched;

        for (int x = c0.x; x <= c1.x; ++x)
        for (int y = c0.y; y <= c1.y; ++y)
        for (int z = c0.z; z <= c1.z; ++z)
            world.collisionChunks[glm::ivec3(x, y, z)].push_back(i);
    }

    if (largeTriCount > 0)
        printf("[WORLD GLB COLLISION WARNING] %d large triangles moved to collisionLargeTriangles (each exceeds %d chunks)\n",
               largeTriCount, kMaxChunksPerTriangle);

    // Build a coarse grid for large triangles so fallback broadphase queries only
    // test the ones near them instead of re-scanning every large triangle every
    // query. Triangles whose span exceeds the coarse cap are not indexed here;
    // they are covered by the persistent collisionTree, which is the primary
    // broadphase path.
    world.collisionLargeChunks.clear();
    if (!world.collisionLargeTriangles.empty())
    {
        const float coarseSize = world.collisionChunkSize * 4.0f;
        constexpr int MAX_COARSE_CHUNKS_PER_TRIANGLE = 1024;
        for (int triIndex : world.collisionLargeTriangles)
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c);
            glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c);
            glm::ivec3 c0((int)std::floor(mn.x / coarseSize),
                          (int)std::floor(mn.y / coarseSize),
                          (int)std::floor(mn.z / coarseSize));
            glm::ivec3 c1((int)std::floor(mx.x / coarseSize),
                          (int)std::floor(mx.y / coarseSize),
                          (int)std::floor(mx.z / coarseSize));
            int64_t coarseCells =
                (int64_t)(c1.x - c0.x + 1) * (c1.y - c0.y + 1) * (c1.z - c0.z + 1);
            if (coarseCells > MAX_COARSE_CHUNKS_PER_TRIANGLE)
                continue;
            for (int x = c0.x; x <= c1.x; ++x)
            for (int y = c0.y; y <= c1.y; ++y)
            for (int z = c0.z; z <= c1.z; ++z)
                world.collisionLargeChunks[glm::ivec3(x, y, z)].push_back(triIndex);
        }
        printf("[WORLD GLB COLLISION] large-tri grid: coarseSize=%.1f cells=%zu\n",
               coarseSize, world.collisionLargeChunks.size());
    }

    // Persistent static broadphase over EVERY collision triangle. This replaces
    // the old always-large scan: very large triangles live in the tree and are
    // only returned when their own AABB overlaps the query.
    world.collisionTree.clear();
    if (!world.collisionMesh.triangles.empty() &&
        world.collisionMesh.triangleAABBs.size() == world.collisionMesh.triangles.size())
    {
        std::vector<int> allTriangleIds(world.collisionMesh.triangles.size());
        for (size_t i = 0; i < allTriangleIds.size(); ++i)
            allTriangleIds[i] = (int)i;
        world.collisionTree.build(allTriangleIds, world.collisionMesh.triangleAABBs);
    }

    printf("[WORLD GLB COLLISION] chunks=%zu chunkSize=%.2f totalRefs=%llu maxChunksPerTri=%llu maxBounds=%.1f largeTris=%d treeNodes=%zu\n",
           world.collisionChunks.size(), world.collisionChunkSize,
           (unsigned long long)totalRefs, (unsigned long long)maxChunks, maxBounds, largeTriCount,
           world.collisionTree.nodes.size());

    buildCollisionSubGrids(world);

    if (metrics) {
        metrics->totalTriangleToChunkRefs = totalRefs;
        metrics->maxChunksTouchedByOneTriangle = maxChunks;
        metrics->maxTriangleBoundsSize = maxBounds;
    }
}

// Second-level sub-grid: divide each chunk into SUBDIV^3 sub-cells so broadphase
// queries near dense geometry (spheres, text, houses) only test the sub-cells
// their AABB touches instead of every triangle in the whole 6-unit chunk.
void buildCollisionSubGrids(World& world)
{
    world.collisionSubGrids.clear();
    constexpr int SUBDIV = 4;
    if (world.collisionChunkSize <= 0.001f || world.collisionChunks.empty())
        return;

    const float subSize = world.collisionChunkSize / (float)SUBDIV;
    uint64_t totalSubRefs = 0;
    for (const auto& kv : world.collisionChunks)
    {
        const glm::ivec3 chunkCoord = kv.first;
        const glm::vec3 chunkMin = glm::vec3(chunkCoord) * world.collisionChunkSize;
        CollisionSubGrid sub;
        sub.subSize = subSize;
        for (int triIndex : kv.second)
        {
            if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                continue;
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c);
            glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c);
            glm::ivec3 s0((int)std::floor((mn.x - chunkMin.x) / subSize),
                          (int)std::floor((mn.y - chunkMin.y) / subSize),
                          (int)std::floor((mn.z - chunkMin.z) / subSize));
            glm::ivec3 s1((int)std::floor((mx.x - chunkMin.x) / subSize),
                          (int)std::floor((mx.y - chunkMin.y) / subSize),
                          (int)std::floor((mx.z - chunkMin.z) / subSize));
            s0 = glm::clamp(s0, glm::ivec3(0), glm::ivec3(SUBDIV - 1));
            s1 = glm::clamp(s1, glm::ivec3(0), glm::ivec3(SUBDIV - 1));
            for (int x = s0.x; x <= s1.x; ++x)
            for (int y = s0.y; y <= s1.y; ++y)
            for (int z = s0.z; z <= s1.z; ++z)
            {
                sub.cells[glm::ivec3(x, y, z)].push_back(triIndex);
                ++totalSubRefs;
            }
        }
        world.collisionSubGrids[chunkCoord] = std::move(sub);
    }
    printf("[WORLD GLB COLLISION] subgrids=%zu subdiv=%d subSize=%.2f totalSubRefs=%llu\n",
           world.collisionSubGrids.size(), SUBDIV, subSize,
           (unsigned long long)totalSubRefs);
}

// Re-decimate the collision proxy from the render mesh with the current
// CollisionLodConfig settings and rebuild the broadphase. Used by live hot-reload
// so the cell size can change in-game without restarting.
void redecimateCollision(World& world)
{
    if (world.mesh.verts.empty())
        return;

    buildCollisionMeshFromRenderMesh(world);
    if (CollisionLodConfig::instance().staticSolidUnion())
        unionStaticSolidGroups(
            world.collisionMesh.triangles, world.collisionMesh.solidIds,
            CollisionLodConfig::instance().staticSolidUnionMaxTriangles(),
            nullptr,
            CollisionLodConfig::instance().staticSolidUnionMaxSolids());
    if (CollisionLodConfig::instance().staticFaceRemoval())
        removeFullyCoveredStaticTriangles(
            world.collisionMesh.triangles, world.collisionMesh.solidIds);
    buildCollisionChunks(world, nullptr);

    printf("[COLLISION LOD] re-decimated collision revision=%u cellSize=%.2f tris=%zu\n",
           CollisionLodConfig::instance().revision(),
           CollisionLodConfig::instance().cellSize(),
           world.collisionMesh.triangles.size());
}

// Decimate a triangle list by vertex clustering (in place). Mirrors the render-mesh
// decimation for server / NPC collision worlds that have no render mesh.
void decimateCollisionTriangleList(std::vector<CollisionTriangle>& tris, float cellSize,
                                   std::vector<uint32_t>* solidIds)
{
    if (cellSize <= 0.001f || tris.empty())
        return;

    const bool carrySolidIds =
        solidIds && solidIds->size() == tris.size();
    if (solidIds && !carrySolidIds)
        solidIds->clear();

    std::vector<glm::vec3> pool;
    pool.reserve(tris.size() * 3);
    std::unordered_map<glm::ivec3, uint32_t, IVec3Hash> clusterRep;
    clusterRep.reserve(tris.size() * 3 / 2);
    std::vector<uint32_t> triVertRep(tris.size() * 3, 0);

    for (size_t ti = 0; ti < tris.size(); ++ti)
    {
        const glm::vec3 verts[3] = {tris[ti].a, tris[ti].b, tris[ti].c};
        for (int k = 0; k < 3; ++k)
        {
            glm::ivec3 key(
                (int)std::floor(verts[k].x / cellSize + 0.5f),
                (int)std::floor(verts[k].y / cellSize + 0.5f),
                (int)std::floor(verts[k].z / cellSize + 0.5f));
            auto it = clusterRep.find(key);
            if (it == clusterRep.end())
            {
                const uint32_t idx = (uint32_t)pool.size();
                clusterRep.emplace(key, idx);
                pool.push_back(verts[k]);
                triVertRep[ti * 3 + k] = idx;
            }
            else
            {
                triVertRep[ti * 3 + k] = it->second;
            }
        }
    }

    std::vector<CollisionTriangle> out;
    out.reserve(tris.size());
    std::vector<uint32_t> outSolidIds;
    if (carrySolidIds)
        outSolidIds.reserve(tris.size());
    for (size_t ti = 0; ti < tris.size(); ++ti)
    {
        const uint32_t ra = triVertRep[ti * 3 + 0];
        const uint32_t rb = triVertRep[ti * 3 + 1];
        const uint32_t rc = triVertRep[ti * 3 + 2];
        if (ra == rb || rb == rc || ra == rc)
            continue;
        CollisionTriangle t;
        t.a = pool[ra];
        t.b = pool[rb];
        t.c = pool[rc];
        glm::vec3 n = glm::cross(t.b - t.a, t.c - t.a);
        float len = glm::length(n);
        if (len < 0.000001f)
            continue;
        t.normal = n / len;
        out.push_back(t);
        if (carrySolidIds)
            outSolidIds.push_back((*solidIds)[ti]);
    }
    tris = std::move(out);
    if (carrySolidIds)
        *solidIds = std::move(outSolidIds);
}

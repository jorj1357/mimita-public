#include "map-loader-collision.h"
#include "world/world.h"
#include "world/world-gltf-loader.h"
#include "config/collision-lod-config.h"
#include <cstdio>
#include <cmath>
#include <unordered_map>
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
    constexpr float MAX_WORLD_EXTENT = 5000.0f;
    constexpr float MIN_TRI_AREA = 0.000001f;

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

        world.collisionMesh.triangles.push_back(tri);

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

static constexpr int MAX_CHUNKS_PER_TRIANGLE = 256;

void buildCollisionChunks(World& world, MapLoadMetrics* metrics)
{
    world.collisionChunks.clear();
    world.collisionLargeTriangles.clear();

    uint64_t totalRefs = 0;
    uint64_t maxChunks = 0;
    double maxBounds = 0;
    int largeTriCount = 0;

    for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i)
    {
        const CollisionTriangle& tri = world.collisionMesh.triangles[i];
        glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c);
        glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c);
        glm::ivec3 c0 = collisionChunkCoord(mn, world.collisionChunkSize);
        glm::ivec3 c1 = collisionChunkCoord(mx, world.collisionChunkSize);

        int chunksX = c1.x - c0.x + 1;
        int chunksY = c1.y - c0.y + 1;
        int chunksZ = c1.z - c0.z + 1;
        int64_t chunksTouched = (int64_t)chunksX * chunksY * chunksZ;

        glm::vec3 triSize = mx - mn;
        double maxDim = std::max({(double)triSize.x, (double)triSize.y, (double)triSize.z});
        if (maxDim > maxBounds) maxBounds = maxDim;

        if (chunksTouched > MAX_CHUNKS_PER_TRIANGLE)
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
               largeTriCount, MAX_CHUNKS_PER_TRIANGLE);

    // Build a coarse grid for large triangles so broadphase queries only test the
    // ones near them instead of re-scanning every large triangle every query.
    world.collisionLargeChunks.clear();
    world.collisionAlwaysLargeTriangles.clear();
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
            {
                world.collisionAlwaysLargeTriangles.push_back(triIndex);
                continue;
            }
            for (int x = c0.x; x <= c1.x; ++x)
            for (int y = c0.y; y <= c1.y; ++y)
            for (int z = c0.z; z <= c1.z; ++z)
                world.collisionLargeChunks[glm::ivec3(x, y, z)].push_back(triIndex);
        }
        printf("[WORLD GLB COLLISION] large-tri grid: coarseSize=%.1f cells=%zu always=%zu\n",
               coarseSize, world.collisionLargeChunks.size(),
               world.collisionAlwaysLargeTriangles.size());
    }

    printf("[WORLD GLB COLLISION] chunks=%zu chunkSize=%.2f totalRefs=%llu maxChunksPerTri=%llu maxBounds=%.1f largeTris=%d\n",
           world.collisionChunks.size(), world.collisionChunkSize,
           (unsigned long long)totalRefs, (unsigned long long)maxChunks, maxBounds, largeTriCount);

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
    buildCollisionChunks(world, nullptr);

    printf("[COLLISION LOD] re-decimated collision revision=%u cellSize=%.2f tris=%zu\n",
           CollisionLodConfig::instance().revision(),
           CollisionLodConfig::instance().cellSize(),
           world.collisionMesh.triangles.size());
}

// Decimate a triangle list by vertex clustering (in place). Mirrors the render-mesh
// decimation for server / NPC collision worlds that have no render mesh.
void decimateCollisionTriangleList(std::vector<CollisionTriangle>& tris, float cellSize)
{
    if (cellSize <= 0.001f || tris.empty())
        return;

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
    }
    tris = std::move(out);
}

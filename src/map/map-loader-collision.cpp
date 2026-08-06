#include "map-loader-collision.h"
#include "world/world.h"
#include "world/world-gltf-loader.h"
#include <cstdio>
#include <cmath>
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

    bool boundsSet = false;
    int skippedDegenerate = 0;
    int skippedNonFinite = 0;
    int skippedExtreme = 0;
    constexpr float MAX_WORLD_EXTENT = 5000.0f;
    constexpr float MIN_TRI_AREA = 0.000001f;

    for (size_t i = 0; i + 2 < world.mesh.verts.size(); i += 3)
    {
        CollisionTriangle tri;
        tri.a = world.mesh.verts[i + 0].pos;
        tri.b = world.mesh.verts[i + 1].pos;
        tri.c = world.mesh.verts[i + 2].pos;

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
        " skipped: degenerate=%d nonFinite=%d extreme=%d\n",
        world.collisionMesh.triangles.size(),
        world.collisionMesh.boundsMin.x,
        world.collisionMesh.boundsMin.y,
        world.collisionMesh.boundsMin.z,
        world.collisionMesh.boundsMax.x,
        world.collisionMesh.boundsMax.y,
        world.collisionMesh.boundsMax.z,
        skippedDegenerate, skippedNonFinite, skippedExtreme
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

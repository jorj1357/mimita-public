#include "map-loader-collision.h"
#include "world/world.h"
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

void buildCollisionChunks(World& world)
{
    world.collisionChunks.clear();

    for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i)
    {
        const CollisionTriangle& tri = world.collisionMesh.triangles[i];
        glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c);
        glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c);
        glm::ivec3 c0 = collisionChunkCoord(mn, world.collisionChunkSize);
        glm::ivec3 c1 = collisionChunkCoord(mx, world.collisionChunkSize);

        for (int x = c0.x; x <= c1.x; ++x)
        for (int y = c0.y; y <= c1.y; ++y)
        for (int z = c0.z; z <= c1.z; ++z)
            world.collisionChunks[glm::ivec3(x, y, z)].push_back(i);
    }

    printf("[WORLD GLB COLLISION] chunks=%zu chunkSize=%.2f\n",
           world.collisionChunks.size(), world.collisionChunkSize);
}

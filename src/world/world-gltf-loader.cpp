// C:\important\mimita-priv-v8\src\world\world-gltf-loader.cpp
// 5 23 2026
/** purpose
 * do gltf loading for the world instead of the old json loader
 * so we cna have cones and sphres triangles etc
 * just all objects are point + line between points + faces between lines 3 dimesnions
 */

#include "world-gltf-loader.h"
#include "map/map_loader.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace {

void buildCollisionMeshFromRenderMesh(World& world)
{
    world.collisionMesh.clear();

    if (world.mesh.verts.size() < 3)
        return;

    bool boundsSet = false;
    for (size_t i = 0; i + 2 < world.mesh.verts.size(); i += 3)
    {
        CollisionTriangle tri;
        tri.a = world.mesh.verts[i + 0].pos;
        tri.b = world.mesh.verts[i + 1].pos;
        tri.c = world.mesh.verts[i + 2].pos;

        glm::vec3 n = glm::cross(tri.b - tri.a, tri.c - tri.a);
        float nLen = glm::length(n);
        if (nLen < 0.000001f)
            continue;
        tri.normal = n / nLen;

        world.collisionMesh.triangles.push_back(tri);

        glm::vec3 triMin = glm::min(glm::min(tri.a, tri.b), tri.c);
        glm::vec3 triMax = glm::max(glm::max(tri.a, tri.b), tri.c);
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
        "[WORLD GLB COLLISION] triangles=%zu bounds min=(%.2f %.2f %.2f) max=(%.2f %.2f %.2f)\n",
        world.collisionMesh.triangles.size(),
        world.collisionMesh.boundsMin.x,
        world.collisionMesh.boundsMin.y,
        world.collisionMesh.boundsMin.z,
        world.collisionMesh.boundsMax.x,
        world.collisionMesh.boundsMax.y,
        world.collisionMesh.boundsMax.z
    );
}

glm::ivec3 collisionChunkCoord(const glm::vec3& p, float size)
{
    return glm::ivec3(
        (int)std::floor(p.x / size),
        (int)std::floor(p.y / size),
        (int)std::floor(p.z / size)
    );
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

}

bool loadWorldFromGLB(World& world, const char* path)
{
    printf("[WORLD GLB] loading %s\n", path);

    world.clear();

    Debug::log(Debug::Category::GLB, "[WORLD GLB] before loadGLB\n");

    world.mesh = loadGLB(path);

    Debug::log(Debug::Category::GLB, "[WORLD GLB] after loadGLB\n");

    printf(
        "[WORLD GLB] verts=%zu triangles=%zu batches=%zu\n",
        world.mesh.verts.size(),
        world.mesh.verts.size() / 3,
        world.mesh.batches.size()
    );

    if (world.mesh.verts.empty())
    {
        printf("[WORLD GLB ERROR] GLB produced no renderable vertices\n");
        return false;
    }

    printf("[WORLD GLB] before collision build\n");

    buildCollisionMeshFromRenderMesh(world);
    buildCollisionChunks(world);

    printf("[WORLD GLB] after collision build\n");

    return true;
}

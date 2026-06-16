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
#include "utils/path_utils.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <chrono>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <glm/gtc/type_ptr.hpp>
#include <glm/geometric.hpp>

#include <tinygltf/tiny_gltf.h>

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
    const auto loadStarted = std::chrono::steady_clock::now();
    printf("[WORLD GLB] loading %s\n", path);

    World candidate;
    candidate.renderRevision = world.renderRevision + 1;

    Debug::log(Debug::Category::GLB, "[WORLD GLB] before loadGLB\n");

    candidate.mesh = loadGLB(path, true, &candidate.skyMesh);

    Debug::log(Debug::Category::GLB, "[WORLD GLB] after loadGLB\n");

    printf(
        "[WORLD GLB] verts=%zu triangles=%zu batches=%zu\n",
        candidate.mesh.verts.size(),
        candidate.mesh.verts.size() / 3,
        candidate.mesh.batches.size()
    );

    if (candidate.mesh.verts.empty())
    {
        printf("[WORLD GLB ERROR] GLB produced no renderable vertices\n");
        releaseMeshGLResources(candidate.mesh);
        return false;
    }

    printf("[WORLD GLB] before collision build\n");

    buildCollisionMeshFromRenderMesh(candidate);
    buildCollisionChunks(candidate);
    extractSpawnPointsFromGLB(candidate, path);

    printf("[WORLD GLB] after collision build\n");

    const auto unloadStarted = std::chrono::steady_clock::now();
    releaseMeshGLResources(world.mesh);
    world = std::move(candidate);
    const auto finished = std::chrono::steady_clock::now();
    const double unloadMs =
        std::chrono::duration<double, std::milli>(finished - unloadStarted).count();
    const double loadMs =
        std::chrono::duration<double, std::milli>(finished - loadStarted).count();
    printf("[WORLD GLB] load success path=%s revision=%llu spawns=%zu unloadMs=%.2f totalMs=%.2f\n",
           path, (unsigned long long)world.renderRevision,
           world.spawnPoints.size(), unloadMs, loadMs);
    return true;
}

void extractSpawnPointsFromGLB(World& world, const char* path)
{
    world.spawnPoints.clear();

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;
    std::string resolvedPath = resolveAssetPath(path);

    if (!loader.LoadBinaryFromFile(&model, &err, &warn, resolvedPath)) {
        printf("[SPAWN GLB] failed to load %s: %s\n", path, err.c_str());
        return;
    }

    std::unordered_map<std::string, int> spawnNameCounts;
    std::unordered_set<int> activeNodes;
    std::function<void(int, const glm::mat4&, int)> walkForSpawns =
        [&](int nodeIndex, const glm::mat4& parent, int depth) {
            if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size()) return;
            if (depth > 128) {
                printf("[SPAWN GLB WARNING] traversal depth exceeded node=%d\n", nodeIndex);
                return;
            }
            if (!activeNodes.insert(nodeIndex).second) {
                printf("[SPAWN GLB WARNING] cyclic node hierarchy node=%d\n", nodeIndex);
                return;
            }

            const tinygltf::Node& node = model.nodes[nodeIndex];

            glm::mat4 local(1.0f);
            if (node.matrix.size() == 16) {
                local = glm::make_mat4(node.matrix.data());
            } else {
                glm::vec3 T(0.0f);
                glm::quat R(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3 S(1.0f);
                if (node.translation.size() == 3)
                    T = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
                if (node.rotation.size() == 4)
                    R = glm::quat((float)node.rotation[3], (float)node.rotation[0],
                                  (float)node.rotation[1], (float)node.rotation[2]);
                if (node.scale.size() == 3)
                    S = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
                local = glm::translate(glm::mat4(1.0f), T)
                      * glm::mat4_cast(R)
                      * glm::scale(glm::mat4(1.0f), S);
            }
            glm::mat4 worldXform = parent * local;

            const std::string& name = node.name;
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });
            if (lowerName.find("spawn") != std::string::npos) {
                const glm::vec3 worldPosition = glm::vec3(worldXform[3]);
                const glm::vec3 xAxis = glm::vec3(worldXform[0]);
                const glm::vec3 yAxis = glm::vec3(worldXform[1]);
                const glm::vec3 zAxis = glm::vec3(worldXform[2]);
                const bool validPosition =
                    std::isfinite(worldPosition.x) &&
                    std::isfinite(worldPosition.y) &&
                    std::isfinite(worldPosition.z);
                const bool finiteBasis =
                    std::isfinite(xAxis.x) && std::isfinite(xAxis.y) && std::isfinite(xAxis.z) &&
                    std::isfinite(yAxis.x) && std::isfinite(yAxis.y) && std::isfinite(yAxis.z) &&
                    std::isfinite(zAxis.x) && std::isfinite(zAxis.y) && std::isfinite(zAxis.z);
                const bool validBasis =
                    finiteBasis &&
                    glm::length(xAxis) > 0.000001f &&
                    glm::length(yAxis) > 0.000001f &&
                    glm::length(zAxis) > 0.000001f;
                if (!validPosition || !validBasis) {
                    printf("[SPAWN GLB WARNING] rejected invalid transform node=%s index=%d\n",
                           name.c_str(), nodeIndex);
                } else {
                SpawnPoint sp;
                sp.position = worldPosition;
                const glm::mat3 rotationBasis(
                    glm::normalize(xAxis),
                    glm::normalize(yAxis),
                    glm::normalize(zAxis));
                sp.rotation = glm::normalize(glm::quat_cast(rotationBasis));
                sp.tag = name;
                sp.arenaIndex = -1;

                size_t arenaPos = lowerName.find("arena_");
                if (arenaPos != std::string::npos) {
                    const char* numStart = lowerName.c_str() + arenaPos + 6;
                    sp.arenaIndex = std::atoi(numStart);
                }

                const int duplicateCount = ++spawnNameCounts[lowerName];
                if (duplicateCount > 1)
                    printf("[SPAWN GLB WARNING] duplicate name=%s occurrence=%d\n",
                           name.c_str(), duplicateCount);
                world.spawnPoints.push_back(sp);
                printf("[SPAWN GLB] extracted index=%zu name=%s pos=(%.3f %.3f %.3f) arena=%d\n",
                       world.spawnPoints.size() - 1, name.c_str(),
                       sp.position.x, sp.position.y, sp.position.z, sp.arenaIndex);
                }
            }

            for (int child : node.children)
                walkForSpawns(child, worldXform, depth + 1);
            activeNodes.erase(nodeIndex);
        };

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size()) {
        for (int node : model.scenes[sceneIndex].nodes)
            walkForSpawns(node, glm::mat4(1.0f), 0);
    }

    printf("[SPAWN GLB] total spawn points extracted: %zu\n", world.spawnPoints.size());
}

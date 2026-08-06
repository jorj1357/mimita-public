// C:\important\mimita-priv-v8\src\world\world-gltf-loader.cpp
// 5 23 2026
/** purpose
 * do gltf loading for the world instead of the old json loader
 * so we cna have cones and sphres triangles etc
 * just all objects are point + line between points + faces between lines 3 dimesnions
 */

#include "world-gltf-loader.h"
#include "analytics/analytics-manager.h"
#include "map/map_loader.h"
#include "map/map-loader-collision.h"
#include "combat/weapon-model-cache.h"
#include "debug/debug-log.h"
#include "utils/path_utils.h"
#include "game/spawn-utils.h"

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



static double msSince(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

bool loadWorldFromGLB(World& world, const char* path)
{
    MapLoadTiming timing;
    MapLoadMetrics metrics;
    const auto loadStarted = std::chrono::steady_clock::now();

    Debug::warn(Debug::Category::World, "[MAP LOAD PHASE] loading path=%s", path);

    std::string resolvedPath = resolveAssetPath(path);
    {
        FILE* f = fopen(resolvedPath.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            metrics.glbFileBytes = ftell(f);
            fclose(f);
        }
    }

    World candidate;
    candidate.renderRevision = world.renderRevision + 1;

    Debug::log(Debug::Category::GLB, "[WORLD GLB] before loadGLB\n");

    auto tStart = std::chrono::steady_clock::now();
    candidate.mesh = loadGLB(path, true, &candidate.skyMesh, &timing, &metrics);
    timing.parse_glb = msSince(tStart);

    Debug::log(Debug::Category::GLB, "[WORLD GLB] after loadGLB\n");

    if (candidate.mesh.verts.empty())
    {
        Debug::warn(Debug::Category::World, "[MAP LOAD PHASE] GLB produced no renderable vertices path=%s", path);
        releaseMeshGLResources(candidate.mesh);
        return false;
    }

    metrics.finalExpandedVertexCount = candidate.mesh.verts.size();
    metrics.renderTriangleCount = candidate.mesh.verts.size() / 3;

    tStart = std::chrono::steady_clock::now();
    buildCollisionMeshFromRenderMesh(candidate);
    timing.collision_triangles = msSince(tStart);
    metrics.collisionTriangleCount = candidate.collisionMesh.triangles.size();

    tStart = std::chrono::steady_clock::now();
    buildCollisionChunks(candidate, &metrics);
    timing.collision_chunks = msSince(tStart);
    metrics.collisionChunkCount = candidate.collisionChunks.size();

    tStart = std::chrono::steady_clock::now();
    extractSpawnPointsFromGLB(candidate, path);
    double spawnTime = msSince(tStart);

    const auto unloadStarted = std::chrono::steady_clock::now();
    releaseMeshGLResources(world.mesh);
    world = std::move(candidate);
    const auto finished = std::chrono::steady_clock::now();
    timing.world_commit = std::chrono::duration<double, std::milli>(finished - unloadStarted).count();
    timing.total = std::chrono::duration<double, std::milli>(finished - loadStarted).count();

    // A map has been loaded on the main thread, so the GLB walker's default
    // texture is warm. Kick off background parses of every weapon model now so
    // the first equip of each weapon never stalls the frame.
    WeaponModelCache::instance().preloadAll();

    printf("[MAP LOAD PHASE] phase=read_glb time=%.1fms\n", timing.read_glb);
    printf("[MAP LOAD PHASE] phase=parse_glb time=%.1fms\n", timing.parse_glb);
    printf("[MAP LOAD PHASE] phase=materials_decode time=%.1fms\n", timing.materials_decode);
    printf("[MAP LOAD PHASE] phase=texture_upload time=%.1fms\n", timing.texture_upload);
    printf("[MAP LOAD PHASE] phase=scene_walk time=%.1fms\n", timing.scene_walk);
    printf("[MAP LOAD PHASE] phase=vertex_expansion time=%.1fms\n", timing.vertex_expansion);
    printf("[MAP LOAD PHASE] phase=batch_merge time=%.1fms\n", timing.batch_merge);
    printf("[MAP LOAD PHASE] phase=collision_triangles time=%.1fms\n", timing.collision_triangles);
    printf("[MAP LOAD PHASE] phase=collision_chunks time=%.1fms\n", timing.collision_chunks);
    printf("[MAP LOAD PHASE] phase=gpu_buffer_upload time=%.1fms\n", timing.gpu_buffer_upload);
    printf("[MAP LOAD PHASE] phase=world_commit time=%.1fms\n", timing.world_commit);
    printf("[MAP LOAD PHASE] phase=spawn_points time=%.1fms\n", spawnTime);
    printf("[MAP LOAD PHASE] phase=total time=%.1fms\n", timing.total);

    printf("[MAP LOAD METRICS] glbBytes=%llu meshes=%d nodes=%d primitives=%d images=%d\n",
           (unsigned long long)metrics.glbFileBytes, metrics.meshCount, metrics.nodeCount,
           metrics.primitiveCount, metrics.imageCount);
    printf("[MAP LOAD METRICS] decodedImageBytes=%llu sourceVerts=%llu sourceIndices=%llu\n",
           (unsigned long long)metrics.totalDecodedImageBytes,
           (unsigned long long)metrics.totalSourceVertices,
           (unsigned long long)metrics.totalSourceIndices);
    printf("[MAP LOAD METRICS] expandedVerts=%llu renderTris=%llu collisionTris=%llu\n",
           (unsigned long long)metrics.finalExpandedVertexCount,
           (unsigned long long)metrics.renderTriangleCount,
           (unsigned long long)metrics.collisionTriangleCount);
    printf("[MAP LOAD METRICS] collisionChunks=%llu totalChunkRefs=%llu maxChunksPerTri=%llu maxTriBoundsSize=%.1f\n",
           (unsigned long long)metrics.collisionChunkCount,
           (unsigned long long)metrics.totalTriangleToChunkRefs,
           (unsigned long long)metrics.maxChunksTouchedByOneTriangle,
           metrics.maxTriangleBoundsSize);
    printf("[MAP LOAD METRICS] largestImage=%dx%d largestPrimVerts=%llu largestPrimIndices=%llu\n",
           metrics.largestImageW, metrics.largestImageH,
           (unsigned long long)metrics.largestPrimitiveVertexCount,
           (unsigned long long)metrics.largestPrimitiveIndexCount);

    printf("[WORLD GLB] load success path=%s revision=%llu spawns=%zu\n",
           path, (unsigned long long)world.renderRevision,
           world.spawnPoints.size());
    AnalyticsManager::instance().trackMapLoaded(
        path,
        (int)world.collisionMesh.triangles.size(),
        (int)world.spawnPoints.size());
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

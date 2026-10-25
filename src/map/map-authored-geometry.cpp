// 2026-10-23
#include "map/map-authored-geometry.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include "debug/structured-log.h"
#include "config/collision-lod-config.h"
#include "gamemode/map-config.h"
#include "map/map-loader-collision.h"
#include "map/map-static-union.h"
#include "network/server.h"
#include "physics/physical-entity.h"
#include "world/texture-store.h"
#include "world/world.h"

namespace {

const glm::vec3 kCubePositions[8] = {
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
};

const int kCubeFaces[6][4] = {
    {4, 5, 6, 7}, // +Z
    {1, 0, 3, 2}, // -Z
    {5, 1, 2, 6}, // +X
    {0, 4, 7, 3}, // -X
    {3, 7, 6, 2}, // +Y
    {0, 1, 5, 4}, // -Y
};

const glm::vec3 kCubeNormals[6] = {
    {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0},
};

GLuint resolveBlockTexture(const std::string& material)
{
    if (material.empty()) return 0;
    if (material.find('/') != std::string::npos ||
        material.find('\\') != std::string::npos ||
        material.find('.') != std::string::npos)
        return gTextures.getPath(material);
    return gTextures.get(material);
}

bool finiteVec(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

glm::mat4 blockTransform(const AuthoredBlock& block)
{
    const glm::mat4 translate = glm::translate(glm::mat4(1.0f), block.position);
    const glm::mat4 rotate = glm::rotate(glm::mat4(1.0f),
        glm::radians(block.rotationEuler.z), glm::vec3(0, 0, 1));
    const glm::mat4 rotateY = glm::rotate(glm::mat4(1.0f),
        glm::radians(block.rotationEuler.y), glm::vec3(0, 1, 0));
    const glm::mat4 rotateX = glm::rotate(glm::mat4(1.0f),
        glm::radians(block.rotationEuler.x), glm::vec3(1, 0, 0));
    const glm::mat4 scale = glm::scale(glm::mat4(1.0f), block.size);
    return translate * rotate * rotateY * rotateX * scale;
}

void appendBlockMesh(World& world, const AuthoredBlock& block)
{
    const glm::mat4 transform = blockTransform(block);
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

    Mesh::Batch batch;
    batch.materialName = block.material.empty() ? "default" : block.material;
    batch.texture = resolveBlockTexture(block.material);
    batch.first = world.mesh.verts.size();
    batch.count = 36;
    batch.boundsMin = glm::vec3(std::numeric_limits<float>::max());
    batch.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    batch.hasBounds = true;

    for (int face = 0; face < 6; ++face) {
        const int order[6] = {0, 1, 2, 0, 2, 3};
        const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (int i : order) {
            const glm::vec3 local = kCubePositions[kCubeFaces[face][i]];
            Vertex v;
            v.pos = glm::vec3(transform * glm::vec4(local, 1.0f));
            v.normal = glm::normalize(normalMatrix * kCubeNormals[face]);
            v.uv = uv[i];
            world.mesh.verts.push_back(v);
            batch.boundsMin = glm::min(batch.boundsMin, v.pos);
            batch.boundsMax = glm::max(batch.boundsMax, v.pos);
        }
    }
    const size_t firstVertex = batch.first;
    const size_t vertexCount = batch.count;
    const glm::vec3 blockBoundsMin = batch.boundsMin;
    const glm::vec3 blockBoundsMax = batch.boundsMax;
    world.mesh.batches.push_back(std::move(batch));

    MapGLBObject object;
    object.id = "block:" + block.id;
    object.name = block.id;
    object.transform = transform;
    object.vertexFirst = firstVertex;
    object.vertexCount = vertexCount;
    object.boundsMin = blockBoundsMin;
    object.boundsMax = blockBoundsMax;
    world.authoredObjects.push_back(std::move(object));
}

bool ensureMapLoaded(const std::string& mapId)
{
    if (mapId.empty()) return false;
    MapConfigRegistry& registry = MapConfigRegistry::instance();
    if (registry.mapId() != mapId)
        return registry.load(mapId);
    return true;
}

} // namespace

std::string mapIdFromAssetPath(const std::string& path)
{
    if (path.empty()) return {};
    return std::filesystem::path(path).stem().string();
}

void applyGLBObjectOverrides(World& world,
                             const std::vector<MapGLBObjectOverride>& overrides)
{
    for (MapGLBObject& object : world.mesh.objects) {
        glm::vec3 target = object.basePosition;
        for (const MapGLBObjectOverride& override : overrides) {
            if (override.id == object.id) {
                target = override.position;
                break;
            }
        }
        const glm::vec3 current(object.transform[3]);
        const glm::vec3 delta = target - current;
        if (glm::dot(delta, delta) <= 1e-10f) continue;
        const size_t first = std::min(object.vertexFirst, world.mesh.verts.size());
        const size_t last = std::min(object.vertexFirst + object.vertexCount,
                                     world.mesh.verts.size());
        for (size_t i = first; i < last; ++i)
            world.mesh.verts[i].pos += delta;
        object.transform[3] += glm::vec4(delta, 0.0f);
        object.boundsMin += delta;
        object.boundsMax += delta;
    }
}

void applyGLBObjectOverrides(MimitaNet::HeadlessWorld& world,
                             const std::vector<MapGLBObjectOverride>& overrides)
{
    for (MimitaNet::HeadlessGLBObject& object : world.glbObjects) {
        glm::vec3 target = object.basePosition;
        for (const MapGLBObjectOverride& override : overrides) {
            if (override.id == object.id) {
                target = override.position;
                break;
            }
        }
        const glm::vec3 delta = target - object.position;
        if (glm::dot(delta, delta) <= 1e-10f) continue;
        const size_t first = std::min(object.triangleFirst, world.triangles.size());
        const size_t last = std::min(object.triangleFirst + object.triangleCount,
                                     world.triangles.size());
        for (size_t i = first; i < last; ++i) {
            CollisionTriangle& triangle = world.triangles[i];
            triangle.a += delta;
            triangle.b += delta;
            triangle.c += delta;
        }
        object.position = target;
    }
    if (!world.triangles.empty()) {
        world.boundsMin = glm::vec3(std::numeric_limits<float>::max());
        world.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
        for (const CollisionTriangle& triangle : world.triangles) {
            const glm::vec3 mn = glm::min(glm::min(triangle.a, triangle.b), triangle.c);
            const glm::vec3 mx = glm::max(glm::max(triangle.a, triangle.b), triangle.c);
            world.boundsMin = glm::min(world.boundsMin, mn);
            world.boundsMax = glm::max(world.boundsMax, mx);
        }
    }
}
void clearAuthoredMapGeometry(World& world)
{
    if (!world.authoredGeometryApplied) return;
    if (world.mesh.verts.size() < world.authoredBaseVertexCount ||
        world.mesh.batches.size() < world.authoredBaseBatchCount) {
        world.authoredGeometryApplied = false;
        return;
    }
    world.mesh.verts.resize(world.authoredBaseVertexCount);
    world.mesh.batches.resize(world.authoredBaseBatchCount);
    world.authoredObjects.clear();
    world.collisionMesh.clear();
    world.renderRevision++;
}

bool applyAuthoredMapGeometry(World& world, const std::string& mapId,
                              const AuthoredGeometryOptions& options)
{
    if (!ensureMapLoaded(mapId)) return false;
    const MapObjectiveConfig& config = MapConfigRegistry::instance().current();

    if (world.authoredGeometryApplied)
        clearAuthoredMapGeometry(world);
    else {
        world.authoredBaseVertexCount = world.mesh.verts.size();
        world.authoredBaseBatchCount = world.mesh.batches.size();
        world.authoredGeometryApplied = true;
    }

    applyGLBObjectOverrides(world, config.glbObjects);

    size_t added = 0;
    for (const AuthoredBlock& block : config.blocks) {
        if (!block.enabled || !finiteVec(block.position) || !finiteVec(block.size))
            continue;
        if (block.size.x <= 0.0001f || block.size.y <= 0.0001f || block.size.z <= 0.0001f)
            continue;
        appendBlockMesh(world, block);
        ++added;
    }

    StaticUnionStats unionStats;
    StaticUnionStats removalStats;
    if (options.rebuildCollision) {
        buildCollisionMeshFromRenderMesh(world);
        if (CollisionLodConfig::instance().staticSolidUnion())
            unionStaticSolidGroups(
                world.collisionMesh.triangles, world.collisionMesh.solidIds,
                CollisionLodConfig::instance().staticSolidUnionMaxTriangles(),
                &unionStats,
                CollisionLodConfig::instance().staticSolidUnionMaxSolids());
        if (options.staticFaceRemoval && CollisionLodConfig::instance().staticFaceRemoval())
            removeFullyCoveredStaticTriangles(
                world.collisionMesh.triangles, world.collisionMesh.solidIds,
                &removalStats);
        buildCollisionChunks(world, nullptr);
    }
    world.renderRevision++;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-authored-geometry.applied", mapId,
        "authored block geometry applied to client world", 0,
        nlohmann::json{{"map", mapId},
                       {"block_count", config.blocks.size()},
                       {"applied_count", added},
                       {"render_vertices", world.mesh.verts.size()},
                       {"collision_triangles", world.collisionMesh.triangles.size()},
                       {"static_union_solids", unionStats.unionSolids},
                       {"static_union_output_triangles", unionStats.unionOutputTriangles},
                       {"static_union_ms", unionStats.unionMs},
                       {"static_union_fallback", unionStats.unionFallback},
                       {"static_removed_triangles", removalStats.removedTriangles},
                       {"static_removal_fallback", removalStats.fallback},
                       {"rebuild_collision", options.rebuildCollision},
                       {"render_revision", world.renderRevision}},
        __FILE__, __LINE__, __FUNCTION__);
    printf("[MAP AUTHORED] map=%s blocks=%zu applied=%zu verts=%zu tris=%zu revision=%llu\n",
           mapId.c_str(), config.blocks.size(), added,
           world.mesh.verts.size(), world.collisionMesh.triangles.size(),
           (unsigned long long)world.renderRevision);
    return true;
}

bool applyAuthoredMapGeometry(MimitaNet::HeadlessWorld& world, const std::string& mapId)
{
    if (!ensureMapLoaded(mapId)) return false;
    const MapObjectiveConfig& config = MapConfigRegistry::instance().current();
    applyGLBObjectOverrides(world, config.glbObjects);
    size_t added = 0;
    for (const AuthoredBlock& block : config.blocks) {
        if (!block.enabled || !finiteVec(block.position) || !finiteVec(block.size))
            continue;
        if (block.size.x <= 0.0001f || block.size.y <= 0.0001f || block.size.z <= 0.0001f)
            continue;
        MimitaNet::HeadlessWorld cube;
        buildBoxCollisionTriangles(cube.triangles, glm::vec3(0.0f), block.size * 0.5f);
        MimitaNet::appendHeadlessWorldInstance(world, cube, blockTransform(block));
        ++added;
    }

    StaticUnionStats unionStats;
    if (CollisionLodConfig::instance().staticSolidUnion())
        unionStaticSolidGroups(
            world.triangles, world.solidIds,
            CollisionLodConfig::instance().staticSolidUnionMaxTriangles(),
            &unionStats,
            CollisionLodConfig::instance().staticSolidUnionMaxSolids());
    StaticUnionStats removalStats;
    if (CollisionLodConfig::instance().staticFaceRemoval())
        removeFullyCoveredStaticTriangles(
            world.triangles, world.solidIds, &removalStats);
    MimitaNet::buildHeadlessCollisionChunks(world);

    // Authored spawnpoint entities extend the GLB spawn list. The base count
    // keeps repeated map rebuilds from duplicating them.
    if (!world.authoredSpawnPointsApplied) {
        world.authoredSpawnPointBaseCount = world.spawnPoints.size();
        world.authoredSpawnPointsApplied = true;
    } else {
        world.spawnPoints.resize(world.authoredSpawnPointBaseCount);
    }
    size_t spawnsAdded = 0;
    for (const MapEntity& entity : config.entities) {
        if (!entity.enabled || entity.type != "spawnpoint") continue;
        if (!finiteVec(entity.position)) continue;
        MimitaNet::ServerSpawnPoint spawn;
        spawn.position = entity.position;
        spawn.yaw = glm::radians(entity.yaw);
        spawn.tag = entity.tag.empty() ? entity.id : entity.tag;
        world.spawnPoints.push_back(std::move(spawn));
        ++spawnsAdded;
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-authored-geometry.applied", mapId,
        "authored block geometry applied to headless world", 0,
        nlohmann::json{{"map", mapId},
                       {"block_count", config.blocks.size()},
                       {"applied_count", added},
                       {"authored_spawn_count", spawnsAdded},
                       {"spawn_points", world.spawnPoints.size()},
                       {"collision_triangles", world.triangles.size()},
                       {"static_removed_triangles", unionStats.removedTriangles},
                       {"static_removal_fallback", removalStats.fallback},
                       {"headless", true}},
        __FILE__, __LINE__, __FUNCTION__);
    printf("[MAP AUTHORED] headless map=%s blocks=%zu applied=%zu spawns=+%zu total_spawns=%zu tris=%zu\n",
           mapId.c_str(), config.blocks.size(), added, spawnsAdded,
           world.spawnPoints.size(), world.triangles.size());
    return true;
}

bool authoredMapGeometrySelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) {
        ok = false;
        report += "FAIL: " + why + "\n";
    };

    const std::string mapId = "zz_authored_geometry_selftest";
    const std::string path = MapConfigRegistry::pathForMap(mapId);
    {
        std::ofstream out(path);
        out << R"({
  "blocks": [
    {"id":"b1","shape":"box","position":[2,3,4],"size":[2,2,2],
     "rotationEuler":[0,0,0],"material":"","enabled":true,"tags":["test"]}
  ],
  "entities": [
    {"id":"sp1","type":"spawnpoint","tag":"attackers","position":[5,6,7],
     "yaw":90,"enabled":true}
  ]
})";
    }
    MapConfigRegistry& registry = MapConfigRegistry::instance();
    if (!registry.load(mapId)) fail("selftest map failed to load");
    if (registry.current().blocks.size() != 1) fail("block was not parsed");

    World world;
    world.mesh.verts.push_back({{0, 0, 0}, {0, 0, 1}, {0, 0}});
    world.mesh.verts.push_back({{1, 0, 0}, {0, 0, 1}, {1, 0}});
    world.mesh.verts.push_back({{0, 1, 0}, {0, 0, 1}, {0, 1}});
    Mesh::Batch base;
    base.first = 0;
    base.count = 3;
    world.mesh.batches.push_back(base);
    CollisionTriangle tri;
    tri.a = {0, 0, 0};
    tri.b = {1, 0, 0};
    tri.c = {0, 1, 0};
    tri.normal = {0, 0, 1};
    world.collisionMesh.triangles.push_back(tri);
    buildCollisionChunks(world, nullptr);

    const size_t baseVerts = world.mesh.verts.size();
    const size_t baseTris = world.collisionMesh.triangles.size();
    if (!applyAuthoredMapGeometry(world, mapId)) fail("applyAuthoredMapGeometry failed");
    if (world.mesh.verts.size() != baseVerts + 36) fail("authored block mesh count");
    if (world.collisionMesh.triangles.size() != baseTris + 12) fail("authored block collision count");
    if (world.authoredBaseVertexCount != baseVerts) fail("authored base vertex count");

    if (!applyAuthoredMapGeometry(world, mapId)) fail("authored rebuild failed");
    if (world.mesh.verts.size() != baseVerts + 36) fail("authored rebuild duplicated geometry");

    // Authored spawnpoint entities extend the headless spawn list once per
    // rebuild and never duplicate the GLB base entries.
    {
        MimitaNet::HeadlessWorld headless;
        MimitaNet::ServerSpawnPoint baseSpawn;
        baseSpawn.position = glm::vec3(0.0f);
        baseSpawn.tag = "spawnpoint.base";
        headless.spawnPoints.push_back(baseSpawn);
        if (!applyAuthoredMapGeometry(headless, mapId)) fail("headless authored apply failed");
        if (headless.spawnPoints.size() != 2) fail("authored spawnpoint not appended");
        if (!applyAuthoredMapGeometry(headless, mapId)) fail("headless authored rebuild failed");
        if (headless.spawnPoints.size() != 2) fail("authored spawnpoint duplicated on rebuild");
    }

    // GLB object translation overrides must apply, drive an incremental
    // collision refresh, and restore when the override is removed (undo).
    {
        World glbWorld;
        glbWorld.mesh.verts.push_back({{0, 0, 0}, {0, 0, 1}, {0, 0}});
        glbWorld.mesh.verts.push_back({{1, 0, 0}, {0, 0, 1}, {1, 0}});
        glbWorld.mesh.verts.push_back({{0, 1, 0}, {0, 0, 1}, {0, 1}});
        Mesh::Batch batch;
        batch.first = 0;
        batch.count = 3;
        glbWorld.mesh.batches.push_back(batch);
        MapGLBObject object;
        object.id = "node:0";
        object.name = "SyntheticNode";
        object.transform = glm::mat4(1.0f);
        object.basePosition = glm::vec3(0.0f);
        object.boundsMin = glm::vec3(0.0f);
        object.boundsMax = glm::vec3(1.0f, 1.0f, 0.0f);
        object.vertexFirst = 0;
        object.vertexCount = 3;
        glbWorld.mesh.objects.push_back(object);
        buildCollisionMeshFromRenderMesh(glbWorld);
        buildCollisionChunks(glbWorld, nullptr);
        const AABB oldBounds{object.boundsMin, object.boundsMax};
        std::vector<MapGLBObjectOverride> overrides;
        overrides.push_back({"node:0", "SyntheticNode", glm::vec3(5.0f, 0.0f, 0.0f)});
        applyGLBObjectOverrides(glbWorld, overrides);
        if (std::abs(glbWorld.mesh.verts[0].pos.x - 5.0f) > 0.001f)
            fail("glb override did not translate render vertices");
        const AABB movedBounds{glbWorld.mesh.objects[0].boundsMin,
                               glbWorld.mesh.objects[0].boundsMax};
        updateCollisionForMovedObject(glbWorld, oldBounds, movedBounds);
        if (glbWorld.collisionMesh.triangles.empty() ||
            std::abs(glbWorld.collisionMesh.triangles[0].a.x - 5.0f) > 0.001f)
            fail("glb override did not update collision");
        applyGLBObjectOverrides(glbWorld, {});
        if (std::abs(glbWorld.mesh.verts[0].pos.x) > 0.001f)
            fail("removing glb override did not restore the base position");
    }
    std::error_code ec;
    std::filesystem::remove(path, ec);
    registry.load("zombietower4");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}

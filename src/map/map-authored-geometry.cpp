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
#include "gamemode/map-config.h"
#include "map/map-loader-collision.h"
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
    world.mesh.batches.push_back(std::move(batch));
}

void appendBlockCollision(World& world, const AuthoredBlock& block)
{
    const glm::mat4 transform = blockTransform(block);
    std::vector<CollisionTriangle> local;
    buildBoxCollisionTriangles(local, glm::vec3(0.0f), block.size * 0.5f);
    world.collisionMesh.triangles.reserve(world.collisionMesh.triangles.size() + local.size());
    for (const CollisionTriangle& source : local) {
        CollisionTriangle tri;
        tri.a = glm::vec3(transform * glm::vec4(source.a, 1.0f));
        tri.b = glm::vec3(transform * glm::vec4(source.b, 1.0f));
        tri.c = glm::vec3(transform * glm::vec4(source.c, 1.0f));
        const glm::vec3 normal = glm::cross(tri.b - tri.a, tri.c - tri.a);
        const float length = glm::length(normal);
        if (length < 1e-6f) continue;
        tri.normal = normal / length;
        world.collisionMesh.triangles.push_back(tri);
    }
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

void clearAuthoredMapGeometry(World& world)
{
    if (!world.authoredGeometryApplied) return;
    if (world.mesh.verts.size() < world.authoredBaseVertexCount ||
        world.mesh.batches.size() < world.authoredBaseBatchCount ||
        world.collisionMesh.triangles.size() < world.authoredBaseCollisionCount) {
        world.authoredGeometryApplied = false;
        return;
    }
    world.mesh.verts.resize(world.authoredBaseVertexCount);
    world.mesh.batches.resize(world.authoredBaseBatchCount);
    world.collisionMesh.triangles.resize(world.authoredBaseCollisionCount);
    world.collisionMesh.boundsMin = glm::vec3(0.0f);
    world.collisionMesh.boundsMax = glm::vec3(0.0f);
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        const glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c);
        const glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c);
        world.collisionMesh.boundsMin = glm::min(world.collisionMesh.boundsMin, mn);
        world.collisionMesh.boundsMax = glm::max(world.collisionMesh.boundsMax, mx);
    }
    buildCollisionChunks(world, nullptr);
    world.renderRevision++;
}

bool applyAuthoredMapGeometry(World& world, const std::string& mapId)
{
    if (!ensureMapLoaded(mapId)) return false;
    const MapObjectiveConfig& config = MapConfigRegistry::instance().current();

    if (world.authoredGeometryApplied)
        clearAuthoredMapGeometry(world);
    else {
        world.authoredBaseVertexCount = world.mesh.verts.size();
        world.authoredBaseBatchCount = world.mesh.batches.size();
        world.authoredBaseCollisionCount = world.collisionMesh.triangles.size();
        world.authoredGeometryApplied = true;
    }

    size_t added = 0;
    for (const AuthoredBlock& block : config.blocks) {
        if (!block.enabled || !finiteVec(block.position) || !finiteVec(block.size))
            continue;
        if (block.size.x <= 0.0001f || block.size.y <= 0.0001f || block.size.z <= 0.0001f)
            continue;
        appendBlockMesh(world, block);
        appendBlockCollision(world, block);
        ++added;
    }

    if (added > 0) {
        bool first = true;
        for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
            const glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c);
            const glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c);
            if (first) {
                world.collisionMesh.boundsMin = mn;
                world.collisionMesh.boundsMax = mx;
                first = false;
            } else {
                world.collisionMesh.boundsMin = glm::min(world.collisionMesh.boundsMin, mn);
                world.collisionMesh.boundsMax = glm::max(world.collisionMesh.boundsMax, mx);
            }
        }
    }
    buildCollisionChunks(world, nullptr);
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
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-authored-geometry.applied", mapId,
        "authored block geometry applied to headless world", 0,
        nlohmann::json{{"map", mapId},
                       {"block_count", config.blocks.size()},
                       {"applied_count", added},
                       {"collision_triangles", world.triangles.size()},
                       {"headless", true}},
        __FILE__, __LINE__, __FUNCTION__);
    printf("[MAP AUTHORED] headless map=%s blocks=%zu applied=%zu tris=%zu\n",
           mapId.c_str(), config.blocks.size(), added, world.triangles.size());
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

    std::error_code ec;
    std::filesystem::remove(path, ec);
    registry.load("zombietower4");

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}

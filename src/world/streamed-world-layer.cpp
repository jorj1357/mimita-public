// 2026-11-05
#include "world/streamed-world-layer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "config/collision-lod-config.h"
#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "hot-reload/module-host.h"
#include "map/map-loader-collision.h"
#include "map/map-static-union.h"
#include "modules/api/actor-contact-policy-api.h"
#include "modules/api/chunk-collision-api.h"
#include "physics/physical-entity.h"
#include "render/world-box-mesh.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "world/world.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;
extern void setUniforms(GLuint shader, const glm::vec3& cameraPos);

namespace {

glm::mat4 partMatrix(const ChunkPart& part)
{
    return glm::translate(glm::mat4(1.0f), part.position) *
           glm::rotate(glm::mat4(1.0f), glm::radians(part.rotationEuler.z),
                       glm::vec3(0, 0, 1)) *
           glm::rotate(glm::mat4(1.0f), glm::radians(part.rotationEuler.y),
                       glm::vec3(0, 1, 0)) *
           glm::rotate(glm::mat4(1.0f), glm::radians(part.rotationEuler.x),
                       glm::vec3(1, 0, 0)) *
           glm::scale(glm::mat4(1.0f), part.size);
}

float distanceToBounds(const glm::vec3& point, const AABB& bounds)
{
    const glm::vec3 closest = glm::clamp(point, bounds.min, bounds.max);
    return glm::length(closest - point);
}

void appendAabbTriangles(std::vector<CollisionTriangle>& out, const AABB& b)
{
    const glm::vec3 p[8] = {
        {b.min.x, b.min.y, b.min.z}, {b.max.x, b.min.y, b.min.z},
        {b.max.x, b.max.y, b.min.z}, {b.min.x, b.max.y, b.min.z},
        {b.min.x, b.min.y, b.max.z}, {b.max.x, b.min.y, b.max.z},
        {b.max.x, b.max.y, b.max.z}, {b.min.x, b.max.y, b.max.z}};
    static const int faces[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
        {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
    for (const auto& face : faces)
        for (int tri : {0, 1, 2, 0, 2, 3}) {
            CollisionTriangle triangle;
            triangle.a = p[face[tri]];
            triangle.b = p[face[(tri + 1) % 4]];
            triangle.c = p[face[(tri + 2) % 4]];
            const glm::vec3 normal = glm::cross(triangle.b - triangle.a,
                                                triangle.c - triangle.a);
            const float length = glm::length(normal);
            if (length > 1e-8f)
                triangle.normal = normal / length;
            out.push_back(triangle);
        }
}

AABB transformedChunkBounds(const AuthoredChunk& chunk,
                            const glm::mat4& transform)
{
    AABB bounds;
    bounds.min = glm::vec3(std::numeric_limits<float>::max());
    bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 corner(
            (i & 1) ? chunk.boundsMax.x : chunk.boundsMin.x,
            (i & 2) ? chunk.boundsMax.y : chunk.boundsMin.y,
            (i & 4) ? chunk.boundsMax.z : chunk.boundsMin.z);
        const glm::vec3 world = glm::vec3(transform * glm::vec4(corner, 1.0f));
        bounds.min = glm::min(bounds.min, world);
        bounds.max = glm::max(bounds.max, world);
    }
    return bounds;
}

std::string aliasedMaterial(const std::string& material)
{
    if (material == "stone_floor") return "floorv2";
    if (material == "stone_wall") return "wallv2";
    if (material == "stone") return "stonev2";
    return material;
}

GLuint resolveMaterial(const std::string& material)
{
    if (material.empty()) return gTextures.get("default");
    std::string resolved = aliasedMaterial(material);
    if (resolved != material) {
        static std::unordered_set<std::string> logged;
        if (logged.insert(material).second) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "world-stream.material.alias", material,
                "legacy chunk material resolved to an existing texture", 0,
                {{"requested", material}, {"resolved", resolved}},
                __FILE__, __LINE__, __func__);
        }
    }
    if (resolved.find('/') != std::string::npos ||
        resolved.find('\\') != std::string::npos ||
        resolved.find('.') != std::string::npos)
        return gTextures.getPath(resolved);
    return gTextures.get(resolved);
}

void appendBoxFaces(std::vector<Vertex>& out, const ChunkPart& part,
                    std::vector<Mesh::Batch>& batches,
                    const glm::mat4& localTransform)
{
    mimita_world_rendering::BoxMeshV1 request{};
    const float* transformValues = &localTransform[0][0];
    for (int i = 0; i < 16; ++i)
        request.transform[i] = transformValues[i];
    request.size[0] = part.size.x;
    request.size[1] = part.size.y;
    request.size[2] = part.size.z;
    request.uvScale[0] = part.uvScale.x > 0.0f ? part.uvScale.x : 1.0f;
    request.uvScale[1] = part.uvScale.y > 0.0f ? part.uvScale.y : 1.0f;
    for (int face = 0; face < 6; ++face) {
        const std::string& material = part.faceMaterials[face].empty()
            ? part.material : part.faceMaterials[face];
        std::snprintf(request.materials[face], sizeof(request.materials[face]),
                      "%s", material.c_str());
        request.faceUvScale[face][0] = part.faceUvScale[face].x;
        request.faceUvScale[face][1] = part.faceUvScale[face].y;
    }

    mimita_world_rendering::BoxMeshV1 result{};
    if (!buildBoxMesh(request, result))
        return;

    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int face = 0; face < 6; ++face) {
        const mimita_world_rendering::MeshFaceV1& faceMesh = result.faces[face];
        const size_t first = out.size();
        for (int i : order) {
            Vertex vertex;
            vertex.pos = glm::vec3(faceMesh.positions[i][0],
                                   faceMesh.positions[i][1],
                                   faceMesh.positions[i][2]);
            vertex.normal = glm::vec3(faceMesh.normal[0], faceMesh.normal[1],
                                      faceMesh.normal[2]);
            vertex.uv = glm::vec2(faceMesh.uv[i][0], faceMesh.uv[i][1]);
            out.push_back(vertex);
        }
        Mesh::Batch batch;
        const std::string material = faceMesh.material;
        batch.materialName = material.empty() ? "default" : material;
        batch.texture = resolveMaterial(material);
        batch.first = first;
        batch.count = 6;
        batch.hasBounds = true;
        batch.boundsMin = glm::vec3(std::numeric_limits<float>::max());
        batch.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
        for (size_t i = first; i < out.size(); ++i) {
            batch.boundsMin = glm::min(batch.boundsMin, out[i].pos);
            batch.boundsMax = glm::max(batch.boundsMax, out[i].pos);
        }
        batches.push_back(std::move(batch));
    }
}

void buildChunkCollision(const AuthoredChunk& chunk, const glm::mat4& transform,
                         std::vector<CollisionTriangle>& out,
                         std::vector<uint32_t>& solidIds)
{
    out.clear();
    solidIds.clear();
    std::vector<mimita_chunk_collision::CollisionPartV1> parts;
    parts.reserve(chunk.parts.size());
    for (const ChunkPart& part : chunk.parts) {
        if (!part.collision)
            continue;
        mimita_chunk_collision::CollisionPartV1 view{};
        std::snprintf(view.id, sizeof(view.id), "%s", part.id.c_str());
        view.position[0] = part.position.x;
        view.position[1] = part.position.y;
        view.position[2] = part.position.z;
        view.size[0] = part.size.x;
        view.size[1] = part.size.y;
        view.size[2] = part.size.z;
        view.rotation[0] = part.rotationEuler.x;
        view.rotation[1] = part.rotationEuler.y;
        view.rotation[2] = part.rotationEuler.z;
        view.collision = part.collision ? 1 : 0;
        parts.push_back(view);
    }
    if (parts.empty())
        return;

    std::vector<mimita_chunk_collision::TriangleV1> triangles(
        mimita_chunk_collision::MAX_OUTPUT_TRIANGLES);
    std::vector<uint32_t> triangleSolidIds(
        mimita_chunk_collision::MAX_OUTPUT_TRIANGLES);
    mimita_chunk_collision::BuildChunkV1 request{};
    std::snprintf(request.chunkId, sizeof(request.chunkId), "%s",
                  chunk.id.c_str());
    const float* transformValues = &transform[0][0];
    for (int i = 0; i < 16; ++i)
        request.chunkTransform[i] = transformValues[i];
    request.parts = parts.data();
    request.partCount = static_cast<uint32_t>(parts.size());
    request.outTriangles = triangles.data();
    request.outSolidIds = triangleSolidIds.data();
    request.capacity = static_cast<uint32_t>(triangles.size());

    bool built = ModuleHost::instance().chunkCollisionBuild(&request) &&
        request.result && request.outCount > 0;
    if (!built) {
        // Safety fallback before the module is loaded: use the shared box
        // triangle helper with the same world transform.
        for (const ChunkPart& part : chunk.parts) {
            if (!part.collision)
                continue;
            std::vector<CollisionTriangle> local;
            buildBoxCollisionTriangles(local, glm::vec3(0.0f),
                                       part.size * 0.5f);
            const glm::mat4 localToWorld = transform * partMatrix(part);
            const glm::mat3 normalMatrix =
                glm::transpose(glm::inverse(glm::mat3(localToWorld)));
            const uint32_t solidId = static_cast<uint32_t>(solidIds.size() / 12) + 1;
            for (const CollisionTriangle& source : local) {
                CollisionTriangle world;
                world.a = glm::vec3(localToWorld * glm::vec4(source.a, 1.0f));
                world.b = glm::vec3(localToWorld * glm::vec4(source.b, 1.0f));
                world.c = glm::vec3(localToWorld * glm::vec4(source.c, 1.0f));
                world.normal = glm::normalize(normalMatrix * source.normal);
                out.push_back(world);
                solidIds.push_back(solidId);
            }
        }
    } else {
        out.reserve(request.outCount);
        solidIds.reserve(request.outCount);
        for (uint32_t i = 0; i < request.outCount; ++i) {
            const auto& source = triangles[i];
            CollisionTriangle triangle;
            triangle.a = glm::vec3(source.a[0], source.a[1], source.a[2]);
            triangle.b = glm::vec3(source.b[0], source.b[1], source.b[2]);
            triangle.c = glm::vec3(source.c[0], source.c[1], source.c[2]);
            triangle.normal =
                glm::vec3(source.normal[0], source.normal[1], source.normal[2]);
            out.push_back(triangle);
            solidIds.push_back(triangleSolidIds[i]);
        }
    }

}

} // namespace

bool buildStreamedChunkCollision(
    const AuthoredChunk& chunk, const glm::mat4& transform,
    std::vector<CollisionTriangle>& out, std::vector<uint32_t>& solidIds)
{
    buildChunkCollision(chunk, transform, out, solidIds);
    return !out.empty();
}

StreamedWorldLayer& StreamedWorldLayer::instance()
{
    static StreamedWorldLayer layer;
    return layer;
}

StreamedWorldLayer::~StreamedWorldLayer()
{
    clear();
}

void StreamedWorldLayer::clear()
{
    for (const ChunkRecord& record : mChunks) {
        if (record.vao) glDeleteVertexArrays(1, &record.vao);
        if (record.vbo) glDeleteBuffers(1, &record.vbo);
    }
    mChunks.clear();
    mCollisionDirty = true;
    mCollidedChunkCount = 0;
    mCollisionTriangleCount = 0;
    mCollisionRemovedFaces = 0;
    mChunksInstalledTotal = 0;
    mChunksRemovedTotal = 0;
    mLastCollisionRebuildMs = 0.0;
    mMaxCollisionRebuildMs = 0.0;
    mLastLoadedChunkCount = 0;
    mBaseBroadphaseTriangles = 0;
    mLastCollisionRevisionMatched = true;
    mHasStreamFocus = false;
    mStreamFocus = glm::vec3(0.0f);
    mCollisionRadiusM = 0.0f;
    mCoarseColliders.clear();
    for (Template& templateData : mTemplates) {
        if (templateData.vao) glDeleteVertexArrays(1, &templateData.vao);
        if (templateData.vbo) glDeleteBuffers(1, &templateData.vbo);
        if (templateData.instanceVbo) glDeleteBuffers(1, &templateData.instanceVbo);
    }
    mTemplates.clear();
}

namespace {

struct FaceKey {
    std::array<int64_t, 9> coords{};
    bool operator==(const FaceKey& other) const { return coords == other.coords; }
};

struct FaceKeyHash {
    size_t operator()(const FaceKey& key) const {
        size_t h = 1469598103934665603ull;
        for (int64_t value : key.coords) {
            h ^= static_cast<size_t>(value);
            h *= 1099511628211ull;
        }
        return h;
    }
};

int64_t faceCoord(float value)
{
    return static_cast<int64_t>(std::llround(value * 1000.0f));
}

void removeExactDuplicateFaces(std::vector<CollisionTriangle>& triangles,
                               std::vector<uint32_t>& solidIds,
                               StaticUnionStats* outStats)
{
    StaticUnionStats stats;
    stats.inputTriangles = static_cast<uint32_t>(triangles.size());
    if (triangles.size() != solidIds.size()) {
        stats.fallback = true;
        stats.fallbackReason = "solid_ids_size_mismatch";
        stats.outputTriangles = stats.inputTriangles;
        if (outStats) *outStats = stats;
        return;
    }

    std::unordered_map<FaceKey, size_t, FaceKeyHash> seen;
    std::vector<uint8_t> removed(triangles.size(), 0);
    seen.reserve(triangles.size());
    for (size_t i = 0; i < triangles.size(); ++i) {
        std::array<std::array<int64_t, 3>, 3> points{{
            {{faceCoord(triangles[i].a.x), faceCoord(triangles[i].a.y),
              faceCoord(triangles[i].a.z)}},
            {{faceCoord(triangles[i].b.x), faceCoord(triangles[i].b.y),
              faceCoord(triangles[i].b.z)}},
            {{faceCoord(triangles[i].c.x), faceCoord(triangles[i].c.y),
              faceCoord(triangles[i].c.z)}}}};
        std::sort(points.begin(), points.end());
        FaceKey key;
        for (int p = 0; p < 3; ++p)
            for (int axis = 0; axis < 3; ++axis)
                key.coords[p * 3 + axis] = points[p][axis];

        auto [it, inserted] = seen.emplace(key, i);
        if (inserted)
            continue;
        const size_t other = it->second;
        if (removed[other]) {
            it->second = i;
            continue;
        }
        if (solidIds[i] == 0 || solidIds[other] == 0 ||
            solidIds[i] == solidIds[other])
            continue;
        if (glm::dot(triangles[i].normal, triangles[other].normal) > -0.9f)
            continue;
        removed[i] = 1;
        ++stats.removedTriangles;
    }

    if (stats.removedTriangles == 0) {
        stats.outputTriangles = stats.inputTriangles;
        if (outStats) *outStats = stats;
        return;
    }

    size_t write = 0;
    for (size_t read = 0; read < triangles.size(); ++read) {
        if (removed[read])
            continue;
        if (write != read) {
            triangles[write] = triangles[read];
            solidIds[write] = solidIds[read];
        }
        ++write;
    }
    triangles.resize(write);
    solidIds.resize(write);
    stats.outputTriangles = static_cast<uint32_t>(write);
    if (outStats) *outStats = stats;
}

} // namespace

bool StreamedWorldLayer::flushCollision(World& world)
{
    if (!mCollisionDirty)
        return false;

    const auto started = std::chrono::steady_clock::now();
    CollisionMeshCache& mesh = world.collisionMesh;

    // Remove the previous streamed tail. A base rebuild clears the mesh (and
    // therefore these counts), so only a still-intact tail is trimmed here.
    size_t baseTriangles = mesh.triangles.size();
    size_t baseSolidIds = mesh.solidIds.size();
    if (mesh.streamedTailTriangles > 0 &&
        mesh.streamedTailTriangles <= baseTriangles) {
        baseTriangles -= mesh.streamedTailTriangles;
        mesh.triangles.resize(baseTriangles);
    }
    if (mesh.streamedTailSolidIds > 0 &&
        mesh.streamedTailSolidIds <= baseSolidIds) {
        baseSolidIds -= mesh.streamedTailSolidIds;
        mesh.solidIds.resize(baseSolidIds);
    }
    if (!mesh.hidden.empty() && mesh.hidden.size() > baseTriangles)
        mesh.hidden.resize(baseTriangles);
    if (mesh.triangleAABBs.size() > baseTriangles)
        mesh.triangleAABBs.resize(baseTriangles);
    if (mesh.triangleOBBs.size() > baseTriangles)
        mesh.triangleOBBs.resize(baseTriangles);

    const bool baseBroadphaseChanged =
        mBaseBroadphaseTriangles != baseTriangles ||
        world.collisionTree.ids.size() != baseTriangles;
    if (baseBroadphaseChanged) {
        buildCollisionChunks(world, nullptr);
        mBaseBroadphaseTriangles = baseTriangles;
    }

    std::vector<CollisionTriangle> surface;
    std::vector<uint32_t> surfaceSolidIds;
    size_t rawTriangles = 0;
    for (size_t recordIndex = 0; recordIndex < mChunks.size(); ++recordIndex) {
        const ChunkRecord& record = mChunks[recordIndex];
        if (!record.collisionActive)
            continue;
        const uint32_t solidBase = static_cast<uint32_t>(
            recordIndex * (mimita_chunk_collision::MAX_PARTS + 1));
        rawTriangles += record.collisionTriangles.size();
        for (size_t i = 0; i < record.collisionTriangles.size(); ++i) {
            surface.push_back(record.collisionTriangles[i]);
            const uint32_t local =
                i < record.collisionSolidIds.size()
                    ? record.collisionSolidIds[i] : 0u;
            surfaceSolidIds.push_back(local == 0 ? 0u : solidBase + local);
        }
    }

    mimita_collision_policy::SurfacePolicyV1 surfacePolicy{};
    surfacePolicy.version = mimita_collision_policy::ABI_VERSION;
    surfacePolicy.structSize = sizeof(surfacePolicy);
    surfacePolicy.mergeMode = mimita_collision_policy::SURFACE_FACE_REMOVAL;
    surfacePolicy.contactTolerance = 0.02f;
    if (!ModuleHost::instance().collisionSurfacePolicy(&surfacePolicy) ||
        !surfacePolicy.result)
        surfacePolicy.mergeMode = mimita_collision_policy::SURFACE_FACE_REMOVAL;

    StaticUnionStats unionStats;
    StaticUnionStats removalStats;
    const bool wantUnion =
        surfacePolicy.mergeMode == mimita_collision_policy::SURFACE_BOUNDED_UNION;
    const bool wantFaceRemoval =
        surfacePolicy.mergeMode != mimita_collision_policy::SURFACE_RAW;
    if (!surface.empty() && wantUnion &&
        CollisionLodConfig::instance().staticSolidUnion() &&
        surface.size() <=
            CollisionLodConfig::instance().staticSolidUnionMaxTriangles() &&
        mChunks.size() <=
            CollisionLodConfig::instance().staticSolidUnionMaxSolids()) {
        unionStaticSolidGroups(
            surface, surfaceSolidIds,
            CollisionLodConfig::instance().staticSolidUnionMaxTriangles(),
            &unionStats,
            CollisionLodConfig::instance().staticSolidUnionMaxSolids());
    }
    if (!surface.empty() && wantFaceRemoval &&
        CollisionLodConfig::instance().staticFaceRemoval()) {
        if (mChunks.size() <= 32)
            removeFullyCoveredStaticTriangles(surface, surfaceSolidIds,
                                              &removalStats);
        else
            removeExactDuplicateFaces(surface, surfaceSolidIds, &removalStats);
    }
    for (const AABB& collider : mCoarseColliders) {
        const size_t first = surface.size();
        appendAabbTriangles(surface, collider);
        surfaceSolidIds.resize(surface.size(), 0u);
        (void)first;
    }

    const size_t streamBase = mesh.triangles.size();
    mesh.triangles.insert(mesh.triangles.end(), surface.begin(), surface.end());
    mesh.solidIds.insert(mesh.solidIds.end(), surfaceSolidIds.begin(),
                         surfaceSolidIds.end());
    mesh.triangleAABBs.resize(mesh.triangles.size());
    mesh.triangleOBBs.resize(mesh.triangles.size());
    for (size_t i = 0; i < surface.size(); ++i) {
        AABB bounds;
        CollisionTriangleOBB obb;
        buildCollisionTrianglePrimitive(surface[i], bounds, obb);
        mesh.triangleAABBs[streamBase + i] = bounds;
        mesh.triangleOBBs[streamBase + i] = obb;
        mesh.boundsMin = glm::min(mesh.boundsMin, bounds.min);
        mesh.boundsMax = glm::max(mesh.boundsMax, bounds.max);
    }
    if (!mesh.hidden.empty()) {
        if (mesh.hidden.size() == baseTriangles)
            mesh.hidden.resize(mesh.triangles.size(), 0);
        else
            mesh.hidden.clear();
    }
    mesh.streamedTailTriangles = surface.size();
    mesh.streamedTailSolidIds = surfaceSolidIds.size();
    mesh.streamedChunkCount = static_cast<uint32_t>(mChunks.size());
    mesh.streamedRevision = ++mCollisionRevision;

    world.streamedCollisionTree.clear();
    if (!surface.empty()) {
        std::vector<int> streamIds(surface.size());
        for (size_t i = 0; i < streamIds.size(); ++i)
            streamIds[i] = static_cast<int>(streamBase + i);
        world.streamedCollisionTree.build(streamIds, mesh.triangleAABBs);
    }
    mCollisionDirty = false;
    mCollidedChunkCount = static_cast<uint32_t>(mChunks.size());
    mCollisionTriangleCount = surface.size();
    mCollisionRemovedFaces = removalStats.removedTriangles;

    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    mLastCollisionRebuildMs = elapsedMs;
    mMaxCollisionRebuildMs = std::max(mMaxCollisionRebuildMs, elapsedMs);
    mLastLoadedChunkCount = static_cast<uint32_t>(mChunks.size());
    mLastCollisionRevisionMatched =
        mLastLoadedChunkCount == mCollidedChunkCount;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world-stream.collision.sync", "streamed-world-layer",
        "streamed chunk collision composed into the world static surface", 0,
        {{"loaded_chunks", mChunks.size()},
         {"collided_chunks", mCollidedChunkCount},
         {"base_triangles", baseTriangles},
         {"raw_stream_triangles", rawTriangles},
         {"stream_triangles", surface.size()},
         {"world_triangles", mesh.triangles.size()},
         {"base_broadphase_rebuilt", baseBroadphaseChanged},
         {"stream_tree_triangles", world.streamedCollisionTree.ids.size()},
         {"base_tree_triangles", world.collisionTree.ids.size()},
         {"merge_mode", surfacePolicy.mergeMode},
         {"removed_internal_faces", removalStats.removedTriangles},
         {"union_solids", unionStats.unionSolids},
         {"union_fallback", unionStats.unionFallback},
         {"union_fallback_reason", unionStats.unionFallbackReason},
         {"revision", mCollisionRevision},
         {"revision_match", mChunks.size() == mCollidedChunkCount},
         {"rebuild_ms", elapsedMs}},
        __FILE__, __LINE__, __func__);
    return true;
}

std::string StreamedWorldLayer::templateKey(const std::string& chunkId,
                                            bool proxy)
{
    return proxy ? chunkId + "#proxy" : chunkId;
}

StreamedWorldLayer::Template* StreamedWorldLayer::ensureTemplate(
    const std::string& chunkId, bool proxy)
{
    if (Template* existing = findTemplate(chunkId, proxy)) return existing;
    Template templateData;
    templateData.chunkId = chunkId;
    templateData.proxy = proxy;
    mTemplates.push_back(std::move(templateData));
    return &mTemplates.back();
}

StreamedWorldLayer::Template* StreamedWorldLayer::findTemplate(
    const std::string& chunkId, bool proxy)
{
    for (Template& templateData : mTemplates)
        if (templateData.chunkId == chunkId &&
            templateData.proxy == proxy)
            return &templateData;
    return nullptr;
}

StreamedWorldLayer::Template* StreamedWorldLayer::findRecordTemplate(
    const ChunkRecord& record)
{
    return findTemplate(record.chunkId, record.proxy);
}

void StreamedWorldLayer::rebuildProxyTemplate(
    Template& templateData, const AuthoredChunk& chunk)
{
    templateData.verts.clear();
    templateData.batches.clear();
    ChunkPart part;
    part.id = "proxy";
    part.kind = "block";
    part.position = (chunk.boundsMin + chunk.boundsMax) * 0.5f;
    part.size = glm::max(chunk.boundsMax - chunk.boundsMin,
                         glm::vec3(0.05f));
    part.material = "default";
    appendBoxFaces(templateData.verts, part, templateData.batches,
                   partMatrix(part));
    templateData.vboDirty = true;
}

void StreamedWorldLayer::rebuildTemplate(Template& templateData,
                                         const std::string& chunkId)
{
    const AuthoredChunk* chunk = nullptr;
    for (const auto& candidate : MapConfigRegistry::instance().chunks())
        if (candidate.id == chunkId) { chunk = &candidate; break; }
    if (!chunk) return;
    if (templateData.proxy)
        rebuildProxyTemplate(templateData, *chunk);
    else
        rebuildTemplateFromChunk(templateData, *chunk);
}

void StreamedWorldLayer::rebuildTemplateFromChunk(
    Template& templateData, const AuthoredChunk& chunk)
{
    templateData.verts.clear();
    templateData.batches.clear();
    for (const ChunkPart& part : chunk.parts) {
        if (!part.visible)
            continue;
        appendBoxFaces(templateData.verts, part, templateData.batches,
                       partMatrix(part));
    }
    templateData.vboDirty = true;
}

void StreamedWorldLayer::uploadTemplate(Template& templateData)
{
    if (templateData.vao == 0) glGenVertexArrays(1, &templateData.vao);
    if (templateData.vbo == 0) glGenBuffers(1, &templateData.vbo);
    glBindVertexArray(templateData.vao);
    glBindBuffer(GL_ARRAY_BUFFER, templateData.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(templateData.verts.size() * sizeof(Vertex)),
                 templateData.verts.empty() ? nullptr : templateData.verts.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));

    // Instance matrix attributes (locations 9..12) with divisor 1.
    if (templateData.instanceVbo == 0) glGenBuffers(1, &templateData.instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, templateData.instanceVbo);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(9 + i);
        glVertexAttribPointer(9 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                              (void*)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(9 + i, 1);
    }
    glBindVertexArray(0);
    templateData.vboDirty = false;
}

void StreamedWorldLayer::uploadInstanceBuffer(Template& templateData)
{
    if (templateData.instanceVbo == 0) glGenBuffers(1, &templateData.instanceVbo);
    glBindBuffer(GL_ARRAY_BUFFER, templateData.instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(templateData.instances.size() * sizeof(glm::mat4)),
                 templateData.instances.empty() ? nullptr
                                                : templateData.instances.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void StreamedWorldLayer::rebuildRecordVbo(ChunkRecord& record,
                                          const Template& templateData)
{
    record.batches = templateData.batches;
    std::vector<Vertex> worldVerts;
    worldVerts.reserve(templateData.verts.size());
    for (const Vertex& v : templateData.verts) {
        Vertex copy = v;
        copy.pos = glm::vec3(record.transform * glm::vec4(v.pos, 1.0f));
        worldVerts.push_back(copy);
    }
    if (record.vbo == 0) glGenBuffers(1, &record.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, record.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(worldVerts.size() * sizeof(Vertex)),
                 worldVerts.empty() ? nullptr : worldVerts.data(),
                 GL_STATIC_DRAW);
    if (record.vao == 0) record.vao = makeChunkVao(record.vbo, 0);
}

void StreamedWorldLayer::rebuildInstances()
{
    for (Template& templateData : mTemplates)
        templateData.instances.clear();
    for (const ChunkRecord& record : mChunks) {
        Template* templateData = findRecordTemplate(record);
        if (!templateData) continue;
        templateData->instances.push_back(record.transform);
    }
    for (Template& templateData : mTemplates)
        uploadInstanceBuffer(templateData);
}

void StreamedWorldLayer::setInstancing(bool enabled)
{
    if (mInstancing == enabled) return;
    mInstancing = enabled;
    size_t rebuilt = 0;
    if (mInstancing) {
        for (ChunkRecord& record : mChunks) {
            if (record.vao) glDeleteVertexArrays(1, &record.vao);
            if (record.vbo) glDeleteBuffers(1, &record.vbo);
            record.vao = 0;
            record.vbo = 0;
            record.batches.clear();
        }
        for (Template& templateData : mTemplates) {
            if (templateData.vao == 0 || templateData.vboDirty)
                uploadTemplate(templateData);
        }
        rebuildInstances();
    } else {
        for (Template& templateData : mTemplates) {
            templateData.instances.clear();
            uploadInstanceBuffer(templateData);
        }
        for (ChunkRecord& record : mChunks) {
            Template* templateData = findRecordTemplate(record);
            if (!templateData) continue;
            if (templateData->vao == 0 || templateData->vboDirty)
                uploadTemplate(*templateData);
            rebuildRecordVbo(record, *templateData);
            ++rebuilt;
        }
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world-stream.backend.changed", enabled ? "instancing" : "vbo",
        "streamed chunk backend switched without reloading chunks", 0,
        {{"backend", enabled ? "instancing" : "vbo"},
         {"chunks", mChunks.size()},
         {"templates", mTemplates.size()},
         {"rebuilt", rebuilt}},
        __FILE__, __LINE__, __func__);
}

GLuint StreamedWorldLayer::makeChunkVao(GLuint vbo, GLuint /*instanceVbo*/)
{
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));
    glBindVertexArray(0);
    return vao;
}

void StreamedWorldLayer::addChunk(uint64_t key, const AuthoredChunk& chunk,
                                  const glm::mat4& transform)
{
    for (const ChunkRecord& existing : mChunks)
        if (existing.key == key) {
            removeChunk(key);
            break;
        }

    Template* templateData = ensureTemplate(chunk.id, false);
    if (templateData->verts.empty())
        rebuildTemplate(*templateData, chunk.id);
    if (templateData->vboDirty)
        uploadTemplate(*templateData);

    // Collision is kept with the chunk record and composed into the World
    // collision mesh tail by flushCollision. The live chunk-collision module
    // selects collidable parts and applies the chunk/part transforms; the host
    // owns cross-chunk face removal, bounded union, and the broadphase rebuild.
    ChunkRecord record;
    record.key = key;
    record.chunkId = chunk.id;
    record.transform = transform;
    record.proxy = false;
    record.bounds = transformedChunkBounds(chunk, transform);
    record.collisionActive =
        !mHasStreamFocus || mCollisionRadiusM <= 0.0f ||
        distanceToBounds(mStreamFocus, record.bounds) <= mCollisionRadiusM;
    buildChunkCollision(chunk, transform, record.collisionTriangles,
                        record.collisionSolidIds);

    if (mInstancing) {
        templateData->instances.push_back(transform);
        uploadInstanceBuffer(*templateData);
    } else {
        // Bake chunk-local geometry to world space and upload a per-chunk VBO.
        rebuildRecordVbo(record, *templateData);
    }

    const size_t collisionTriangles = record.collisionTriangles.size();
    mChunks.push_back(std::move(record));
    ++mChunksInstalledTotal;
    mCollisionDirty = true;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world-stream.chunk.applied", chunk.id,
        "streamed chunk installed", 0,
        {{"chunk", chunk.id}, {"key", key}, {"chunks", mChunks.size()},
         {"triangles", collisionTriangles},
         {"solid_groups", static_cast<uint32_t>(std::count_if(
              chunk.parts.begin(), chunk.parts.end(),
              [](const ChunkPart& part) { return part.collision; }))},
         {"collision_pending_flush", true},
         {"backend", mInstancing ? "instancing" : "vbo"}},
        __FILE__, __LINE__, __func__);
}

void StreamedWorldLayer::addProxy(uint64_t key, const AuthoredChunk& chunk,
                                  const glm::mat4& transform,
                                  const glm::vec3& boundsMin,
                                  const glm::vec3& boundsMax)
{
    for (const ChunkRecord& existing : mChunks)
        if (existing.key == key) {
            removeChunk(key);
            break;
        }

    Template* templateData = ensureTemplate(chunk.id, true);
    if (templateData->verts.empty())
        rebuildProxyTemplate(*templateData, chunk);
    if (templateData->vboDirty)
        uploadTemplate(*templateData);

    ChunkRecord record;
    record.key = key;
    record.chunkId = chunk.id;
    record.transform = transform;
    record.proxy = true;
    record.bounds = {boundsMin, boundsMax};
    record.collisionActive = false;

    if (mInstancing) {
        templateData->instances.push_back(transform);
        uploadInstanceBuffer(*templateData);
    } else {
        rebuildRecordVbo(record, *templateData);
    }

    mChunks.push_back(std::move(record));
    ++mChunksInstalledTotal;
    mCollisionDirty = true;
}

bool StreamedWorldLayer::isProxy(uint64_t key) const
{
    for (const ChunkRecord& record : mChunks)
        if (record.key == key)
            return record.proxy;
    return false;
}

void StreamedWorldLayer::setStreamFocus(const glm::vec3& focus,
                                        float collisionRadiusM)
{
    if (mHasStreamFocus && focus == mStreamFocus &&
        collisionRadiusM == mCollisionRadiusM)
        return;
    mHasStreamFocus = true;
    mStreamFocus = focus;
    mCollisionRadiusM = collisionRadiusM;
    for (ChunkRecord& record : mChunks) {
        const bool active = !record.proxy &&
            (collisionRadiusM <= 0.0f ||
             distanceToBounds(focus, record.bounds) <= collisionRadiusM);
        if (active != record.collisionActive) {
            record.collisionActive = active;
            mCollisionDirty = true;
        }
    }
}

void StreamedWorldLayer::setCoarseColliders(
    const std::vector<AABB>& colliders)
{
    if (mCoarseColliders.size() == colliders.size()) {
        bool same = true;
        for (size_t i = 0; i < colliders.size(); ++i) {
            if (glm::any(glm::notEqual(mCoarseColliders[i].min,
                                       colliders[i].min)) ||
                glm::any(glm::notEqual(mCoarseColliders[i].max,
                                       colliders[i].max))) {
                same = false;
                break;
            }
        }
        if (same)
            return;
    }
    mCoarseColliders = colliders;
    mCollisionDirty = true;
}

bool StreamedWorldLayer::hasFullCollisionNear(const glm::vec3& focus,
                                              float radius) const
{
    for (const ChunkRecord& record : mChunks) {
        if (record.proxy || record.collisionTriangles.empty())
            continue;
        if (radius <= 0.0f ||
            distanceToBounds(focus, record.bounds) <= radius)
            return true;
    }
    return false;
}

void StreamedWorldLayer::removeChunk(uint64_t key)
{
    for (size_t i = 0; i < mChunks.size(); ++i) {
        if (mChunks[i].key != key) continue;
        ChunkRecord record = std::move(mChunks[i]);
        mChunks.erase(mChunks.begin() + i);
        ++mChunksRemovedTotal;
        mCollisionDirty = true;

        if (!mInstancing) {
            if (record.vao) glDeleteVertexArrays(1, &record.vao);
            if (record.vbo) glDeleteBuffers(1, &record.vbo);
        } else {
            Template* templateData = findRecordTemplate(record);
            if (templateData) {
                auto found = std::find(templateData->instances.begin(),
                                       templateData->instances.end(),
                                       record.transform);
                if (found != templateData->instances.end())
                    templateData->instances.erase(found);
                uploadInstanceBuffer(*templateData);
            }
        }
        return;
    }
}

void StreamedWorldLayer::refreshChunk(const AuthoredChunk& chunk,
                                      bool rebuildCollision)
{
    Template* templateData = findTemplate(chunk.id, false);
    if (!templateData) return;
    rebuildTemplateFromChunk(*templateData, chunk);
    uploadTemplate(*templateData);

    size_t records = 0;
    size_t collisionTriangles = 0;
    for (ChunkRecord& record : mChunks) {
        if (record.chunkId != chunk.id) continue;
        if (!mInstancing)
            rebuildRecordVbo(record, *templateData);
        ++records;
        if (!rebuildCollision)
            continue;
        buildChunkCollision(chunk, record.transform, record.collisionTriangles,
                            record.collisionSolidIds);
        collisionTriangles += record.collisionTriangles.size();
        mCollisionDirty = true;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "world-stream.template.updated", chunk.id,
        rebuildCollision ? "loaded chunk template and collision refreshed"
                         : "loaded chunk template refreshed without collision rebuild",
        0,
        {{"chunk", chunk.id},
         {"verts", templateData->verts.size()},
         {"batches", templateData->batches.size()},
         {"records", records},
         {"backend", mInstancing ? "instancing" : "vbo"},
         {"collision_rebuilt", rebuildCollision},
         {"collision_triangles", collisionTriangles},
         {"collision_pending_flush", mCollisionDirty}},
        __FILE__, __LINE__, __func__);
}

void StreamedWorldLayer::updateMaterial(const std::string& chunkId,
                                        const std::string& partId, int face,
                                        const std::string& material)
{
    AuthoredChunk* changed = nullptr;
    for (auto& chunk : MapConfigRegistry::instance().chunksMutable()) {
        if (chunk.id != chunkId) continue;
        for (auto& part : chunk.parts) {
            if (part.id != partId) continue;
            if (face < 0)
                part.material = material;
            else if (face < 6)
                part.faceMaterials[face] = material;
        }
        changed = &chunk;
        MapConfigRegistry::instance().markChanged();
        break;
    }
    if (changed) refreshChunk(*changed, false);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "chunk.part.material-changed", chunkId,
        "chunk part material changed live", 0,
        {{"chunk", chunkId}, {"part", partId}, {"face", face},
         {"material", material}, {"found", changed != nullptr}},
        __FILE__, __LINE__, __func__);
}

void StreamedWorldLayer::render(const Camera& camera)
{
    if (!gRenderer || !gRenderer->shaderProgram || mChunks.empty())
        return;

    glUseProgram(gRenderer->shaderProgram);
    const glm::mat4 view = camera.getView();
    const glm::mat4 proj = camera.getProj((float)gRenderer->sceneWidth,
                                          (float)gRenderer->sceneHeight);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "view"),
                       1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "projection"),
                       1, GL_FALSE, &proj[0][0]);
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUsePartModels"), 0);
    setUniforms(gRenderer->shaderProgram, camera.pos);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glActiveTexture(GL_TEXTURE0);

    if (mInstancing) {
        glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseInstanceModel"), 1);
        const glm::mat4 identity(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"),
                           1, GL_FALSE, &identity[0][0]);
        for (Template& templateData : mTemplates) {
            if (templateData.instances.empty()) continue;
            glBindVertexArray(templateData.vao);
            for (const Mesh::Batch& batch : templateData.batches) {
                glBindTexture(GL_TEXTURE_2D, batch.texture);
                glDrawArraysInstanced(GL_TRIANGLES, (GLint)batch.first,
                                      (GLsizei)batch.count,
                                      (GLsizei)templateData.instances.size());
            }
        }
    } else {
        glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseInstanceModel"), 0);
        const glm::mat4 identity(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"),
                           1, GL_FALSE, &identity[0][0]);
        for (const ChunkRecord& record : mChunks) {
            if (record.vao == 0) continue;
            glBindVertexArray(record.vao);
            for (const Mesh::Batch& batch : record.batches) {
                glBindTexture(GL_TEXTURE_2D, batch.texture);
                glDrawArrays(GL_TRIANGLES, (GLint)batch.first,
                             (GLsizei)batch.count);
            }
        }
    }
    // Shared-shader state: leaving uUseInstanceModel enabled corrupts the next
    // frame's base-world and actor draws because their VAOs have no instance
    // matrix attributes.
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram,
                                     "uUseInstanceModel"), 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

// 2026-10-23
#include "map/map-authored-geometry.h"
#include "map/map-glb-authored.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <nlohmann/json.hpp>

#include "procedural/procedural-world.h"

#include "debug/structured-log.h"
#include "config/collision-lod-config.h"
#include "gamemode/map-config.h"
#include "hot-reload/module-host.h"
#include "modules/api/worldgen-api.h"
#include "worldgen/worldgen-library.h"
#include "map/map-loader-collision.h"
#include "map/map-static-union.h"
#include "network/server.h"
#include "physics/physical-entity.h"
#include "render/world-box-mesh.h"
#include "world/texture-store.h"
#include "world/world.h"

namespace {

uint64_t serverCellHash(uint32_t seed, const std::string& version,
                        const std::string& deps, int x, int y, int z)
{
    uint64_t h = MimitaProcedural::proceduralZoneChunkHash(seed, version, deps,
                                                           x, y);
    h ^= (uint64_t(uint32_t(z)) * 0xC2B2AE3D27D4EB4Full) + 0x9E3779B97F4A7C15ull;
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
    return h ^ (h >> 27);
}

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

    std::array<std::string, 6> faceMaterial;
    bool uniformMaterial = true;
    for (int face = 0; face < 6; ++face) {
        faceMaterial[face] = block.faceMaterials[face].empty()
            ? block.material
            : block.faceMaterials[face];
        if (faceMaterial[face] != faceMaterial[0])
            uniformMaterial = false;
    }

    mimita_world_rendering::BoxMeshV1 request{};
    const float* transformValues = &transform[0][0];
    for (int i = 0; i < 16; ++i)
        request.transform[i] = transformValues[i];
    request.size[0] = block.size.x;
    request.size[1] = block.size.y;
    request.size[2] = block.size.z;
    request.uvScale[0] = 1.0f;
    request.uvScale[1] = 1.0f;
    for (int face = 0; face < 6; ++face) {
        std::snprintf(request.materials[face], sizeof(request.materials[face]),
                      "%s", faceMaterial[face].c_str());
    }
    mimita_world_rendering::BoxMeshV1 result{};
    if (!buildBoxMesh(request, result))
        return;

    struct FaceRange {
        int index = 0;
        std::string material;
        glm::vec3 normal{0.0f};
        size_t first = 0;
        size_t count = 0;
    };
    std::vector<FaceRange> faceRanges;

    const size_t objectFirst = world.mesh.verts.size();
    glm::vec3 objectBoundsMin(std::numeric_limits<float>::max());
    glm::vec3 objectBoundsMax(std::numeric_limits<float>::lowest());

    auto makeBatch = [&](const std::string& material, size_t first) {
        Mesh::Batch batch;
        batch.materialName = material.empty() ? "default" : material;
        batch.texture = resolveBlockTexture(material);
        batch.first = first;
        batch.count = 0;
        batch.boundsMin = glm::vec3(std::numeric_limits<float>::max());
        batch.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
        batch.hasBounds = true;
        return batch;
    };

    Mesh::Batch uniformBatch;
    if (uniformMaterial)
        uniformBatch = makeBatch(faceMaterial[0], world.mesh.verts.size());

    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int face = 0; face < 6; ++face) {
        const mimita_world_rendering::MeshFaceV1& faceMesh = result.faces[face];
        Mesh::Batch batch = uniformMaterial
            ? Mesh::Batch{}
            : makeBatch(faceMaterial[face], world.mesh.verts.size());
        FaceRange range;
        range.index = face;
        range.material = faceMaterial[face];
        range.normal = glm::vec3(faceMesh.normal[0], faceMesh.normal[1],
                                 faceMesh.normal[2]);
        range.first = world.mesh.verts.size();
        for (int i : order) {
            Vertex v;
            v.pos = glm::vec3(faceMesh.positions[i][0],
                              faceMesh.positions[i][1],
                              faceMesh.positions[i][2]);
            v.normal = range.normal;
            v.uv = glm::vec2(faceMesh.uv[i][0], faceMesh.uv[i][1]);
            world.mesh.verts.push_back(v);
            objectBoundsMin = glm::min(objectBoundsMin, v.pos);
            objectBoundsMax = glm::max(objectBoundsMax, v.pos);
        }
        range.count = world.mesh.verts.size() - range.first;
        faceRanges.push_back(range);
        if (!uniformMaterial) {
            batch.count = world.mesh.verts.size() - batch.first;
            for (size_t i = batch.first; i < world.mesh.verts.size(); ++i) {
                batch.boundsMin = glm::min(batch.boundsMin, world.mesh.verts[i].pos);
                batch.boundsMax = glm::max(batch.boundsMax, world.mesh.verts[i].pos);
            }
            world.mesh.batches.push_back(std::move(batch));
        }
    }
    if (uniformMaterial) {
        uniformBatch.count = world.mesh.verts.size() - uniformBatch.first;
        for (size_t i = uniformBatch.first; i < world.mesh.verts.size(); ++i) {
            uniformBatch.boundsMin = glm::min(uniformBatch.boundsMin,
                                              world.mesh.verts[i].pos);
            uniformBatch.boundsMax = glm::max(uniformBatch.boundsMax,
                                              world.mesh.verts[i].pos);
        }
        world.mesh.batches.push_back(std::move(uniformBatch));
    }

    nlohmann::json faces = nlohmann::json::array();
    for (const FaceRange& range : faceRanges) {
        faces.push_back({
            {"index", range.index},
            {"normal", {range.normal.x, range.normal.y, range.normal.z}},
            {"winding", "ccw"},
            {"material", range.material.empty() ? "default" : range.material},
            {"first", range.first},
            {"count", range.count}
        });
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Rendering, StructuredLevel::Important,
        "world-render.face-template", "block:" + block.id,
        "authored box face template generated", 0,
        nlohmann::json{{"object", "block:" + block.id},
                       {"block", block.id},
                       {"uniform_material", uniformMaterial},
                       {"face_count", faceRanges.size()},
                       {"vertex_count", world.mesh.verts.size() - objectFirst},
                       {"batch_count", uniformMaterial ? 1 : 6},
                       {"faces", faces},
                       {"render_revision", world.renderRevision}},
        __FILE__, __LINE__, __FUNCTION__);

    MapGLBObject object;
    object.id = "block:" + block.id;
    object.name = block.id;
    object.transform = transform;
    object.vertexFirst = objectFirst;
    object.vertexCount = world.mesh.verts.size() - objectFirst;
    object.boundsMin = objectBoundsMin;
    object.boundsMax = objectBoundsMax;
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

void hashBytes(uint64_t& hash, const void* data, size_t size)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= (uint64_t)bytes[i];
        hash *= 1099511628211ull;
    }
}

template <typename T>
void hashPod(uint64_t& hash, const T& value)
{
    hashBytes(hash, &value, sizeof(T));
}

void hashString(uint64_t& hash, const std::string& value)
{
    const uint64_t size = (uint64_t)value.size();
    hashPod(hash, size);
    hashBytes(hash, value.data(), value.size());
}

void hashVec3(uint64_t& hash, const glm::vec3& value)
{
    hashPod(hash, value.x);
    hashPod(hash, value.y);
    hashPod(hash, value.z);
}

} // namespace

uint64_t authoredGeometrySignature(const MapObjectiveConfig& config)
{
    uint64_t hash = 1469598103934665603ull;
    hashPod(hash, config.generateWholeWorld);

    for (const AuthoredBlock& block : config.blocks) {
        hashString(hash, block.id);
        hashString(hash, block.shape);
        hashVec3(hash, block.position);
        hashVec3(hash, block.size);
        hashVec3(hash, block.rotationEuler);
        hashString(hash, block.material);
        for (const std::string& face : block.faceMaterials)
            hashString(hash, face);
        hashPod(hash, block.enabled);
    }

    for (const AuthoredChunk& chunk : config.chunks) {
        hashString(hash, chunk.id);
        hashString(hash, chunk.geometryPath);
        hashString(hash, chunk.blobSha256);
        hashVec3(hash, chunk.boundsMin);
        hashVec3(hash, chunk.boundsMax);
        hashPod(hash, chunk.enabled);
        for (const ChunkPart& part : chunk.parts) {
            hashString(hash, part.id);
            hashString(hash, part.kind);
            hashVec3(hash, part.position);
            hashVec3(hash, part.size);
            hashVec3(hash, part.rotationEuler);
            hashString(hash, part.material);
            for (const std::string& face : part.faceMaterials)
                hashString(hash, face);
            hashPod(hash, part.uvScale.x);
            hashPod(hash, part.uvScale.y);
            for (const glm::vec2& faceScale : part.faceUvScale) {
                hashPod(hash, faceScale.x);
                hashPod(hash, faceScale.y);
            }
            hashPod(hash, part.collision);
            hashPod(hash, part.visible);
        }
        for (const AuthoredChunkConnector& connector : chunk.connectors) {
            hashString(hash, connector.id);
            hashString(hash, connector.side);
            hashString(hash, connector.orientation);
            hashString(hash, connector.role);
            hashVec3(hash, connector.position);
            hashPod(hash, connector.radius);
            for (const std::string& link : connector.links)
                hashString(hash, link);
        }
        for (const ChunkSpawnPoint& spawn : chunk.spawnPoints) {
            hashString(hash, spawn.id);
            hashVec3(hash, spawn.position);
            hashPod(hash, spawn.radius);
            hashString(hash, spawn.allowedActors);
            hashPod(hash, spawn.enabled);
        }
    }

    for (const GenerationZone& zone : config.generationZones) {
        hashString(hash, zone.id);
        hashVec3(hash, zone.boundsMin);
        hashVec3(hash, zone.boundsMax);
        hashPod(hash, zone.seed);
        hashString(hash, zone.generatorVersion);
        hashString(hash, zone.ruleSet);
        for (const std::string& dependency : zone.chunkDependencies)
            hashString(hash, dependency);
        hashString(hash, zone.modeId);
        hashString(hash, zone.plane);
        hashPod(hash, zone.repeatAxes);
        hashVec3(hash, zone.origin);
        hashVec3(hash, zone.cellSize);
        hashVec3(hash, zone.chunkSizeVec);
        hashPod(hash, zone.streamRadius);
        hashString(hash, zone.spacingMode);
        hashPod(hash, zone.connectorGap);
        hashPod(hash, zone.maxBranchDepth);
        hashPod(hash, zone.allowCycles);
        hashPod(hash, zone.allowCrossings);
        hashPod(hash, zone.chunkSize);
        hashPod(hash, zone.streamRadiusChunks);
        hashPod(hash, zone.enabled);
    }

    for (const MapGLBObjectOverride& object : config.glbObjects) {
        hashString(hash, object.id);
        hashString(hash, object.name);
        hashString(hash, object.instanceId);
        hashString(hash, object.source);
        hashVec3(hash, object.position);
        hashVec3(hash, object.rotation);
        hashVec3(hash, object.scale);
        hashString(hash, object.material);
        hashPod(hash, object.visible);
        hashPod(hash, object.collision);
        hashPod(hash, object.deleted);
        hashPod(hash, object.clone);
    }

    for (const MapEntity& entity : config.entities) {
        if (entity.type != "spawnpoint") continue;
        hashString(hash, entity.id);
        hashVec3(hash, entity.position);
        hashPod(hash, entity.yaw);
        hashString(hash, entity.tag);
        hashPod(hash, entity.enabled);
    }

    return hash ? hash : 1ull;
}

uint64_t authoredGeometrySignatureLive(const MapObjectiveConfig& config)
{
    const MapObjectiveConfig& current =
        MapConfigRegistry::instance().current();
    if (&config == &current) {
        static thread_local std::uint64_t cachedRevision = ~0ull;
        static thread_local std::uint32_t cachedGeneration = ~0u;
        static thread_local std::uint64_t cachedSignature = 0;
        const std::uint64_t revision = MapConfigRegistry::instance().revision();
        const std::uint32_t generation =
            ModuleHost::instance().activeGeneration("map-geometry");
        if (revision == cachedRevision && generation == cachedGeneration &&
            cachedSignature != 0)
            return cachedSignature;
        const std::string bytes =
            MapConfigRegistry::instance().toJsonString(false);
        std::uint64_t signature = 0;
        if (ModuleHost::instance().mapGeometrySignature(
                bytes.data(), static_cast<std::uint32_t>(bytes.size()),
                &signature)) {
            cachedRevision = revision;
            cachedGeneration = generation;
            cachedSignature = signature;
            return signature;
        }
    }
    return authoredGeometrySignature(config);
}

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
        glm::vec3 rotation(0.0f);
        glm::vec3 scale(1.0f);
        GLBObjectAuthoredV2& state = MapGLBAuthoredState::instance().ensure(
            &world.mesh, object.id, object.transform, object.basePosition);
        if (state.basePositions.size() != object.vertexCount) {
            state.basePositions.clear();
            state.baseNormals.clear();
            state.basePositions.reserve(object.vertexCount);
            state.baseNormals.reserve(object.vertexCount);
            const size_t firstBase = std::min(object.vertexFirst,
                                              world.mesh.verts.size());
            const size_t lastBase = std::min(
                object.vertexFirst + object.vertexCount,
                world.mesh.verts.size());
            for (size_t i = firstBase; i < lastBase; ++i) {
                state.basePositions.push_back(world.mesh.verts[i].pos);
                state.baseNormals.push_back(world.mesh.verts[i].normal);
            }
        }
        const MapGLBObjectOverride* matched = nullptr;
        for (const MapGLBObjectOverride& override : overrides) {
            if (override.id == object.id) {
                matched = &override;
                target = override.position;
                rotation = override.rotation;
                scale = glm::max(override.scale, glm::vec3(0.0001f));
                if (!override.material.empty())
                    object.materialName = override.material;
                break;
            }
        }
        state.position = target;
        state.rotation = rotation;
        state.scale = scale;
        state.material = matched ? matched->material : std::string{};
        state.visible = matched ? matched->visible : true;
        state.collision = matched ? matched->collision : true;
        state.deleted = matched ? matched->deleted : false;
        state.clone = matched ? matched->clone : false;
        state.authoredHidden = state.deleted || !state.visible;
        state.authoredCollisionHidden = state.deleted || !state.collision;
        const size_t first = std::min(object.vertexFirst, world.mesh.verts.size());
        const size_t last = std::min(object.vertexFirst + object.vertexCount,
                                     world.mesh.verts.size());
        const bool rangeValid =
            object.vertexFirst < world.mesh.verts.size() &&
            object.vertexFirst + object.vertexCount <= world.mesh.verts.size();
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), target);
        transform = glm::rotate(transform, glm::radians(rotation.z),
                                glm::vec3(0, 0, 1));
        transform = glm::rotate(transform, glm::radians(rotation.y),
                                glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(rotation.x),
                                glm::vec3(1, 0, 0));
        transform = glm::scale(transform, scale);
        transform = transform * glm::translate(glm::mat4(1.0f),
                                               -object.basePosition);
        object.transform = transform * state.baseTransform;
        object.boundsMin = glm::vec3(std::numeric_limits<float>::max());
        object.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
        for (size_t i = first; i < last; ++i) {
            const size_t local = i - object.vertexFirst;
            const glm::vec3 base =
                local < state.basePositions.size()
                    ? state.basePositions[local]
                    : world.mesh.verts[i].pos;
            const glm::vec3 normal =
                local < state.baseNormals.size()
                    ? state.baseNormals[local]
                    : world.mesh.verts[i].normal;
            world.mesh.verts[i].pos =
                glm::vec3(transform * glm::vec4(base, 1.0f));
            const glm::mat3 normalMatrix(transform);
            const glm::vec3 transformed = normalMatrix * normal;
            if (glm::dot(transformed, transformed) > 0.000001f)
                world.mesh.verts[i].normal = glm::normalize(transformed);
            object.boundsMin = glm::min(object.boundsMin,
                                        world.mesh.verts[i].pos);
            object.boundsMax = glm::max(object.boundsMax,
                                        world.mesh.verts[i].pos);
        }
        if (!rangeValid) {
            object.boundsMin = object.basePosition;
            object.boundsMax = object.basePosition;
        }
        if (matched) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "map.glb-object.applied", object.id,
                rangeValid ? "client GLB override applied" :
                             "client GLB override range invalid",
                0,
                nlohmann::json{{"side", "client"},
                               {"object", object.id},
                               {"name", object.name},
                               {"base_position", {object.basePosition.x,
                                                  object.basePosition.y,
                                                  object.basePosition.z}},
                               {"target_position", {target.x, target.y, target.z}},
                               {"rotation", {rotation.x, rotation.y, rotation.z}},
                               {"scale", {scale.x, scale.y, scale.z}},
                               {"deleted", matched->deleted},
                               {"visible", matched->visible},
                               {"vertex_first", object.vertexFirst},
                               {"vertex_count", object.vertexCount},
                               {"range_valid", rangeValid},
                               {"vertices_moved", rangeValid ? last - first : 0},
                               {"material", object.materialName}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
}

void applyGLBObjectClones(World& world,
                          const std::vector<MapGLBObjectOverride>& overrides)
{
    for (const MapGLBObjectOverride& override : overrides) {
        if (!override.clone || override.deleted)
            continue;
        const std::string sourceId =
            !override.source.empty() ? override.source : override.id;
        MapGLBObject* source = nullptr;
        for (MapGLBObject& object : world.mesh.objects)
            if (object.id == sourceId) { source = &object; break; }
        if (!source)
            for (MapGLBObject& object : world.authoredObjects)
                if (object.id == sourceId) { source = &object; break; }
        if (!source || source->vertexCount == 0)
            continue;
        const GLBObjectAuthoredV2* sourceState =
            MapGLBAuthoredState::instance().find(&world.mesh, source->id);
        const std::vector<glm::vec3>* basePositions =
            sourceState && !sourceState->basePositions.empty()
                ? &sourceState->basePositions : nullptr;
        const std::vector<glm::vec3>* baseNormals =
            sourceState && !sourceState->baseNormals.empty()
                ? &sourceState->baseNormals : nullptr;
        const size_t first = world.mesh.verts.size();
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), override.position);
        transform = glm::rotate(transform, glm::radians(override.rotation.z),
                                glm::vec3(0, 0, 1));
        transform = glm::rotate(transform, glm::radians(override.rotation.y),
                                glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(override.rotation.x),
                                glm::vec3(1, 0, 0));
        transform = glm::scale(transform,
                               glm::max(override.scale, glm::vec3(0.0001f)));
        transform = transform * glm::translate(glm::mat4(1.0f),
                                               -source->basePosition);
        MapGLBObject clone;
        clone.id = override.instanceId.empty() ? override.id : override.instanceId;
        clone.name = override.name.empty() ? source->name + "_clone" : override.name;
        clone.nodeIndex = -1;
        clone.basePosition = override.position;
        clone.transform = transform;
        clone.materialName = !override.material.empty()
            ? override.material : source->materialName;
        clone.vertexFirst = first;
        clone.vertexCount = source->vertexCount;
        clone.boundsMin = glm::vec3(std::numeric_limits<float>::max());
        clone.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
        for (size_t i = 0; i < source->vertexCount; ++i) {
            const glm::vec3 base = basePositions && i < basePositions->size()
                ? (*basePositions)[i]
                : world.mesh.verts[source->vertexFirst + i].pos;
            const glm::vec3 normal = baseNormals && i < baseNormals->size()
                ? (*baseNormals)[i]
                : world.mesh.verts[source->vertexFirst + i].normal;
            Vertex vertex = world.mesh.verts[source->vertexFirst + i];
            vertex.pos = glm::vec3(transform * glm::vec4(base, 1.0f));
            const glm::vec3 transformed = glm::mat3(transform) * normal;
            if (glm::dot(transformed, transformed) > 0.000001f)
                vertex.normal = glm::normalize(transformed);
            world.mesh.verts.push_back(vertex);
            clone.boundsMin = glm::min(clone.boundsMin, vertex.pos);
            clone.boundsMax = glm::max(clone.boundsMax, vertex.pos);
        }
        Mesh::Batch batch;
        batch.materialName = clone.materialName.empty()
            ? "default" : clone.materialName;
        batch.first = first;
        batch.count = clone.vertexCount;
        for (const Mesh::Batch& sourceBatch : world.mesh.batches)
            if (sourceBatch.first <= source->vertexFirst &&
                sourceBatch.first + sourceBatch.count > source->vertexFirst) {
                batch.texture = sourceBatch.texture;
                if (clone.materialName.empty())
                    batch.materialName = sourceBatch.materialName;
                break;
            }
        world.mesh.batches.push_back(batch);
        GLBObjectAuthoredV2& state = MapGLBAuthoredState::instance().ensure(
            &world.mesh, clone.id, clone.transform, clone.basePosition);
        state.position = override.position;
        state.rotation = override.rotation;
        state.scale = override.scale;
        state.material = clone.materialName;
        state.visible = override.visible;
        state.collision = override.collision;
        state.deleted = override.deleted;
        state.clone = true;
        state.authoredHidden = state.deleted || !state.visible;
        state.authoredCollisionHidden = state.deleted || !state.collision;
        const std::string cloneId = clone.id;
        world.authoredObjects.push_back(std::move(clone));
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map.glb-object.clone.created", sourceId,
            "client GLB source node cloned into authored geometry", 0,
            nlohmann::json{{"source", sourceId},
                           {"clone", cloneId},
                           {"vertex_first", first},
                           {"vertex_count", world.authoredObjects.back().vertexCount}},
            __FILE__, __LINE__, __FUNCTION__);
    }
}

void applyGLBObjectClones(MimitaNet::HeadlessWorld& world,
                          const std::vector<MapGLBObjectOverride>& overrides)
{
    for (const MapGLBObjectOverride& override : overrides) {
        if (!override.clone || override.deleted)
            continue;
        const std::string sourceId =
            !override.source.empty() ? override.source : override.id;
        const MimitaNet::HeadlessGLBObject* source = nullptr;
        for (const auto& object : world.rawGlbObjects)
            if (object.id == sourceId) { source = &object; break; }
        if (!source)
            for (const auto& object : world.glbObjects)
                if (object.id == sourceId) { source = &object; break; }
        if (!source || source->triangleCount == 0)
            continue;
        const size_t first = std::min(source->triangleFirst,
                                      world.rawTriangles.size());
        const size_t last = std::min(source->triangleFirst + source->triangleCount,
                                     world.rawTriangles.size());
        if (first >= last)
            continue;
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), override.position);
        transform = glm::rotate(transform, glm::radians(override.rotation.z),
                                glm::vec3(0, 0, 1));
        transform = glm::rotate(transform, glm::radians(override.rotation.y),
                                glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(override.rotation.x),
                                glm::vec3(1, 0, 0));
        transform = glm::scale(transform,
                               glm::max(override.scale, glm::vec3(0.0001f)));
        transform = transform * glm::translate(glm::mat4(1.0f),
                                               -source->basePosition);
        MimitaNet::HeadlessGLBObject clone;
        clone.id = override.instanceId.empty() ? override.id : override.instanceId;
        clone.name = override.name.empty() ? source->name + "_clone" : override.name;
        clone.nodeIndex = -1;
        clone.basePosition = override.position;
        clone.position = override.position;
        clone.materialName = override.material;
        clone.triangleFirst = world.triangles.size();
        clone.triangleCount = static_cast<size_t>(last - first);
        clone.solidId = world.nextSolidId++;
        clone.boundsMin = glm::vec3(std::numeric_limits<float>::max());
        clone.boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
        for (size_t i = first; i < last; ++i) {
            CollisionTriangle triangle = world.rawTriangles[i];
            triangle.a = glm::vec3(transform * glm::vec4(triangle.a, 1.0f));
            triangle.b = glm::vec3(transform * glm::vec4(triangle.b, 1.0f));
            triangle.c = glm::vec3(transform * glm::vec4(triangle.c, 1.0f));
            world.triangles.push_back(triangle);
            world.solidIds.push_back(
                (override.deleted || !override.collision)
                    ? kRemovedCollisionSolidId : clone.solidId);
            clone.boundsMin = glm::min(clone.boundsMin,
                glm::min(triangle.a, glm::min(triangle.b, triangle.c)));
            clone.boundsMax = glm::max(clone.boundsMax,
                glm::max(triangle.a, glm::max(triangle.b, triangle.c)));
        }
        world.glbObjects.push_back(clone);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map.glb-object.clone.created", sourceId,
            "server GLB source node cloned into headless collision", 0,
            nlohmann::json{{"source", sourceId},
                           {"clone", clone.id},
                           {"triangle_first", clone.triangleFirst},
                           {"triangle_count", clone.triangleCount},
                           {"solid_id", clone.solidId}},
            __FILE__, __LINE__, __FUNCTION__);
    }
}

void applyGLBObjectOverrides(MimitaNet::HeadlessWorld& world,
                             const std::vector<MapGLBObjectOverride>& overrides)
{
    for (MimitaNet::HeadlessGLBObject& object : world.glbObjects) {
        glm::vec3 target = object.basePosition;
        glm::vec3 rotation(0.0f);
        glm::vec3 scale(1.0f);
        const MapGLBObjectOverride* matched = nullptr;
        for (const MapGLBObjectOverride& override : overrides) {
            if (override.id == object.id) {
                matched = &override;
                target = override.position;
                rotation = override.rotation;
                scale = glm::max(override.scale, glm::vec3(0.0001f));
                if (!override.material.empty())
                    object.materialName = override.material;
                break;
            }
        }
        const size_t first = std::min(object.triangleFirst, world.triangles.size());
        const size_t last = std::min(object.triangleFirst + object.triangleCount,
                                     world.triangles.size());
        const bool rangeValid =
            object.triangleFirst < world.triangles.size() &&
            object.triangleFirst + object.triangleCount <= world.triangles.size();
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), target);
        transform = glm::rotate(transform, glm::radians(rotation.z),
                                glm::vec3(0, 0, 1));
        transform = glm::rotate(transform, glm::radians(rotation.y),
                                glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(rotation.x),
                                glm::vec3(1, 0, 0));
        transform = glm::scale(transform, scale);
        transform = transform * glm::translate(glm::mat4(1.0f),
                                               -object.basePosition);
        for (size_t i = first; i < last; ++i) {
            CollisionTriangle& triangle = world.triangles[i];
            triangle.a = glm::vec3(transform * glm::vec4(triangle.a, 1.0f));
            triangle.b = glm::vec3(transform * glm::vec4(triangle.b, 1.0f));
            triangle.c = glm::vec3(transform * glm::vec4(triangle.c, 1.0f));
            if (matched && (matched->deleted || !matched->collision) &&
                i < world.solidIds.size())
                world.solidIds[i] = kRemovedCollisionSolidId;
        }
        object.position = target;
        if (matched) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "map.glb-object.applied", object.id,
                rangeValid ? "server GLB override applied" :
                             "server GLB override range invalid",
                0,
                nlohmann::json{{"side", "server"},
                               {"object", object.id},
                               {"name", object.name},
                               {"base_position", {object.basePosition.x,
                                                  object.basePosition.y,
                                                  object.basePosition.z}},
                               {"target_position", {target.x, target.y, target.z}},
                               {"rotation", {rotation.x, rotation.y, rotation.z}},
                               {"scale", {scale.x, scale.y, scale.z}},
                               {"deleted", matched->deleted},
                               {"collision", matched->collision},
                               {"triangle_first", object.triangleFirst},
                               {"triangle_count", object.triangleCount},
                               {"range_valid", rangeValid},
                               {"triangles_moved", rangeValid ? last - first : 0},
                               {"material", object.materialName},
                               {"solid_id", object.solidId}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
    for (const MapGLBObjectOverride& override : overrides) {
        const bool found = std::any_of(
            world.glbObjects.begin(), world.glbObjects.end(),
            [&override](const MimitaNet::HeadlessGLBObject& object) {
                return object.id == override.id;
            });
        if (!found) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Errors,
                "map.glb-object.override-unmatched", override.id,
                "server GLB override does not match any raw GLB node",
                0,
                nlohmann::json{{"side", "server"},
                               {"object", override.id},
                               {"name", override.name},
                               {"target_position", {override.position.x,
                                                    override.position.y,
                                                    override.position.z}}},
                __FILE__, __LINE__, __FUNCTION__);
        }
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
    applyGLBObjectClones(world, config.glbObjects);

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
                       {"glb_override_count", config.glbObjects.size()},
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

bool applyAuthoredMapGeometry(MimitaNet::HeadlessWorld& world,
                              const std::string& mapId,
                              bool allowStreamedFootprint)
{
    if (!ensureMapLoaded(mapId)) return false;
    const MapObjectiveConfig& config = MapConfigRegistry::instance().current();
    if (world.rawBaseValid) {
        world.triangles = world.rawTriangles;
        world.solidIds = world.rawSolidIds;
        world.glbObjects = world.rawGlbObjects;
        world.spawnPoints = world.rawSpawnPoints;
        world.boundsMin = world.rawBoundsMin;
        world.boundsMax = world.rawBoundsMax;
        world.nextSolidId = world.rawNextSolidId;
    }
    world.connectedFlatGround = false;
    for (const GenerationZone& zone : config.generationZones) {
        if (!allowStreamedFootprint || !config.generateWholeWorld ||
            !zone.enabled || zone.modeId != "connected_world" ||
            zone.repeatAxes.x == 0 || zone.repeatAxes.y == 0 ||
            zone.repeatAxes.z != 0) continue;
        bool valid = !config.chunks.empty();
        bool foundFloor = false;
        float floorZ = 0.0f;
        for (const AuthoredChunk& chunk : config.chunks) {
            if (!chunk.enabled) continue;
            for (const ChunkPart& part : chunk.parts) {
                if (!part.collision) continue;
                const float top = zone.origin.z + part.position.z +
                                  part.size.z * 0.5f;
                if (part.kind != "floor" || part.size.x != zone.cellSize.x ||
                    part.size.y != zone.cellSize.y ||
                    glm::length(part.rotationEuler) > 0.001f ||
                    (foundFloor && std::abs(top - floorZ) > 0.001f)) {
                    valid = false;
                    break;
                }
                floorZ = top;
                foundFloor = true;
            }
            if (!valid) break;
        }
        world.connectedFlatGround = valid && foundFloor;
        world.connectedFlatGroundZ = floorZ;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "world-stream.server-ground-config", mapId,
            world.connectedFlatGround ? "flat_floor_enabled" :
                                        "flat_floor_not_eligible", 0,
            {{"map", mapId}, {"enabled", world.connectedFlatGround},
             {"floor_z", floorZ}, {"chunks", config.chunks.size()}},
            __FILE__, __LINE__, __FUNCTION__);

        // Build the server-authoritative collision/spawn neighborhood from
        // the same authored chunk templates the client streams. This is the
        // fixed 60 Hz server's initial authoritative footprint; live streaming
        // around moving actors is a separate follow-up.
        std::vector<const AuthoredChunk*> candidates;
        for (const AuthoredChunk& chunk : config.chunks)
            if (chunk.enabled && !chunk.parts.empty())
                candidates.push_back(&chunk);
        std::vector<AuthoredChunk> modeChunks;
        if (allowStreamedFootprint) {
            MimitaWorldGen::WorldGenMode profile;
            std::string profileError;
            if (MimitaWorldGen::WorldGenLibrary::instance().loadMode(
                    mapId, profile, &profileError) &&
                MimitaWorldGen::WorldGenLibrary::instance().loadChunksForMode(
                    profile, modeChunks, &profileError)) {
                for (const AuthoredChunk& chunk : modeChunks) {
                    if (!chunk.enabled || chunk.parts.empty())
                        continue;
                    const bool shadowed = std::any_of(
                        candidates.begin(), candidates.end(),
                        [&chunk](const AuthoredChunk* authored) {
                            return authored && authored->id == chunk.id;
                        });
                    if (!shadowed)
                        candidates.push_back(&chunk);
                }
            }
        }
        if (candidates.empty())
            continue;
        // Keep the headless server's initial authored footprint bounded. Live
        // server collision streaming extends it around active actors after the
        // server is ready; building the full configured radius here delays
        // logger readiness and first join.
        constexpr int kServerInitialFootprintRadius = 4;
        const int radiusX = std::clamp(zone.streamRadius.x, 0,
                                       kServerInitialFootprintRadius);
        const int radiusY = std::clamp(zone.streamRadius.y, 0,
                                       kServerInitialFootprintRadius);
        size_t cells = 0;
        size_t partsAdded = 0;
        size_t spawnsAdded = 0;
        std::string deps;
        for (const std::string& dep : zone.chunkDependencies) {
            if (!deps.empty()) deps += ",";
            deps += dep;
        }

        auto addChunkInstance = [&](const AuthoredChunk& chunk,
                                    const glm::mat4& chunkTransform) {
            for (const ChunkPart& part : chunk.parts) {
                if (!part.collision) continue;
                MimitaNet::HeadlessWorld partWorld;
                buildBoxCollisionTriangles(partWorld.triangles, glm::vec3(0.0f),
                                           part.size * 0.5f);
                glm::mat4 rotation(1.0f);
                rotation = glm::rotate(rotation,
                    glm::radians(part.rotationEuler.z), glm::vec3(0, 0, 1));
                rotation = glm::rotate(rotation,
                    glm::radians(part.rotationEuler.y), glm::vec3(0, 1, 0));
                rotation = glm::rotate(rotation,
                    glm::radians(part.rotationEuler.x), glm::vec3(1, 0, 0));
                const glm::mat4 transform = chunkTransform *
                    glm::translate(glm::mat4(1.0f), part.position) * rotation;
                MimitaNet::appendHeadlessWorldInstance(world, partWorld,
                                                       transform, false);
                ++partsAdded;
            }
            for (const ChunkSpawnPoint& spawn : chunk.spawnPoints) {
                if (!spawn.enabled) continue;
                MimitaNet::ServerSpawnPoint point;
                point.position = glm::vec3(
                    chunkTransform * glm::vec4(spawn.position, 1.0f));
                point.tag = spawn.allowedActors;
                world.spawnPoints.push_back(std::move(point));
                ++spawnsAdded;
            }
        };

        if (zone.spacingMode == "connector" &&
            ModuleHost::instance().moduleLoaded("infinite-worldgen")) {
            std::vector<mimita_worldgen::ChunkV1> chunkViews;
            std::vector<mimita_worldgen::PartV1> partViews;
            std::vector<mimita_worldgen::ConnectorV1> connectorViews;
            std::vector<uint32_t> connectorOffsets;
            for (const AuthoredChunk* chunk : candidates) {
                mimita_worldgen::ChunkV1 view{};
                std::snprintf(view.id, sizeof(view.id), "%s",
                              chunk->id.c_str());
                view.boundsMin[0] = chunk->boundsMin.x;
                view.boundsMin[1] = chunk->boundsMin.y;
                view.boundsMin[2] = chunk->boundsMin.z;
                view.boundsMax[0] = chunk->boundsMax.x;
                view.boundsMax[1] = chunk->boundsMax.y;
                view.boundsMax[2] = chunk->boundsMax.z;
                view.enabled = chunk->enabled ? 1 : 0;
                view.partOffset = static_cast<uint32_t>(partViews.size());
                view.connectorOffset =
                    static_cast<uint32_t>(connectorViews.size());
                connectorOffsets.push_back(view.connectorOffset);
                for (const ChunkPart& part : chunk->parts) {
                    mimita_worldgen::PartV1 p{};
                    std::snprintf(p.id, sizeof(p.id), "%s", part.id.c_str());
                    p.position[0] = part.position.x;
                    p.position[1] = part.position.y;
                    p.position[2] = part.position.z;
                    p.size[0] = part.size.x;
                    p.size[1] = part.size.y;
                    p.size[2] = part.size.z;
                    partViews.push_back(p);
                }
                for (const AuthoredChunkConnector& connector :
                     chunk->connectors) {
                    mimita_worldgen::ConnectorV1 c{};
                    std::snprintf(c.id, sizeof(c.id), "%s",
                                  connector.id.c_str());
                    std::snprintf(c.side, sizeof(c.side), "%s",
                                  connector.side.c_str());
                    std::snprintf(c.orientation, sizeof(c.orientation), "%s",
                                  connector.orientation.c_str());
                    std::snprintf(c.role, sizeof(c.role), "%s",
                                  connector.role.c_str());
                    c.position[0] = connector.position.x;
                    c.position[1] = connector.position.y;
                    c.position[2] = connector.position.z;
                    c.radius = connector.radius;
                    connectorViews.push_back(c);
                }
                view.partCount =
                    static_cast<uint32_t>(partViews.size()) - view.partOffset;
                view.connectorCount =
                    static_cast<uint32_t>(connectorViews.size()) -
                    view.connectorOffset;
                chunkViews.push_back(view);
            }
            mimita_worldgen::TablesV1 tables{};
            tables.chunks = chunkViews.data();
            tables.chunkCount = static_cast<uint32_t>(chunkViews.size());
            tables.parts = partViews.data();
            tables.partCount = static_cast<uint32_t>(partViews.size());
            tables.connectors = connectorViews.data();
            tables.connectorCount =
                static_cast<uint32_t>(connectorViews.size());

            mimita_worldgen::ZoneV1 zoneView{};
            std::snprintf(zoneView.id, sizeof(zoneView.id), "%s",
                          zone.id.c_str());
            std::snprintf(zoneView.spacingMode, sizeof(zoneView.spacingMode),
                          "%s", zone.spacingMode.c_str());
            std::snprintf(zoneView.generatorVersion,
                          sizeof(zoneView.generatorVersion), "%s",
                          zone.generatorVersion.c_str());
            std::snprintf(zoneView.dependencies, sizeof(zoneView.dependencies),
                          "%s", deps.c_str());
            zoneView.seed = zone.seed;
            zoneView.repeatAxes[0] = zone.repeatAxes.x;
            zoneView.repeatAxes[1] = zone.repeatAxes.y;
            zoneView.repeatAxes[2] = zone.repeatAxes.z;
            zoneView.streamRadius[0] = zone.streamRadius.x;
            zoneView.streamRadius[1] = zone.streamRadius.y;
            zoneView.streamRadius[2] = zone.streamRadius.z;
            zoneView.connectorGap = zone.connectorGap;
            zoneView.maxBranchDepth = zone.maxBranchDepth;
            zoneView.allowCycles = zone.allowCycles ? 1 : 0;
            zoneView.allowCrossings = zone.allowCrossings ? 1 : 0;

            const float radiusMeters = static_cast<float>(std::max(
                {zone.streamRadius.x, zone.streamRadius.y,
                 zone.streamRadius.z, 1}));
            auto withinRadius = [&](const glm::vec3& boundsMin,
                                    const glm::vec3& boundsMax) {
                const glm::vec3 closest =
                    glm::clamp(zone.origin, boundsMin, boundsMax);
                return glm::dot(closest - zone.origin,
                                closest - zone.origin) <=
                       radiusMeters * radiusMeters;
            };
            auto placementKey = [](const glm::mat4& transform) {
                const int64_t values[3] = {
                    static_cast<int64_t>(std::llround(transform[3][0] * 100.0f)),
                    static_cast<int64_t>(std::llround(transform[3][1] * 100.0f)),
                    static_cast<int64_t>(std::llround(transform[3][2] * 100.0f)),
                };
                uint64_t hash = 1469598103934665603ull;
                const unsigned char* bytes =
                    reinterpret_cast<const unsigned char*>(values);
                for (size_t i = 0; i < sizeof(values); ++i) {
                    hash ^= bytes[i];
                    hash *= 1099511628211ull;
                }
                return hash;
            };

            struct Placed {
                int chunkIndex = -1;
                glm::mat4 transform{1.0f};
                glm::vec3 boundsMin{0.0f};
                glm::vec3 boundsMax{0.0f};
            };
            std::deque<Placed> queue;
            std::unordered_set<uint64_t> seen;
            mimita_worldgen::ResolveV1 seed{};
            seed.tables = &tables;
            seed.zone = zoneView;
            seed.zone.origin[0] = zone.origin.x;
            seed.zone.origin[1] = zone.origin.y;
            seed.zone.origin[2] = zone.origin.z;
            seed.requestKind = 0;
            seed.cell[0] = seed.cell[1] = seed.cell[2] = 0;
            if (ModuleHost::instance().worldgenResolve(&seed) && seed.outValid &&
                seed.outChunk >= 0) {
                Placed placed;
                placed.chunkIndex = seed.outChunk;
                std::memcpy(&placed.transform[0][0], seed.outTransform,
                            sizeof(float) * 16);
                placed.boundsMin = glm::vec3(seed.outBoundsMin[0],
                                             seed.outBoundsMin[1],
                                             seed.outBoundsMin[2]);
                placed.boundsMax = glm::vec3(seed.outBoundsMax[0],
                                             seed.outBoundsMax[1],
                                             seed.outBoundsMax[2]);
                seen.insert(placementKey(placed.transform));
                queue.push_back(placed);
            }
            constexpr size_t kServerInitialChunkCap = 64;
            while (!queue.empty() && cells < kServerInitialChunkCap) {
                const Placed placed = queue.front();
                queue.pop_front();
                const AuthoredChunk* chunk = candidates[placed.chunkIndex];
                if (!chunk)
                    continue;
                addChunkInstance(*chunk, placed.transform);
                ++cells;
                for (size_t connectorIndex = 0;
                     connectorIndex < chunk->connectors.size();
                     ++connectorIndex) {
                    mimita_worldgen::ResolveV1 request{};
                    request.tables = &tables;
                    request.zone = zoneView;
                    request.requestKind = 1;
                    request.parentChunk = placed.chunkIndex;
                    request.parentConnector = static_cast<int>(
                        connectorOffsets[placed.chunkIndex] + connectorIndex);
                    std::memcpy(request.parentTransform, &placed.transform[0][0],
                                sizeof(float) * 16);
                    if (!ModuleHost::instance().worldgenResolve(&request) ||
                        !request.outValid || request.outChunk < 0)
                        continue;
                    glm::mat4 transform(1.0f);
                    std::memcpy(&transform[0][0], request.outTransform,
                                sizeof(float) * 16);
                    const uint64_t key = placementKey(transform);
                    if (!seen.insert(key).second)
                        continue;
                    const glm::vec3 boundsMin(request.outBoundsMin[0],
                                              request.outBoundsMin[1],
                                              request.outBoundsMin[2]);
                    const glm::vec3 boundsMax(request.outBoundsMax[0],
                                              request.outBoundsMax[1],
                                              request.outBoundsMax[2]);
                    if (!withinRadius(boundsMin, boundsMax))
                        continue;
                    Placed child;
                    child.chunkIndex = request.outChunk;
                    child.transform = transform;
                    child.boundsMin = boundsMin;
                    child.boundsMax = boundsMax;
                    queue.push_back(child);
                }
            }
        } else {
            for (int cy = -radiusY; cy <= radiusY; ++cy)
                for (int cx = -radiusX; cx <= radiusX; ++cx) {
                    const glm::vec3 cellOrigin = zone.origin +
                        glm::vec3((float)cx * zone.cellSize.x,
                                  (float)cy * zone.cellSize.y, 0.0f);
                    const uint64_t hash = serverCellHash(
                        zone.seed, zone.generatorVersion, deps, cx, cy, 0);
                    const AuthoredChunk* chunk =
                        candidates[hash % candidates.size()];
                    ++cells;
                    addChunkInstance(*chunk,
                                     glm::translate(glm::mat4(1.0f), cellOrigin));
                }
        }
        std::string descriptorIdentity = mapId + "|" +
            std::to_string(zone.seed) + "|" + zone.generatorVersion + "|" +
            zone.ruleSet + "|" + deps;
        const uint64_t descriptorHash =
            std::hash<std::string>{}(descriptorIdentity);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "world-stream.identity", mapId,
            "server canonical world identity", 0,
            {{"side", "server"}, {"map", mapId}, {"seed", zone.seed},
             {"generator_version", zone.generatorVersion},
             {"descriptor_hash", descriptorHash}},
            __FILE__, __LINE__, __FUNCTION__);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "world-stream.server.cells", mapId,
            "server built authoritative chunk collision and spawn anchors", 0,
            {{"map", mapId}, {"cells", cells}, {"seed", zone.seed},
             {"generator_version", zone.generatorVersion},
             {"descriptor_hash", descriptorHash},
             {"parts_added", partsAdded}, {"spawns_added", spawnsAdded},
             {"radius", {radiusX, radiusY, 0}},
             {"triangles", world.triangles.size()}},
            __FILE__, __LINE__, __FUNCTION__);
        break;
    }
    applyGLBObjectOverrides(world, config.glbObjects);
    applyGLBObjectClones(world, config.glbObjects);
    size_t added = 0;
    for (const AuthoredBlock& block : config.blocks) {
        if (!block.enabled || !finiteVec(block.position) || !finiteVec(block.size))
            continue;
        if (block.size.x <= 0.0001f || block.size.y <= 0.0001f || block.size.z <= 0.0001f)
            continue;
        MimitaNet::HeadlessWorld cube;
        buildBoxCollisionTriangles(cube.triangles, glm::vec3(0.0f), block.size * 0.5f);
        MimitaNet::appendHeadlessWorldInstance(world, cube, blockTransform(block),
                                               false);
        ++added;
    }

    StaticUnionStats unionStats;
    StaticUnionStats removalStats;
    MimitaNet::finalizeHeadlessWorldCollision(world, &unionStats,
                                              &removalStats);

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
                       {"raw_triangles", world.rawTriangles.size()},
                       {"raw_base", world.rawBaseValid},
                       {"glb_override_count", config.glbObjects.size()},
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

namespace {

void setNodeCollisionHidden(World& world, const MapGLBObject& object,
                            bool hidden)
{
    if (object.collisionSolidId == 0)
        return;
    CollisionMeshCache& collision = world.collisionMesh;
    if (collision.hidden.size() != collision.triangles.size())
        collision.hidden.assign(collision.triangles.size(), 0);
    const uint8_t value = hidden ? 1 : 0;
    for (size_t i = 0; i < collision.solidIds.size() && i < collision.hidden.size();
         ++i)
        if (collision.solidIds[i] == object.collisionSolidId)
            collision.hidden[i] = value;
}

} // namespace

bool hideDestructibleMapNode(World& world, int nodeIndex,
                            unsigned int* textureHandle)
{
    const auto object = std::find_if(world.mesh.objects.begin(),
        world.mesh.objects.end(), [nodeIndex](const MapGLBObject& candidate) {
            return candidate.nodeIndex == nodeIndex;
        });
    if (object == world.mesh.objects.end() || object->vertexCount == 0 ||
        object->vertexFirst + object->vertexCount > world.mesh.verts.size())
        return false;
    if (textureHandle)
        for (const Mesh::Batch& batch : world.mesh.batches)
            if (batch.first <= object->vertexFirst &&
                batch.first + batch.count > object->vertexFirst)
            {
                *textureHandle = batch.texture;
                break;
            }
    if (object->hidden)
        return true;
    const auto start = std::chrono::steady_clock::now();
    object->hidden = true;
    setNodeCollisionHidden(world, *object, true);
    const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Verbose,
        "world.destructible-map-hide", std::to_string(nodeIndex),
        "authored map node hidden without a full-map rebuild", 0,
        {{"node", nodeIndex}, {"solid", object->collisionSolidId},
         {"hidden_triangles", world.collisionMesh.hidden.size()},
         {"ms", ms}});
    return true;
}

bool restoreDestructibleMapNode(World& world, int nodeIndex)
{
    const auto object = std::find_if(world.mesh.objects.begin(),
        world.mesh.objects.end(), [nodeIndex](const MapGLBObject& candidate) {
            return candidate.nodeIndex == nodeIndex;
        });
    if (object == world.mesh.objects.end() || !object->hidden)
        return false;
    const auto start = std::chrono::steady_clock::now();
    object->hidden = false;
    setNodeCollisionHidden(world, *object, false);
    const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Verbose,
        "world.destructible-map-restore", std::to_string(nodeIndex),
        "authored map node restored without a full-map rebuild", 0,
        {{"node", nodeIndex}, {"solid", object->collisionSolidId},
         {"ms", ms}});
    return true;
}

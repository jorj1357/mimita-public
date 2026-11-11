// 2026-11-07
#include "network/server-infinite-collision.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "config/collision-lod-config.h"
#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "hot-reload/module-host.h"
#include "modules/api/worldgen-api.h"
#include "procedural/procedural-world.h"
#include "map/map-loader-collision.h"
#include "map/map-static-union.h"
#include "world/streamed-world-layer.h"
#include "world/world.h"
#include "worldgen/worldgen-library.h"

namespace {

constexpr size_t kMaxServerChunks = 512;
constexpr uint64_t kRebuildIntervalMs = 250;

struct ServerChunk
{
    std::string chunkId;
    glm::mat4 transform{1.0f};
    std::vector<CollisionTriangle> triangles;
    std::vector<uint32_t> solidIds;
};

struct ServerStreamState
{
    std::string mapId;
    std::string profileId;
    uint64_t configRevision = 0;
    uint64_t lastBuildMs = 0;
    bool haveProfile = false;
    bool unsupportedConnector = false;
    bool disabled = false;
    bool haveConnectorAnchor = false;
    glm::vec3 connectorAnchor{0.0f};
    std::vector<AuthoredChunk> libraryChunks;
    std::vector<const AuthoredChunk*> candidates;
    MimitaWorldGen::WorldGenMode profile;
    std::unordered_map<uint64_t, ServerChunk> loaded;
};

ServerStreamState gState;

uint64_t cellKey(int x, int y, int z)
{
    return (uint64_t(uint32_t(x)) << 42) ^ (uint64_t(uint32_t(y)) << 21) ^
           uint64_t(uint32_t(z));
}

uint64_t nowMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<
        std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::string zoneDepsIdentity(const GenerationZone& zone)
{
    std::string identity;
    for (size_t i = 0; i < zone.chunkDependencies.size(); ++i) {
        if (i) identity += ",";
        identity += zone.chunkDependencies[i];
    }
    return identity;
}

// Must match InfiniteWorldRuntime::mixCoord so client and server select the
// same authored chunk in the same grid cell.
uint64_t mixCoord(uint32_t seed, const std::string& version,
                  const std::string& deps, int x, int y, int z)
{
    uint64_t h = MimitaProcedural::proceduralZoneChunkHash(seed, version, deps,
                                                           x, y);
    h ^= (uint64_t(uint32_t(z)) * 0xC2B2AE3D27D4EB4Full) + 0x9E3779B97F4A7C15ull;
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
    return h ^ (h >> 27);
}

void resetState()
{
    gState = ServerStreamState{};
}

bool ensureProfile()
{
    const MapObjectiveConfig& config = MapConfigRegistry::instance().current();
    const std::string mapId = config.mapId.empty() ? "connected_world"
                                                    : config.mapId;
    if (gState.haveProfile && gState.mapId == mapId &&
        gState.configRevision == MapConfigRegistry::instance().revision())
        return true;

    std::string error;
    MimitaWorldGen::WorldGenLibrary::instance().ensureDefaults(&error);
    MimitaWorldGen::WorldGenMode profile;
    if (!MimitaWorldGen::WorldGenLibrary::instance().loadMode(mapId, profile,
                                                               &error)) {
        if (!MimitaWorldGen::WorldGenLibrary::instance().loadMode(
                "connected_world", profile, &error)) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcMovement, StructuredLevel::Important,
                "npc.collision-world.stream.missing", mapId,
                "server streamed collision profile could not be loaded", 0,
                {{"map", mapId}, {"error", error}}, __FILE__, __LINE__,
                __func__);
            return false;
        }
    }

    std::vector<AuthoredChunk> libraryChunks;
    if (!MimitaWorldGen::WorldGenLibrary::instance().loadChunksForMode(
            profile, libraryChunks, &error)) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::NpcMovement, StructuredLevel::Important,
            "npc.collision-world.stream.missing", mapId,
            "server streamed chunk library could not be loaded", 0,
            {{"map", mapId}, {"error", error}}, __FILE__, __LINE__, __func__);
        return false;
    }

    std::vector<const AuthoredChunk*> candidates;
    for (const AuthoredChunk& chunk : config.chunks) {
        if (chunk.enabled && !chunk.parts.empty())
            candidates.push_back(&chunk);
    }
    for (const AuthoredChunk& chunk : libraryChunks) {
        if (!chunk.enabled || chunk.parts.empty())
            continue;
        const bool shadowed = std::any_of(
            candidates.begin(), candidates.end(),
            [&chunk](const AuthoredChunk* existing) {
                return existing && existing->id == chunk.id;
            });
        if (!shadowed)
            candidates.push_back(&chunk);
    }
    if (candidates.empty()) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::NpcMovement, StructuredLevel::Important,
            "npc.collision-world.stream.missing", mapId,
            "server streamed collision has no enabled chunk candidates", 0,
            {{"map", mapId}}, __FILE__, __LINE__, __func__);
        return false;
    }

    gState.mapId = mapId;
    gState.profileId = profile.id;
    gState.configRevision = MapConfigRegistry::instance().revision();
    gState.profile = std::move(profile);
    gState.libraryChunks = std::move(libraryChunks);
    gState.candidates = std::move(candidates);
    gState.haveProfile = true;
    // Connector mode is only safe to approximate as a regular grid when every
    // candidate has the profile's footprint. Mixed-size connector worlds keep
    // their client-owned graph placement and are reported as unsupported.
    bool gridCompatible = true;
    for (const AuthoredChunk* chunk : gState.candidates) {
        const glm::vec3 size = chunk->boundsMax - chunk->boundsMin;
        if (std::fabs(size.x - gState.profile.grid.cellSize.x) > 0.5f ||
            std::fabs(size.y - gState.profile.grid.cellSize.y) > 0.5f) {
            gridCompatible = false;
            break;
        }
    }
    gState.unsupportedConnector =
        gState.profile.grid.spacingMode == "connector" && !gridCompatible;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::NpcMovement, StructuredLevel::Important,
        "npc.collision-world.stream.profile", mapId,
        gState.unsupportedConnector
            ? "server streamed collision using authored map chunks because connector placement is client-owned"
            : "server streamed collision profile resolved",
        0,
        {{"map", mapId},
         {"profile", gState.profileId},
         {"chunks", gState.candidates.size()},
         {"spacing_mode", gState.profile.grid.spacingMode},
         {"stream_radius", {gState.profile.grid.streamRadius.x,
                            gState.profile.grid.streamRadius.y,
                            gState.profile.grid.streamRadius.z}}},
        __FILE__, __LINE__, __func__);
    return true;
}

const GenerationZone* connectedZone(const MapObjectiveConfig& config)
{
    for (const GenerationZone& zone : config.generationZones) {
        if (zone.enabled && zone.modeId == "connected_world")
            return &zone;
    }
    return nullptr;
}

GenerationZone syntheticZone(const MapObjectiveConfig& config,
                             const MimitaWorldGen::WorldGenMode& profile)
{
    GenerationZone zone;
    zone.id = "connected_world";
    zone.name = "Connected World";
    zone.modeId = "connected_world";
    zone.enabled = true;
    for (const AuthoredChunk& chunk : config.chunks) {
        if (!chunk.enabled) continue;
        zone.boundsMin = glm::min(zone.boundsMin, chunk.boundsMin);
        zone.boundsMax = glm::max(zone.boundsMax, chunk.boundsMax);
    }
    zone.origin = profile.grid.origin;
    zone.cellSize = profile.grid.cellSize;
    zone.chunkSizeVec = profile.grid.chunkSize;
    zone.repeatAxes = profile.grid.repeatAxes;
    zone.streamRadius = profile.grid.streamRadius;
    zone.spacingMode = profile.grid.spacingMode;
    zone.connectorGap = profile.grid.connectorGap;
    zone.maxBranchDepth = profile.maxBranchDepth;
    zone.allowCycles = profile.allowCycles;
    zone.allowCrossings = profile.allowCrossings;
    return zone;
}

ServerChunk buildChunk(uint64_t key, const AuthoredChunk& chunk,
                       const glm::mat4& transform)
{
    ServerChunk record;
    record.chunkId = chunk.id;
    record.transform = transform;
    buildStreamedChunkCollision(chunk, transform, record.triangles,
                                record.solidIds);
    return record;
}

void applySurface(World& npcWorld,
                  const std::vector<CollisionTriangle>& triangles,
                  const std::vector<uint32_t>& solidIds, size_t chunkCount)
{
    CollisionMeshCache& mesh = npcWorld.collisionMesh;
    if (mesh.streamedTailTriangles > 0 &&
        mesh.streamedTailTriangles <= mesh.triangles.size())
        mesh.triangles.resize(mesh.triangles.size() -
                              mesh.streamedTailTriangles);
    if (mesh.streamedTailSolidIds > 0 &&
        mesh.streamedTailSolidIds <= mesh.solidIds.size())
        mesh.solidIds.resize(mesh.solidIds.size() -
                             mesh.streamedTailSolidIds);
    if (mesh.triangleAABBs.size() > mesh.triangles.size())
        mesh.triangleAABBs.resize(mesh.triangles.size());
    if (mesh.triangleOBBs.size() > mesh.triangles.size())
        mesh.triangleOBBs.resize(mesh.triangles.size());
    if (!mesh.hidden.empty() && mesh.hidden.size() > mesh.triangles.size())
        mesh.hidden.resize(mesh.triangles.size());

    const size_t baseTriangles = mesh.triangles.size();
    mesh.triangles.insert(mesh.triangles.end(), triangles.begin(),
                          triangles.end());
    mesh.solidIds.insert(mesh.solidIds.end(), solidIds.begin(),
                         solidIds.end());
    if (!mesh.hidden.empty() && mesh.hidden.size() == baseTriangles)
        mesh.hidden.resize(mesh.triangles.size(), 0);

    // Keep the base collision tree stable. The server streamed tail gets its
    // own tree, so chunk churn never rebuilds the map-wide broadphase.
    mesh.triangleAABBs.resize(mesh.triangles.size());
    mesh.triangleOBBs.resize(mesh.triangles.size());
    for (size_t i = 0; i < triangles.size(); ++i) {
        const size_t index = baseTriangles + i;
        buildCollisionTrianglePrimitive(mesh.triangles[index],
                                        mesh.triangleAABBs[index],
                                        mesh.triangleOBBs[index]);
        mesh.boundsMin = glm::min(mesh.boundsMin,
                                  mesh.triangleAABBs[index].min);
        mesh.boundsMax = glm::max(mesh.boundsMax,
                                  mesh.triangleAABBs[index].max);
    }
    npcWorld.streamedCollisionTree.clear();
    if (!triangles.empty()) {
        std::vector<int> streamedIds(triangles.size());
        for (size_t i = 0; i < streamedIds.size(); ++i)
            streamedIds[i] = static_cast<int>(baseTriangles + i);
        npcWorld.streamedCollisionTree.build(streamedIds, mesh.triangleAABBs);
    }
    mesh.streamedTailTriangles = triangles.size();
    mesh.streamedTailSolidIds = solidIds.size();
    mesh.streamedChunkCount = static_cast<uint32_t>(chunkCount);
    mesh.streamedRevision++;
}

void rebuildSurface(World& npcWorld)
{
    std::vector<CollisionTriangle> surface;
    std::vector<uint32_t> surfaceSolidIds;
    surface.reserve(gState.loaded.size() * 12);
    surfaceSolidIds.reserve(gState.loaded.size() * 12);

    uint32_t solidBase = 0;
    for (auto& pair : gState.loaded) {
        ServerChunk& chunk = pair.second;
        for (size_t i = 0; i < chunk.triangles.size(); ++i) {
            surface.push_back(chunk.triangles[i]);
            const uint32_t local =
                i < chunk.solidIds.size() ? chunk.solidIds[i] : 0u;
            surfaceSolidIds.push_back(local == 0 ? 0u : solidBase + local);
        }
        solidBase += 1024;
    }
    if (!surface.empty() && CollisionLodConfig::instance().staticFaceRemoval())
        removeFullyCoveredStaticTriangles(surface, surfaceSolidIds);
    applySurface(npcWorld, surface, surfaceSolidIds, gState.loaded.size());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::NpcMovement, StructuredLevel::Important,
        "npc.collision-world.stream.sync", gState.mapId,
        "server NPC collision tail rebuilt from streamed grid chunks", 0,
        {{"map", gState.mapId},
         {"profile", gState.profileId},
         {"chunks", gState.loaded.size()},
         {"world_triangles", npcWorld.collisionMesh.triangles.size()},
         {"stream_triangles", surface.size()},
         {"revision", npcWorld.collisionMesh.streamedRevision}},
        __FILE__, __LINE__, __func__);
}

} // namespace

uint64_t placementKey(const glm::mat4& transform)
{
    uint64_t hash = 1469598103934665603ull;
    for (int axis = 0; axis < 3; ++axis) {
        const int64_t value =
            static_cast<int64_t>(std::llround(transform[3][axis] * 100.0f));
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(&value);
        for (size_t i = 0; i < sizeof(value); ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

bool buildConnectorSurface(
    const GenerationZone& zone,
    const std::vector<const AuthoredChunk*>& candidates,
    const glm::vec3& anchor,
    std::unordered_map<uint64_t, ServerChunk>& out)
{
    if (candidates.empty() ||
        !ModuleHost::instance().moduleLoaded("infinite-worldgen"))
        return false;

    std::vector<mimita_worldgen::ChunkV1> chunkViews;
    std::vector<mimita_worldgen::PartV1> partViews;
    std::vector<mimita_worldgen::ConnectorV1> connectorViews;
    std::vector<uint32_t> connectorOffsets;
    for (const AuthoredChunk* chunk : candidates) {
        if (!chunk)
            continue;
        mimita_worldgen::ChunkV1 view{};
        std::snprintf(view.id, sizeof(view.id), "%s", chunk->id.c_str());
        view.boundsMin[0] = chunk->boundsMin.x;
        view.boundsMin[1] = chunk->boundsMin.y;
        view.boundsMin[2] = chunk->boundsMin.z;
        view.boundsMax[0] = chunk->boundsMax.x;
        view.boundsMax[1] = chunk->boundsMax.y;
        view.boundsMax[2] = chunk->boundsMax.z;
        view.enabled = chunk->enabled ? 1 : 0;
        view.partOffset = static_cast<uint32_t>(partViews.size());
        view.connectorOffset = static_cast<uint32_t>(connectorViews.size());
        connectorOffsets.push_back(view.connectorOffset);
        for (const ChunkPart& part : chunk->parts) {
            mimita_worldgen::PartV1 p{};
            std::snprintf(p.id, sizeof(p.id), "%s", part.id.c_str());
            std::snprintf(p.kind, sizeof(p.kind), "%s", part.kind.c_str());
            p.position[0] = part.position.x;
            p.position[1] = part.position.y;
            p.position[2] = part.position.z;
            p.size[0] = part.size.x;
            p.size[1] = part.size.y;
            p.size[2] = part.size.z;
            p.rotation[0] = part.rotationEuler.x;
            p.rotation[1] = part.rotationEuler.y;
            p.rotation[2] = part.rotationEuler.z;
            p.collision = part.collision ? 1 : 0;
            p.visible = part.visible ? 1 : 0;
            partViews.push_back(p);
        }
        for (const AuthoredChunkConnector& connector : chunk->connectors) {
            mimita_worldgen::ConnectorV1 c{};
            std::snprintf(c.id, sizeof(c.id), "%s", connector.id.c_str());
            std::snprintf(c.side, sizeof(c.side), "%s", connector.side.c_str());
            std::snprintf(c.orientation, sizeof(c.orientation), "%s",
                          connector.orientation.c_str());
            std::snprintf(c.role, sizeof(c.role), "%s", connector.role.c_str());
            c.position[0] = connector.position.x;
            c.position[1] = connector.position.y;
            c.position[2] = connector.position.z;
            c.radius = connector.radius;
            std::string tags;
            for (const std::string& tag : connector.tags) {
                if (!tags.empty()) tags += "|";
                tags += tag;
            }
            std::snprintf(c.tags, sizeof(c.tags), "%s", tags.c_str());
            std::string links;
            for (const std::string& link : connector.links) {
                if (!links.empty()) links += "|";
                links += link;
            }
            std::snprintf(c.links, sizeof(c.links), "%s", links.c_str());
            connectorViews.push_back(c);
        }
        view.partCount =
            static_cast<uint32_t>(partViews.size()) - view.partOffset;
        view.connectorCount =
            static_cast<uint32_t>(connectorViews.size()) - view.connectorOffset;
        chunkViews.push_back(view);
    }
    if (chunkViews.empty())
        return false;

    mimita_worldgen::TablesV1 tables{};
    tables.chunks = chunkViews.data();
    tables.chunkCount = static_cast<uint32_t>(chunkViews.size());
    tables.parts = partViews.data();
    tables.partCount = static_cast<uint32_t>(partViews.size());
    tables.connectors = connectorViews.data();
    tables.connectorCount = static_cast<uint32_t>(connectorViews.size());

    mimita_worldgen::ZoneV1 zoneView{};
    std::snprintf(zoneView.id, sizeof(zoneView.id), "%s", zone.id.c_str());
    std::snprintf(zoneView.plane, sizeof(zoneView.plane), "%s",
                  zone.plane.c_str());
    std::snprintf(zoneView.spacingMode, sizeof(zoneView.spacingMode), "%s",
                  zone.spacingMode.c_str());
    std::snprintf(zoneView.generatorVersion,
                  sizeof(zoneView.generatorVersion), "%s",
                  zone.generatorVersion.c_str());
    std::snprintf(zoneView.ruleSet, sizeof(zoneView.ruleSet), "%s",
                  zone.ruleSet.c_str());
    std::string deps;
    for (const std::string& dep : zone.chunkDependencies) {
        if (!deps.empty()) deps += ",";
        deps += dep;
    }
    std::snprintf(zoneView.dependencies, sizeof(zoneView.dependencies), "%s",
                  deps.c_str());
    zoneView.seed = zone.seed;
    zoneView.origin[0] = anchor.x;
    zoneView.origin[1] = anchor.y;
    zoneView.origin[2] = anchor.z;
    zoneView.cellSize[0] = zone.cellSize.x;
    zoneView.cellSize[1] = zone.cellSize.y;
    zoneView.cellSize[2] = zone.cellSize.z;
    zoneView.chunkSize[0] = zone.chunkSizeVec.x;
    zoneView.chunkSize[1] = zone.chunkSizeVec.y;
    zoneView.chunkSize[2] = zone.chunkSizeVec.z;
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
        {zone.streamRadius.x, zone.streamRadius.y, zone.streamRadius.z, 1}));
    auto withinRadius = [&](const glm::vec3& boundsMin,
                            const glm::vec3& boundsMax) {
        const glm::vec3 closest = glm::clamp(anchor, boundsMin, boundsMax);
        return glm::dot(closest - anchor, closest - anchor) <=
               radiusMeters * radiusMeters;
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
    seed.requestKind = 0;
    seed.cell[0] = seed.cell[1] = seed.cell[2] = 0;
    if (!ModuleHost::instance().worldgenResolve(&seed) || !seed.outValid ||
        seed.outChunk < 0)
        return false;
    Placed first;
    first.chunkIndex = seed.outChunk;
    std::memcpy(&first.transform[0][0], seed.outTransform, sizeof(float) * 16);
    first.boundsMin = glm::vec3(seed.outBoundsMin[0], seed.outBoundsMin[1],
                                seed.outBoundsMin[2]);
    first.boundsMax = glm::vec3(seed.outBoundsMax[0], seed.outBoundsMax[1],
                                seed.outBoundsMax[2]);
    seen.insert(placementKey(first.transform));
    queue.push_back(first);

    size_t cells = 0;
    while (!queue.empty() && cells < kMaxServerChunks) {
        const Placed placed = queue.front();
        queue.pop_front();
        if (placed.chunkIndex < 0 ||
            static_cast<size_t>(placed.chunkIndex) >= candidates.size())
            continue;
        const AuthoredChunk* chunk = candidates[placed.chunkIndex];
        if (!chunk || chunk->parts.empty())
            continue;
        if (!withinRadius(placed.boundsMin, placed.boundsMax))
            continue;
        out.emplace(placementKey(placed.transform),
                    buildChunk(placementKey(placed.transform), *chunk,
                               placed.transform));
        ++cells;
        for (size_t connectorIndex = 0;
             connectorIndex < chunk->connectors.size(); ++connectorIndex) {
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
            Placed child;
            child.chunkIndex = request.outChunk;
            child.transform = transform;
            child.boundsMin = glm::vec3(request.outBoundsMin[0],
                                        request.outBoundsMin[1],
                                        request.outBoundsMin[2]);
            child.boundsMax = glm::vec3(request.outBoundsMax[0],
                                        request.outBoundsMax[1],
                                        request.outBoundsMax[2]);
            if (!withinRadius(child.boundsMin, child.boundsMax))
                continue;
            queue.push_back(child);
        }
    }
    return !out.empty();
}

void serverInfiniteCollisionReset()
{
    resetState();
}

void serverInfiniteCollisionTick(const std::vector<glm::vec3>& anchors,
                                 World& npcWorld, bool streamingEnabled)
{
    if (!streamingEnabled) {
        if (gState.disabled)
            return;
        CollisionMeshCache& mesh = npcWorld.collisionMesh;
        const bool hadTail = mesh.streamedTailTriangles > 0 ||
                             mesh.streamedTailSolidIds > 0;
        if (hadTail)
            applySurface(npcWorld, {}, {}, 0);
        resetState();
        gState.disabled = true;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::NpcMovement, StructuredLevel::Important,
            "npc.collision-world.stream.disabled", "server",
            "streamed NPC collision disabled because the gamemode does not stream",
            0,
            {{"had_tail", hadTail},
             {"world_triangles", npcWorld.collisionMesh.triangles.size()}},
            __FILE__, __LINE__, __func__);
        return;
    }
    gState.disabled = false;
    if (anchors.empty())
        return;
    const uint64_t now = nowMs();
    if (now - gState.lastBuildMs < kRebuildIntervalMs)
        return;
    gState.lastBuildMs = now;

    const MapObjectiveConfig& config = MapConfigRegistry::instance().current();
    if (!ensureProfile())
        return;
    const GenerationZone* zone = connectedZone(config);
    GenerationZone fallback;
    if (!zone) {
        fallback = syntheticZone(config, gState.profile);
        zone = &fallback;
    }

    if (gState.profile.grid.spacingMode == "connector") {
        const glm::vec3 anchor = anchors.front();
        if (gState.haveConnectorAnchor && !gState.loaded.empty() &&
            glm::length(anchor - gState.connectorAnchor) < 1.5f)
            return;
        GenerationZone graphZone = *zone;
        graphZone.streamRadius = gState.profile.grid.streamRadius;
        // Cover the client's recent frontier between server anchor updates.
        graphZone.streamRadius.x += 12;
        graphZone.streamRadius.y += 12;
        graphZone.cellSize = gState.profile.grid.cellSize;
        graphZone.connectorGap = gState.profile.grid.connectorGap;
        graphZone.maxBranchDepth = gState.profile.maxBranchDepth;
        graphZone.allowCycles = gState.profile.allowCycles;
        graphZone.allowCrossings = gState.profile.allowCrossings;
        std::unordered_map<uint64_t, ServerChunk> next;
        if (!buildConnectorSurface(graphZone, gState.candidates, anchor, next)) {
            static uint64_t lastLog = 0;
            if (now - lastLog >= 5000) {
                lastLog = now;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::NpcMovement,
                    StructuredLevel::Important,
                    "npc.collision-world.stream.connector-missing",
                    gState.mapId,
                    "server connector graph could not be seeded",
                    0, {{"map", gState.mapId}}, __FILE__, __LINE__, __func__);
            }
            return;
        }
        gState.loaded = std::move(next);
        gState.connectorAnchor = anchor;
        gState.haveConnectorAnchor = true;
        rebuildSurface(npcWorld);
        return;
    }

    if (gState.unsupportedConnector) {
        static uint64_t lastLog = 0;
        if (now - lastLog >= 5000) {
            lastLog = now;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::NpcMovement, StructuredLevel::Important,
                "npc.collision-world.stream.connector-unsupported",
                gState.mapId,
                "connector-mode server collision uses the map-authored chunk footprint",
                0, {{"map", gState.mapId}}, __FILE__, __LINE__, __func__);
        }
        return;
    }

    const glm::vec3 origin = gState.profile.grid.origin;
    const glm::vec3 cell = gState.profile.grid.cellSize;
    const glm::ivec3 repeat = gState.profile.grid.repeatAxes;
    const glm::ivec3 radius = gState.profile.grid.streamRadius;
    const int rx = std::clamp(radius.x, 0, 512);
    const int ry = std::clamp(radius.y, 0, 512);
    const int rz = std::clamp(radius.z, 0, 2);
    const std::string deps = zoneDepsIdentity(*zone);

    struct Wanted
    {
        uint64_t key = 0;
        int x = 0;
        int y = 0;
        int z = 0;
        float distance = 0.0f;
    };
    std::unordered_set<uint64_t> desired;
    std::vector<Wanted> wanted;
    for (const glm::vec3& anchor : anchors) {
        const int cx = cell.x > 0.001f
            ? static_cast<int>(std::floor((anchor.x - origin.x) / cell.x))
            : 0;
        const int cy = cell.y > 0.001f
            ? static_cast<int>(std::floor((anchor.y - origin.y) / cell.y))
            : 0;
        const int cz = repeat.z && cell.z > 0.001f
            ? static_cast<int>(std::floor((anchor.z - origin.z) / cell.z))
            : 0;
        for (int dz = -rz; dz <= rz; ++dz)
        for (int dy = -ry; dy <= ry; ++dy)
        for (int dx = -rx; dx <= rx; ++dx) {
            const int x = repeat.x ? cx + dx : 0;
            const int y = repeat.y ? cy + dy : 0;
            const int z = repeat.z ? cz + dz : 0;
            const uint64_t key = cellKey(x, y, z);
            if (!desired.insert(key).second)
                continue;
            const glm::vec3 center =
                origin + glm::vec3(float(x) * cell.x, float(y) * cell.y,
                                   float(z) * cell.z);
            const glm::vec3 delta = center - anchor;
            wanted.push_back({key, x, y, z, glm::dot(delta, delta)});
        }
    }
    std::sort(wanted.begin(), wanted.end(),
              [](const Wanted& a, const Wanted& b) {
                  return a.distance < b.distance;
              });
    if (wanted.size() > kMaxServerChunks) {
        wanted.resize(kMaxServerChunks);
        desired.clear();
        for (const Wanted& item : wanted)
            desired.insert(item.key);
    }

    bool changed = false;
    for (auto it = gState.loaded.begin(); it != gState.loaded.end();) {
        if (desired.count(it->first)) {
            ++it;
            continue;
        }
        it = gState.loaded.erase(it);
        changed = true;
    }

    for (const Wanted& item : wanted) {
        if (gState.loaded.count(item.key) ||
            gState.loaded.size() >= kMaxServerChunks)
            continue;
        const uint64_t hash = mixCoord(zone->seed, zone->generatorVersion, deps,
                                       item.x, item.y, item.z);
        const AuthoredChunk* chunk =
            gState.candidates[hash % gState.candidates.size()];
        if (!chunk)
            continue;
        const glm::mat4 transform = glm::translate(
            glm::mat4(1.0f),
            origin + glm::vec3(float(item.x) * cell.x, float(item.y) * cell.y,
                               float(item.z) * cell.z));
        gState.loaded.emplace(item.key,
                              buildChunk(item.key, *chunk, transform));
        changed = true;
    }

    if (changed || npcWorld.collisionMesh.streamedChunkCount !=
                       gState.loaded.size())
        rebuildSurface(npcWorld);
}

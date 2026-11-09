// 2026-11-07
#include "network/server-infinite-collision.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "config/collision-lod-config.h"
#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "map/map-loader-collision.h"
#include "map/map-static-union.h"
#include "world/streamed-world-layer.h"
#include "world/world.h"
#include "worldgen/worldgen-library.h"

namespace {

constexpr size_t kMaxServerChunks = 256;
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

uint64_t mixCell(uint32_t seed, int x, int y, int z)
{
    uint64_t h = uint64_t(seed) * 0x9E3779B97F4A7C15ull;
    h ^= uint64_t(uint32_t(x)) * 0xC2B2AE3D27D4EB4Full;
    h ^= uint64_t(uint32_t(y)) * 0x165667B19E3779F9ull;
    h ^= uint64_t(uint32_t(z)) * 0x27D4EB2F165667C5ull;
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

    // Connector-mode placement is owned by the live worldgen module and its
    // graph frontier. The server stream currently covers the regular grid
    // mode; connector-mode authors should use the same finite chunk footprint
    // already authored into the map until the server frontier is extracted.
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
    const int rx = std::clamp(radius.x, 0, 12);
    const int ry = std::clamp(radius.y, 0, 12);
    const int rz = std::clamp(radius.z, 0, 2);

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
        const uint64_t hash = mixCell(zone->seed, item.x, item.y, item.z);
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

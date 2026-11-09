// 2026-11-05
#include "procedural/infinite-world-runtime.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "entities/player.h"
#include "gamemode/map-config.h"
#include "hot-reload/module-host.h"
#include "modules/api/worldgen-api.h"
#include "network/community-match-client.h"
#include "network/multiplayer-context.h"
#include "network/server-gamemode.h"
#include "procedural/procedural-world.h"
#include "world/streamed-world-layer.h"
#include "worldgen/worldgen-library.h"

extern MimitaNet::MultiplayerContext* gpMpContext;

namespace {

// Chunk installation creates render buffers and collision proxies. Keep that
// work out of a burst when a player crosses several cells at once, and let the
// pending neighborhood drain over subsequent frames.
constexpr int kMaxChunksPerFrame = 2;
constexpr size_t kMaxConnectorLoaded = 256;
constexpr size_t kMaxConnectorPending = 64;

uint64_t mixCoord(uint32_t seed, const std::string& version,
                  const std::string& deps, int x, int y, int z)
{
    uint64_t h = MimitaProcedural::proceduralZoneChunkHash(seed, version, deps,
                                                           x, y);
    h ^= (uint64_t(uint32_t(z)) * 0xC2B2AE3D27D4EB4Full) + 0x9E3779B97F4A7C15ull;
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
    return h ^ (h >> 27);
}

bool zoneContains(const GenerationZone& zone, const glm::vec3& point)
{
    const glm::vec3 min = glm::min(zone.boundsMin, zone.boundsMax);
    const glm::vec3 max = glm::max(zone.boundsMin, zone.boundsMax);
    if (min.x == max.x && min.y == max.y && min.z == max.z) return false;
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

std::string depsIdentity(const GenerationZone& zone)
{
    std::string identity;
    for (size_t i = 0; i < zone.chunkDependencies.size(); ++i) {
        if (i) identity += ",";
        identity += zone.chunkDependencies[i];
    }
    return identity;
}

enum class Side
{
    None,
    East,
    West,
    North,
    South,
    Up,
    Down
};

Side sideFromName(const std::string& name)
{
    if (name == "east") return Side::East;
    if (name == "west") return Side::West;
    if (name == "north") return Side::North;
    if (name == "south") return Side::South;
    if (name == "up") return Side::Up;
    if (name == "down") return Side::Down;
    return Side::None;
}

const char* sideName(Side side)
{
    switch (side) {
        case Side::East: return "east";
        case Side::West: return "west";
        case Side::North: return "north";
        case Side::South: return "south";
        case Side::Up: return "up";
        case Side::Down: return "down";
        default: return "none";
    }
}

Side oppositeSide(Side side)
{
    switch (side) {
        case Side::East: return Side::West;
        case Side::West: return Side::East;
        case Side::North: return Side::South;
        case Side::South: return Side::North;
        case Side::Up: return Side::Down;
        case Side::Down: return Side::Up;
        default: return Side::None;
    }
}

glm::vec3 sideDirection(Side side)
{
    switch (side) {
        case Side::East: return {1.0f, 0.0f, 0.0f};
        case Side::West: return {-1.0f, 0.0f, 0.0f};
        case Side::North: return {0.0f, 1.0f, 0.0f};
        case Side::South: return {0.0f, -1.0f, 0.0f};
        case Side::Up: return {0.0f, 0.0f, 1.0f};
        case Side::Down: return {0.0f, 0.0f, -1.0f};
        default: return {0.0f, 0.0f, 0.0f};
    }
}

Side sideBetween(int px, int py, int pz, int cx, int cy, int cz)
{
    if (cx > px) return Side::East;
    if (cx < px) return Side::West;
    if (cy > py) return Side::North;
    if (cy < py) return Side::South;
    if (cz > pz) return Side::Up;
    if (cz < pz) return Side::Down;
    return Side::None;
}

bool tagsCompatible(const std::vector<std::string>& a,
                    const std::vector<std::string>& b)
{
    // Empty tags are authored as "unconstrained". Tags only restrict a match
    // when both endpoints declare at least one.
    if (a.empty() || b.empty()) return true;
    for (const std::string& tag : a)
        if (std::find(b.begin(), b.end(), tag) != b.end()) return true;
    return false;
}

bool rolesCompatible(const std::string& a, const std::string& b)
{
    if (a == "none" || b == "none") return false;
    if (a == "bidirectional")
        return b == "entry" || b == "exit" || b == "bidirectional";
    if (b == "bidirectional")
        return a == "entry" || a == "exit" || a == "bidirectional";
    return (a == "entry" && b == "exit") || (a == "exit" && b == "entry");
}

bool connectorsCompatible(const AuthoredChunkConnector& a,
                          const AuthoredChunkConnector& b)
{
    return tagsCompatible(a.tags, b.tags) && rolesCompatible(a.role, b.role);
}

bool linkMatches(const std::string& link, const std::string& chunkId,
                 const std::string& connectorId)
{
    size_t separator = link.find('/');
    if (separator == std::string::npos) separator = link.find(':');
    if (separator == std::string::npos) return link == chunkId;
    return link.substr(0, separator) == chunkId &&
           link.substr(separator + 1) == connectorId;
}

bool linksAllow(const AuthoredChunkConnector& parent,
                const std::string& chunkId, const std::string& connectorId)
{
    if (parent.links.empty()) return true;
    for (const std::string& link : parent.links)
        if (linkMatches(link, chunkId, connectorId)) return true;
    return false;
}

bool hasExplicitLink(const AuthoredChunkConnector& connector,
                     const std::string& chunkId, const std::string& connectorId)
{
    if (connector.links.empty()) return false;
    for (const std::string& link : connector.links)
        if (linkMatches(link, chunkId, connectorId)) return true;
    return false;
}

const AuthoredChunkConnector* connectorOnSide(const AuthoredChunk& chunk,
                                              Side side)
{
    for (const AuthoredChunkConnector& connector : chunk.connectors)
        if (sideFromName(connector.side) == side) return &connector;
    return nullptr;
}

// A complete, mutually compatible edge set can be selected by cell hash
// without walking a parent chain back to the anchor. This keeps the flat
// connected-world trial bounded even with several authored tile variants.
bool independentGrid(const GenerationZone& zone,
                     const std::vector<const AuthoredChunk*>& candidates)
{
    if (zone.modeId != "connected_world" || zone.repeatAxes.z != 0 ||
        zone.connectorGap != 0.0f || candidates.empty()) return false;
    for (const AuthoredChunk* a : candidates) {
        if (!a || a->parts.empty()) return false;
        for (const AuthoredChunk* b : candidates) {
            if (!b) return false;
            for (Side side : {Side::East, Side::West, Side::North, Side::South}) {
                if ((side == Side::East || side == Side::West)
                        ? zone.repeatAxes.x == 0 : zone.repeatAxes.y == 0)
                    continue;
                const AuthoredChunkConnector* from = connectorOnSide(*a, side);
                const AuthoredChunkConnector* to = connectorOnSide(
                    *b, oppositeSide(side));
                if (!from || !to || !from->links.empty() ||
                    !to->links.empty() || !connectorsCompatible(*from, *to))
                    return false;
                const glm::vec3 step = sideDirection(side) * zone.cellSize;
                const float contactRadius =
                    std::max(0.01f, from->radius) + std::max(0.01f, to->radius);
                if (glm::length(from->position - step - to->position) >
                    contactRadius)
                    return false;
            }
        }
    }
    return true;
}

void logEvent(const char* name, const std::string& subject,
              const nlohmann::json& fields)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important, name, subject,
        "authored infinite-world streaming", 0, fields, __FILE__, __LINE__,
        __func__);
}

void signatureString(std::string& out, const std::string& value)
{
    out += value;
    out.push_back('\x1f');
}

void signatureFloat(std::string& out, float value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.5f", value);
    signatureString(out, buffer);
}

void signatureVec3(std::string& out, const glm::vec3& value)
{
    signatureFloat(out, value.x);
    signatureFloat(out, value.y);
    signatureFloat(out, value.z);
}

} // namespace

std::vector<WorldConnectorMatch> previewCompatibleConnectors(
    const std::string& chunkId, const std::string& connectorId,
    std::string* error)
{
    std::vector<WorldConnectorMatch> matches;
    const MapObjectiveConfig& config = MapConfigRegistry::instance().current();
    const AuthoredChunk* source = nullptr;
    for (const AuthoredChunk& chunk : config.chunks)
        if (chunk.id == chunkId) { source = &chunk; break; }
    if (!source) {
        if (error) *error = "chunk not found";
        return matches;
    }
    const AuthoredChunkConnector* sourceConnector = nullptr;
    for (const AuthoredChunkConnector& connector : source->connectors)
        if (connector.id == connectorId) { sourceConnector = &connector; break; }
    if (!sourceConnector) {
        if (error) *error = "connector not found";
        return matches;
    }
    const Side sourceSide = sideFromName(sourceConnector->side);
    const Side requiredSide = oppositeSide(sourceSide);
    if (requiredSide == Side::None) {
        if (error) *error = "connector side is invalid";
        return matches;
    }

    for (const AuthoredChunk& chunk : config.chunks) {
        if (chunk.id == chunkId) continue;
        for (const AuthoredChunkConnector& connector : chunk.connectors) {
            if (sideFromName(connector.side) != requiredSide) continue;
            const bool linked =
                linksAllow(*sourceConnector, chunk.id, connector.id) ||
                linksAllow(connector, source->id, sourceConnector->id);
            if (!linked && !connectorsCompatible(*sourceConnector, connector))
                continue;
            if (!sourceConnector->links.empty() && !linked) continue;
            matches.push_back({chunk.id, connector.id, connector.side,
                               connector.role, linked});
        }
    }
    std::sort(matches.begin(), matches.end(),
              [](const WorldConnectorMatch& a, const WorldConnectorMatch& b) {
                  if (a.chunkId != b.chunkId) return a.chunkId < b.chunkId;
                  return a.connectorId < b.connectorId;
              });
    return matches;
}

InfiniteWorldRuntime& InfiniteWorldRuntime::instance()
{
    static InfiniteWorldRuntime runtime;
    return runtime;
}

InfiniteWorldRuntime::~InfiniteWorldRuntime()
{
    shutdown();
}

void InfiniteWorldRuntime::shutdown()
{
    clearZone();
}

void InfiniteWorldRuntime::invalidateModeChunks()
{
    mModeChunks.clear();
    mModeCacheId.clear();
    mSignatureDirty = true;
}

nlohmann::json InfiniteWorldRuntime::statusJson() const
{
    return {
        {"active", mActive},
        {"zone", mZoneId},
        {"anchor", {mAnchorX, mAnchorY, mAnchorZ}},
        {"cell", {mLastCellX, mLastCellY, mLastCellZ}},
        {"radius", {mLastRadius.x, mLastRadius.y, mLastRadius.z}},
        {"desired", mDesired.size()},
        {"pending", mPendingOffsets.size()},
        {"loaded", mLoaded.size()},
        {"rejected", mRejected.size()},
        {"resolved", mResolved.size()},
        {"chunks", StreamedWorldLayer::instance().chunkCount()},
    };
}

void InfiniteWorldRuntime::reset()
{
    clearZone();
    mActive = false;
    mZoneId.clear();
    mHasCell = false;
    mHasGeneration = false;
    mHasRevision = false;
    mSignatureDirty = true;
    mAnchorX = 0;
    mAnchorY = 0;
    mAnchorZ = 0;
    mLastRadius = glm::ivec3(-1);
    mDesired.clear();
    mPendingOffsets.clear();
}

uint64_t InfiniteWorldRuntime::cellKey(int x, int y, int z) const
{
    return (uint64_t(uint32_t(x)) << 42) ^ (uint64_t(uint32_t(y)) << 21) ^
           uint64_t(uint32_t(z));
}

uint64_t InfiniteWorldRuntime::placementKey(const glm::mat4& transform,
                                            int /*chunkIndex*/) const
{
    const int64_t x = static_cast<int64_t>(std::llround(transform[3][0] * 100.0f));
    const int64_t y = static_cast<int64_t>(std::llround(transform[3][1] * 100.0f));
    const int64_t z = static_cast<int64_t>(std::llround(transform[3][2] * 100.0f));
    uint64_t hash = 1469598103934665603ull;
    const int64_t values[3] = {x, y, z};
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(values);
    for (size_t i = 0; i < sizeof(values); ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    hash ^= hash >> 30;
    hash *= 0xbf58476d1ce4e5b9ull;
    return hash ^ (hash >> 27);
}

bool InfiniteWorldRuntime::withinStreamRadius(const glm::vec3& playerPos,
                                              const glm::vec3& boundsMin,
                                              const glm::vec3& boundsMax,
                                              float margin) const
{
    const glm::vec3 closest = glm::clamp(playerPos, boundsMin, boundsMax);
    const float radius = static_cast<float>(std::max(
        {mModeZone.streamRadius.x, mModeZone.streamRadius.y,
         mModeZone.streamRadius.z, 1})) * margin;
    const glm::vec3 delta = closest - playerPos;
    return glm::dot(delta, delta) <= radius * radius;
}

void InfiniteWorldRuntime::applyStreamingConfig(
    const GenerationZone& zone, const GamemodeWorldStreaming& config)
{
    if (!config.enabled)
        return;
    const int visual = std::max(1, static_cast<int>(std::ceil(
        config.visualRadiusM)));
    if (zone.spacingMode == "connector") {
        mModeZone.streamRadius = glm::ivec3(visual, visual, 0);
        return;
    }
    const float cellX = std::max(0.001f, zone.cellSize.x);
    const float cellY = std::max(0.001f, zone.cellSize.y);
    const float cellZ = std::max(0.001f, zone.cellSize.z);
    mModeZone.streamRadius = glm::ivec3(
        std::max(1, static_cast<int>(std::ceil(config.visualRadiusM / cellX))),
        std::max(1, static_cast<int>(std::ceil(config.visualRadiusM / cellY))),
        zone.repeatAxes.z
            ? std::max(1, static_cast<int>(std::ceil(
                  config.visualRadiusM / cellZ)))
            : 0);
}

float InfiniteWorldRuntime::chunkDistance(
    const glm::vec3& point, const glm::vec3& boundsMin,
    const glm::vec3& boundsMax) const
{
    const glm::vec3 closest = glm::clamp(point, glm::min(boundsMin, boundsMax),
                                         glm::max(boundsMin, boundsMax));
    return glm::length(closest - point);
}

void InfiniteWorldRuntime::upgradeProxyChunks(
    const Player& player, const std::vector<const AuthoredChunk*>& candidates)
{
    if (!mStreaming.enabled)
        return;
    for (auto& entry : mConnectorLoaded) {
        ConnectorInstance& instance = entry.second;
        if (instance.chunkIndex < 0 ||
            static_cast<size_t>(instance.chunkIndex) >= candidates.size())
            continue;
        const AuthoredChunk* chunk = candidates[instance.chunkIndex];
        if (!chunk || chunk->parts.empty())
            continue;
        const float distance = chunkDistance(player.pos, instance.boundsMin,
                                             instance.boundsMax);
        if (!instance.proxy && mStreaming.farProxy &&
            distance > mStreaming.nearFullM * 1.5f) {
            StreamedWorldLayer::instance().addProxy(
                instance.key, *chunk, instance.transform, instance.boundsMin,
                instance.boundsMax);
            instance.proxy = true;
            continue;
        }
        if (!instance.proxy || distance > mStreaming.nearFullM)
            continue;
        StreamedWorldLayer::instance().addChunk(instance.key, *chunk,
                                                instance.transform);
        instance.proxy = false;
    }
    if (mProxyKeys.empty())
        return;
    for (auto it = mProxyKeys.begin(); it != mProxyKeys.end();) {
        const auto resolved = mResolved.find(*it);
        if (resolved == mResolved.end() || !resolved->second.valid) {
            it = mProxyKeys.erase(it);
            continue;
        }
        const ResolvedCell& cell = resolved->second;
        const float distance = chunkDistance(player.pos, cell.boundsMin,
                                             cell.boundsMax);
        if (distance > mStreaming.nearFullM) {
            ++it;
            continue;
        }
        const AuthoredChunk* chunk = nullptr;
        for (const AuthoredChunk* candidate : candidates)
            if (candidate && candidate->id == cell.chunkId) {
                chunk = candidate;
                break;
            }
        if (!chunk) {
            ++it;
            continue;
        }
        StreamedWorldLayer::instance().addChunk(*it, *chunk, cell.transform);
        it = mProxyKeys.erase(it);
    }
}

std::vector<AABB> InfiniteWorldRuntime::buildCoarseColliders(
    const Player& player, const GenerationZone& zone,
    const std::vector<const AuthoredChunk*>& candidates) const
{
    (void)zone;
    (void)candidates;
    std::vector<AABB> colliders;
    if (!mStreaming.enabled || !mStreaming.coarseCollision)
        return colliders;

    const float radius = mStreaming.collisionRadiusM;
    const float travel = glm::length(player.vel) *
        std::max(0.0f, mStreaming.lookaheadSeconds);
    const bool highSpeed = radius > 0.0f && travel > radius * 2.0f;
    if (!highSpeed &&
        StreamedWorldLayer::instance().hasFullCollisionNear(player.pos, radius))
        return colliders;

    const glm::vec3 end = player.pos +
        player.vel * std::max(0.0f, mStreaming.lookaheadSeconds);
    AABB corridor;
    corridor.min = glm::min(player.pos, end) - glm::vec3(2.0f, 2.0f, 1.0f);
    corridor.max = glm::max(player.pos, end) + glm::vec3(2.0f, 2.0f, 0.2f);
    if (zone.repeatAxes.z == 0) {
        corridor.min.z = player.pos.z - 1.0f;
        corridor.max.z = player.pos.z + 0.2f;
    }
    colliders.push_back(corridor);
    return colliders;
}

glm::vec3 InfiniteWorldRuntime::cellOrigin(int x, int y, int z,
                                           const GenerationZone& zone) const
{
    const bool haveOrigin = zone.origin.x != 0.0f || zone.origin.y != 0.0f ||
                            zone.origin.z != 0.0f;
    const glm::vec3 base = (haveOrigin || zone.modeId == "connected_world")
        ? zone.origin : zone.boundsMin;
    const glm::vec3 size(zone.cellSize.x > 0.0f ? zone.cellSize.x : 104.0f,
                         zone.cellSize.y > 0.0f ? zone.cellSize.y : 104.0f,
                         zone.cellSize.z > 0.0f ? zone.cellSize.z : 104.0f);
    return base + glm::vec3(static_cast<float>(x) * size.x,
                            static_cast<float>(y) * size.y,
                            static_cast<float>(z) * size.z);
}

int64_t InfiniteWorldRuntime::chunkCoord(const glm::vec3& point, int axis,
                                         const GenerationZone& zone) const
{
    const float cell = axis == 0 ? zone.cellSize.x :
                       axis == 1 ? zone.cellSize.y : zone.cellSize.z;
    const float base = zone.modeId == "connected_world" ? zone.origin[axis] :
                       axis == 0 ? (zone.origin.x != 0.0f ? zone.origin.x
                                                          : zone.boundsMin.x) :
                       axis == 1 ? (zone.origin.y != 0.0f ? zone.origin.y
                                                          : zone.boundsMin.y) :
                                   (zone.origin.z != 0.0f ? zone.origin.z
                                                          : zone.boundsMin.z);
    const float c = cell > 0.001f ? cell : 104.0f;
    return static_cast<int64_t>(std::floor((point[axis] - base) / c));
}

void InfiniteWorldRuntime::clearZone()
{
    StreamedWorldLayer::instance().clear();
    mLoaded.clear();
    mRejected.clear();
    mResolved.clear();
    mConnectorLoaded.clear();
    mConnectorTried.clear();
    mConnectorPending.clear();
    mConnectorSeeded = false;
    mProxyKeys.clear();
}

std::vector<const AuthoredChunk*> InfiniteWorldRuntime::resolveCandidates(
    const GenerationZone& zone)
{
    std::vector<const AuthoredChunk*> candidates;
    const MapObjectiveConfig& config = MapConfigRegistry::instance().current();
    for (const AuthoredChunk& chunk : config.chunks) {
        if (!chunk.enabled || chunk.parts.empty()) continue;
        if (!zone.chunkDependencies.empty() &&
            std::find(zone.chunkDependencies.begin(),
                      zone.chunkDependencies.end(),
                      chunk.id) == zone.chunkDependencies.end())
            continue;
        candidates.push_back(&chunk);
    }
    // Map-authored chunks remain the editable source for a zone. A named mode
    // contributes its reusable library chunks alongside them, with a map
    // chunk winning when both sources use the same stable ID.
    std::string modeId = zone.modeId.empty()
        ? (candidates.empty() ? "tunnel" : "") : zone.modeId;
    if (modeId == "connected_world" && !mModeProfile.id.empty())
        modeId = mModeProfile.id;
    if (modeId.empty()) return candidates;
    if (mModeCacheId != modeId || mSignatureDirty) {
        mModeChunks.clear();
        mModeCacheId = modeId;
        MimitaWorldGen::WorldGenMode mode;
        std::string error;
        if (MimitaWorldGen::WorldGenLibrary::instance().loadMode(modeId, mode,
                                                                 &error)) {
            MimitaWorldGen::WorldGenLibrary::instance().loadChunksForMode(
                mode, mModeChunks, &error);
        }
    }
    for (const AuthoredChunk& chunk : mModeChunks) {
        if (!chunk.enabled || chunk.parts.empty()) continue;
        const bool shadowedByMap = std::any_of(
            candidates.begin(), candidates.end(),
            [&chunk](const AuthoredChunk* authored) {
                return authored && authored->id == chunk.id;
            });
        if (!shadowedByMap) candidates.push_back(&chunk);
    }
    return candidates;
}

uint64_t InfiniteWorldRuntime::generationSignature(
    const GenerationZone& zone,
    const std::vector<const AuthoredChunk*>& candidates) const
{
    std::string signature;
    signatureString(signature, zone.id);
    signatureString(signature, zone.name);
    signatureVec3(signature, zone.boundsMin);
    signatureVec3(signature, zone.boundsMax);
    signatureString(signature, std::to_string(zone.seed));
    signatureString(signature, zone.generatorVersion);
    signatureString(signature, zone.ruleSet);
    signatureString(signature, depsIdentity(zone));
    signatureString(signature, zone.modeId);
    signatureString(signature, zone.plane);
    signatureString(signature, std::to_string(zone.repeatAxes.x) +
                                   std::to_string(zone.repeatAxes.y) +
                                   std::to_string(zone.repeatAxes.z));
    signatureVec3(signature, zone.origin);
    signatureVec3(signature, zone.cellSize);
    signatureVec3(signature, zone.chunkSizeVec);
    signatureString(signature, std::to_string(zone.streamRadius.x) +
                                   std::to_string(zone.streamRadius.y) +
                                   std::to_string(zone.streamRadius.z));
    signatureString(signature, zone.spacingMode);
    signatureFloat(signature, zone.connectorGap);
    signatureString(signature, std::to_string(zone.maxBranchDepth));
    signatureString(signature, zone.allowCycles ? "1" : "0");
    signatureString(signature, zone.allowCrossings ? "1" : "0");
    signatureString(signature, zone.enabled ? "1" : "0");

    // Material and UV-only edits are intentionally excluded so they refresh
    // loaded templates without a structural chunk reset.
    for (const AuthoredChunk* chunk : candidates) {
        if (!chunk) continue;
        signatureString(signature, chunk->id);
        signatureVec3(signature, chunk->boundsMin);
        signatureVec3(signature, chunk->boundsMax);
        signatureString(signature, chunk->enabled ? "1" : "0");
        for (const std::string& tag : chunk->tags)
            signatureString(signature, tag);
        for (const AuthoredChunkConnector& connector : chunk->connectors) {
            signatureString(signature, connector.id);
            signatureString(signature, connector.side);
            signatureString(signature, connector.orientation);
            signatureString(signature, connector.role);
            signatureVec3(signature, connector.position);
            for (const std::string& tag : connector.tags)
                signatureString(signature, tag);
            for (const std::string& link : connector.links)
                signatureString(signature, link);
        }
        for (const ChunkPart& part : chunk->parts) {
            signatureString(signature, part.id);
            signatureString(signature, part.kind);
            signatureVec3(signature, part.position);
            signatureVec3(signature, part.size);
            signatureVec3(signature, part.rotationEuler);
            signatureString(signature, part.collision ? "1" : "0");
            signatureString(signature, part.visible ? "1" : "0");
        }
    }
    return std::hash<std::string>{}(signature);
}

const InfiniteWorldRuntime::ResolvedCell& InfiniteWorldRuntime::resolveCell(
    int x, int y, int z, const GenerationZone& zone,
    const std::vector<const AuthoredChunk*>& candidates, bool independent)
{
    const uint64_t key = cellKey(x, y, z);
    auto found = mResolved.find(key);
    if (found != mResolved.end()) return found->second;

    ResolvedCell cell;
    cell.attempted = true;
    cell.transform = glm::translate(glm::mat4(1.0f), cellOrigin(x, y, z, zone));

    // A single flat tile has no graph choice. Resolve it from its cell alone,
    // so long travel does not recurse back through every previous cell.
    if (independent) {
        const uint64_t hash = mixCoord(zone.seed, zone.generatorVersion,
                                       depsIdentity(zone), x, y, z);
        cell.valid = true;
        cell.chunkId = candidates[hash % candidates.size()]->id;
        return mResolved.emplace(key, cell).first->second;
    }

    auto reject = [&](const char* reason, Side side = Side::None,
                      const AuthoredChunk* parentChunk = nullptr,
                      const AuthoredChunkConnector* parentConnector = nullptr)
        -> const ResolvedCell& {
        cell.valid = false;
        auto inserted = mResolved.emplace(key, cell);
        logEvent("world-stream.connector.missing", zone.id,
                 {{"zone", zone.id},
                  {"cell", {x, y, z}},
                  {"side", sideName(side)},
                  {"reason", reason},
                  {"parent_chunk", parentChunk ? parentChunk->id : ""},
                  {"parent_connector",
                   parentConnector ? parentConnector->id : ""}});
        cell.logged = true;
        inserted.first->second.logged = true;
        return inserted.first->second;
    };

    if (candidates.empty())
        return reject("no-candidates");

    const uint64_t hash = mixCoord(zone.seed, zone.generatorVersion,
                                   depsIdentity(zone), x, y, z);

    if (x == mAnchorX && y == mAnchorY && z == mAnchorZ) {
        const AuthoredChunk* chunk = candidates[hash % candidates.size()];
        cell.valid = chunk != nullptr && !chunk->parts.empty();
        cell.chunkId = chunk ? chunk->id : "";
        cell.depth = 0;
        auto inserted = mResolved.emplace(key, cell);
        return inserted.first->second;
    }

    int px = x;
    int py = y;
    int pz = z;
    if (zone.repeatAxes.x != 0 && x != mAnchorX)
        px = x + (x > mAnchorX ? -1 : 1);
    else if (zone.repeatAxes.y != 0 && y != mAnchorY)
        py = y + (y > mAnchorY ? -1 : 1);
    else if (zone.repeatAxes.z != 0 && z != mAnchorZ)
        pz = z + (z > mAnchorZ ? -1 : 1);
    else
        return reject("not-on-repeat-tree");

    const Side parentSide = sideBetween(px, py, pz, x, y, z);
    const Side childSide = oppositeSide(parentSide);
    if (parentSide == Side::None)
        return reject("no-parent-direction");

    const ResolvedCell parentCell = resolveCell(px, py, pz, zone, candidates,
                                                independent);
    if (!parentCell.valid) {
        cell.valid = false;
        auto inserted = mResolved.emplace(key, cell);
        return inserted.first->second;
    }
    cell.depth = parentCell.depth + 1;

    const AuthoredChunk* parentChunk = nullptr;
    for (const AuthoredChunk* candidate : candidates)
        if (candidate && candidate->id == parentCell.chunkId) {
            parentChunk = candidate;
            break;
        }
    const AuthoredChunkConnector* parentConnector =
        parentChunk ? connectorOnSide(*parentChunk, parentSide) : nullptr;

    if (zone.spacingMode == "connector" && !parentConnector)
        return reject("parent-has-no-connector", parentSide, parentChunk,
                      nullptr);

    bool linksRestrict = parentConnector && !parentConnector->links.empty();
    std::vector<std::pair<const AuthoredChunk*,
                          const AuthoredChunkConnector*>> options;
    for (const AuthoredChunk* candidate : candidates) {
        if (!candidate || candidate->parts.empty()) continue;
        if (parentConnector) {
            if (linksRestrict &&
                !linksAllow(*parentConnector, candidate->id, "")) {
                for (const AuthoredChunkConnector& connector :
                     candidate->connectors) {
                    if (sideFromName(connector.side) != childSide) continue;
                    if (!linksAllow(*parentConnector, candidate->id,
                                    connector.id))
                        continue;
                    if (!connector.links.empty() &&
                        !linksAllow(connector, parentChunk->id,
                                    parentConnector->id))
                        continue;
                    options.emplace_back(candidate, &connector);
                }
                continue;
            }
            for (const AuthoredChunkConnector& connector :
                 candidate->connectors) {
                if (sideFromName(connector.side) != childSide) continue;
                if (!connectorsCompatible(*parentConnector, connector))
                    continue;
                if (linksRestrict &&
                    !linksAllow(*parentConnector, candidate->id,
                                connector.id))
                    continue;
                if (!connector.links.empty() && parentChunk &&
                    !linksAllow(connector, parentChunk->id,
                                parentConnector->id))
                    continue;
                options.emplace_back(candidate, &connector);
            }
        } else if (zone.spacingMode != "connector") {
            options.emplace_back(candidate, nullptr);
        }
    }

    if (parentConnector && parentChunk) {
        std::vector<std::pair<const AuthoredChunk*,
                              const AuthoredChunkConnector*>> explicitOptions;
        for (const auto& option : options) {
            if (!option.first) continue;
            const bool parentLink = hasExplicitLink(
                *parentConnector, option.first->id,
                option.second ? option.second->id : "");
            const bool childLink = option.second && hasExplicitLink(
                *option.second, parentChunk->id, parentConnector->id);
            if (parentLink || childLink)
                explicitOptions.push_back(option);
        }
        if (!explicitOptions.empty()) options = std::move(explicitOptions);
    }

    // Cycles and crossings are evaluated against already-resolved neighbors.
    auto producesCycle = [&](const AuthoredChunk& candidate,
                             Side ignoredSide) {
        for (Side side : {Side::East, Side::West, Side::North, Side::South,
                          Side::Up, Side::Down}) {
            if (side == ignoredSide) continue;
            const bool repeated =
                (side == Side::East || side == Side::West)
                    ? zone.repeatAxes.x != 0
                    : (side == Side::North || side == Side::South)
                          ? zone.repeatAxes.y != 0
                          : zone.repeatAxes.z != 0;
            if (!repeated) continue;
            int nx = x;
            int ny = y;
            int nz = z;
            const glm::vec3 direction = sideDirection(side);
            nx += (int)direction.x;
            ny += (int)direction.y;
            nz += (int)direction.z;
            auto neighbor = mResolved.find(cellKey(nx, ny, nz));
            if (neighbor == mResolved.end() || !neighbor->second.valid)
                continue;
            if (neighbor->second.chunkId.empty()) continue;
            const AuthoredChunk* neighborChunk = nullptr;
            for (const AuthoredChunk* candidatePtr : candidates)
                if (candidatePtr &&
                    candidatePtr->id == neighbor->second.chunkId) {
                    neighborChunk = candidatePtr;
                    break;
                }
            if (!neighborChunk) continue;
            const AuthoredChunkConnector* neighborConnector =
                connectorOnSide(*neighborChunk, oppositeSide(side));
            const AuthoredChunkConnector* candidateConnector =
                connectorOnSide(candidate, side);
            if (neighborConnector && candidateConnector &&
                connectorsCompatible(*candidateConnector, *neighborConnector))
                return true;
        }
        return false;
    };

    auto hasTooManyConnectors = [&](const AuthoredChunk& candidate) {
        int count = 0;
        for (const AuthoredChunkConnector& connector : candidate.connectors)
            if (sideFromName(connector.side) != Side::None) ++count;
        return count > 2;
    };

    std::vector<std::pair<const AuthoredChunk*,
                          const AuthoredChunkConnector*>> filtered;
    for (const auto& option : options) {
        if (!option.first) continue;
        if (!zone.allowCycles && producesCycle(*option.first, childSide))
            continue;
        if (!zone.allowCrossings && hasTooManyConnectors(*option.first))
            continue;
        filtered.push_back(option);
    }

    if (filtered.empty() && zone.spacingMode != "connector" && !linksRestrict) {
        // Center spacing remains a filled uniform grid. Prefer a compatible
        // connector, but fall back to the deterministic hash candidate.
        const AuthoredChunk* fallback = candidates[hash % candidates.size()];
        if (fallback && !fallback->parts.empty())
            filtered.emplace_back(fallback, nullptr);
    }
    if (filtered.empty())
        return reject(zone.spacingMode == "connector"
                          ? "no-compatible-connector"
                          : "no-compatible-center-candidate",
                      parentSide, parentChunk, parentConnector);

    const auto& chosen = filtered[hash % filtered.size()];
    cell.valid = true;
    cell.chunkId = chosen.first->id;
    if (zone.spacingMode == "connector" && parentConnector && chosen.second) {
        const glm::vec3 parentWorld =
            glm::vec3(parentCell.transform *
                      glm::vec4(parentConnector->position, 1.0f));
        const glm::vec3 desired = parentWorld +
            sideDirection(parentSide) * zone.connectorGap;
        const glm::vec3 translation = desired - chosen.second->position;
        cell.transform = glm::translate(glm::mat4(1.0f), translation);
    }

    auto inserted = mResolved.emplace(key, cell);
    if (parentConnector && chosen.second) {
        logEvent("world-stream.connector.matched", zone.id,
                 {{"zone", zone.id},
                  {"cell", {x, y, z}},
                  {"parent_cell", {px, py, pz}},
                  {"side", sideName(parentSide)},
                  {"chunk", chosen.first->id},
                  {"parent_chunk", parentChunk ? parentChunk->id : ""},
                  {"connector", chosen.second->id},
                  {"parent_connector", parentConnector->id},
                  {"connector_radius", chosen.second->radius},
                  {"parent_connector_radius", parentConnector->radius},
                  {"depth", cell.depth},
                  {"spacing", zone.spacingMode}});
    }
    return inserted.first->second;
}

bool InfiniteWorldRuntime::placeChunk(
    int x, int y, int z, const GenerationZone& zone,
    const std::vector<const AuthoredChunk*>& candidates, bool independent)
{
    const uint64_t key = cellKey(x, y, z);
    if (mLoaded.count(key) || mRejected.count(key)) return false;
    const ResolvedCell& resolved = resolveCell(x, y, z, zone, candidates,
                                                independent);
    if (!resolved.valid) {
        mRejected.insert(key);
        return false;
    }
    const AuthoredChunk* chunk = nullptr;
    for (const AuthoredChunk* candidate : candidates)
        if (candidate && candidate->id == resolved.chunkId) {
            chunk = candidate;
            break;
        }
    if (!chunk || chunk->parts.empty()) {
        mRejected.insert(key);
        return false;
    }

    const auto applyStarted = std::chrono::steady_clock::now();
    const float distance = chunkDistance(mFocusPos, resolved.boundsMin,
                                         resolved.boundsMax);
    const bool proxy = mStreaming.enabled && mStreaming.farProxy &&
        distance > mStreaming.nearFullM;
    if (proxy) {
        StreamedWorldLayer::instance().addProxy(
            key, *chunk, resolved.transform, resolved.boundsMin,
            resolved.boundsMax);
        mProxyKeys.insert(key);
    } else {
        StreamedWorldLayer::instance().addChunk(key, *chunk,
                                                resolved.transform);
        mProxyKeys.erase(key);
    }
    mLoaded.insert(key);
    const double applyMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - applyStarted).count();
    if (applyMs >= 8.0) {
        logEvent("world-stream.chunk.slow-install", chunk->id,
                 {{"zone", zone.id}, {"chunk", chunk->id},
                  {"cell", {x, y, z}}, {"install_ms", applyMs},
                  {"parts", chunk->parts.size()},
                  {"reason", "render-upload-and-collision-registration"}});
    }
    return true;
}

void InfiniteWorldRuntime::tickConnectorStream(
    const Player& player, const GenerationZone& zone,
    const std::vector<const AuthoredChunk*>& candidates, uint64_t revision)
{
    if (candidates.empty())
        return;

    std::vector<mimita_worldgen::ChunkV1> chunkViews;
    std::vector<mimita_worldgen::PartV1> partViews;
    std::vector<mimita_worldgen::ConnectorV1> connectorViews;
    std::vector<uint32_t> connectorOffsets;
    chunkViews.reserve(candidates.size());
    connectorOffsets.reserve(candidates.size());
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
        view.partCount = static_cast<uint32_t>(partViews.size()) - view.partOffset;
        view.connectorCount = static_cast<uint32_t>(connectorViews.size()) -
                              view.connectorOffset;
        chunkViews.push_back(view);
    }
    if (chunkViews.empty())
        return;

    mimita_worldgen::TablesV1 tables{};
    tables.chunks = chunkViews.data();
    tables.chunkCount = static_cast<uint32_t>(chunkViews.size());
    tables.parts = partViews.data();
    tables.partCount = static_cast<uint32_t>(partViews.size());
    tables.connectors = connectorViews.data();
    tables.connectorCount = static_cast<uint32_t>(connectorViews.size());

    mimita_worldgen::ZoneV1 zoneView{};
    std::snprintf(zoneView.id, sizeof(zoneView.id), "%s", zone.id.c_str());
    std::snprintf(zoneView.plane, sizeof(zoneView.plane), "%s", zone.plane.c_str());
    std::snprintf(zoneView.spacingMode, sizeof(zoneView.spacingMode), "%s",
                  zone.spacingMode.c_str());
    std::snprintf(zoneView.generatorVersion, sizeof(zoneView.generatorVersion),
                  "%s", zone.generatorVersion.c_str());
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
    zoneView.origin[0] = zone.origin.x;
    zoneView.origin[1] = zone.origin.y;
    zoneView.origin[2] = zone.origin.z;
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

    auto placeInstance = [&](const mimita_worldgen::ResolveV1& resolved,
                             int depth) {
        if (!resolved.outValid || resolved.outChunk < 0 ||
            static_cast<size_t>(resolved.outChunk) >= chunkViews.size())
            return;
        const AuthoredChunk* chunk = candidates[resolved.outChunk];
        if (!chunk || chunk->parts.empty())
            return;
        glm::mat4 transform(1.0f);
        std::memcpy(&transform[0][0], resolved.outTransform, sizeof(float) * 16);
        const uint64_t key = placementKey(transform, resolved.outChunk);
        if (mConnectorLoaded.count(key) || mLoaded.count(key))
            return;
        ConnectorInstance instance;
        instance.key = key;
        instance.chunkIndex = resolved.outChunk;
        instance.transform = transform;
        instance.boundsMin = glm::vec3(resolved.outBoundsMin[0],
                                       resolved.outBoundsMin[1],
                                       resolved.outBoundsMin[2]);
        instance.boundsMax = glm::vec3(resolved.outBoundsMax[0],
                                       resolved.outBoundsMax[1],
                                       resolved.outBoundsMax[2]);
        instance.quarterTurns = resolved.outQuarterTurns;
        instance.depth = depth;
        StreamedWorldLayer::instance().addChunk(key, *chunk, transform);
        mLoaded.insert(key);
        mConnectorLoaded[key] = instance;
        mConnectorTried.insert(key);
        logEvent("world-stream.connector.matched", zone.id,
                 {{"zone", zone.id},
                  {"chunk", chunk->id},
                  {"cell", {instance.depth, 0, 0}},
                  {"quarter_turns", instance.quarterTurns},
                  {"transform",
                   {transform[3][0], transform[3][1], transform[3][2]}},
                  {"bounds_min", {instance.boundsMin.x, instance.boundsMin.y,
                                  instance.boundsMin.z}},
                  {"bounds_max", {instance.boundsMax.x, instance.boundsMax.y,
                                  instance.boundsMax.z}},
                  {"stream_distance_m", glm::length(
                       glm::clamp(player.pos, instance.boundsMin,
                                  instance.boundsMax) - player.pos)}});
    };

    if (!mConnectorSeeded || mConnectorLoaded.empty()) {
        const bool haveOrigin = zone.origin.x != 0.0f || zone.origin.y != 0.0f ||
                                zone.origin.z != 0.0f;
        const glm::vec3 seedPoint = haveOrigin ? zone.origin : player.pos;
        mimita_worldgen::ResolveV1 seed{};
        seed.tables = &tables;
        seed.zone = zoneView;
        seed.zone.origin[0] = seedPoint.x;
        seed.zone.origin[1] = seedPoint.y;
        seed.zone.origin[2] = seedPoint.z;
        seed.requestKind = 0;
        seed.cell[0] = seed.cell[1] = seed.cell[2] = 0;
        if (ModuleHost::instance().worldgenResolve(&seed) && seed.outValid) {
            mConnectorAnchor = seedPoint;
            mConnectorSeeded = true;
            placeInstance(seed, 0);
            logEvent("world-stream.anchor", zone.id,
                     {{"zone", zone.id},
                      {"seed_point", {seedPoint.x, seedPoint.y, seedPoint.z}},
                      {"mode", "connector-distance"},
                      {"stream_radius_m", {zone.streamRadius.x,
                                           zone.streamRadius.y,
                                           zone.streamRadius.z}}});
        }
    }

    for (auto it = mConnectorLoaded.begin(); it != mConnectorLoaded.end();) {
        if (withinStreamRadius(player.pos, it->second.boundsMin,
                               it->second.boundsMax, 1.25f)) {
            ++it;
            continue;
        }
        StreamedWorldLayer::instance().removeChunk(it->first);
        mLoaded.erase(it->first);
        mConnectorTried.erase(it->first);
        it = mConnectorLoaded.erase(it);
    }

    int added = 0;
    const int maxChunksPerFrame = mStreaming.enabled
        ? mStreaming.maxChunksPerFrame : kMaxChunksPerFrame;
    const size_t maxLoadedChunks = mStreaming.enabled
        ? static_cast<size_t>(mStreaming.maxLoadedChunks)
        : kMaxConnectorLoaded;
    const size_t maxPendingChunks = mStreaming.enabled
        ? static_cast<size_t>(mStreaming.maxPendingChunks)
        : kMaxConnectorPending;
    const auto installStarted = std::chrono::steady_clock::now();
    for (size_t i = 0;
         i < mConnectorPending.size() && added < maxChunksPerFrame &&
         mConnectorLoaded.size() < maxLoadedChunks;) {
        if (mStreaming.enabled && mStreaming.installBudgetMs > 0.0f) {
            const double elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - installStarted).count();
            if (elapsed >= mStreaming.installBudgetMs)
                break;
        }
        const ConnectorInstance pending = mConnectorPending[i];
        mConnectorPending.erase(mConnectorPending.begin() + i);
        if (mConnectorLoaded.count(pending.key) || mLoaded.count(pending.key))
            continue;
        if (!withinStreamRadius(player.pos, pending.boundsMin,
                                pending.boundsMax))
            continue;
        const AuthoredChunk* chunk = candidates[pending.chunkIndex];
        if (!chunk || chunk->parts.empty())
            continue;
        const float distance = chunkDistance(player.pos, pending.boundsMin,
                                             pending.boundsMax);
        const bool proxy = mStreaming.enabled && mStreaming.farProxy &&
            distance > mStreaming.nearFullM;
        if (proxy) {
            StreamedWorldLayer::instance().addProxy(
                pending.key, *chunk, pending.transform, pending.boundsMin,
                pending.boundsMax);
        } else {
            StreamedWorldLayer::instance().addChunk(pending.key, *chunk,
                                                    pending.transform);
        }
        ConnectorInstance loaded = pending;
        loaded.proxy = proxy;
        mLoaded.insert(pending.key);
        mConnectorLoaded[pending.key] = loaded;
        mConnectorTried.insert(pending.key);
        logEvent("world-stream.connector.matched", zone.id,
                 {{"zone", zone.id},
                  {"chunk", chunk->id},
                  {"quarter_turns", pending.quarterTurns},
                  {"transform", {pending.transform[3][0],
                                 pending.transform[3][1],
                                 pending.transform[3][2]}},
                  {"bounds_min", {pending.boundsMin.x, pending.boundsMin.y,
                                  pending.boundsMin.z}},
                  {"bounds_max", {pending.boundsMax.x, pending.boundsMax.y,
                                  pending.boundsMax.z}},
                  {"depth", pending.depth},
                  {"stream_distance_m", glm::length(
                       glm::clamp(player.pos, pending.boundsMin,
                                  pending.boundsMax) - player.pos)}});
        ++added;
    }

    const size_t attempts = mConnectorPending.size();
    std::vector<ConnectorInstance> loadedSnapshot;
    if (mConnectorLoaded.size() < maxLoadedChunks &&
        mConnectorPending.size() < maxPendingChunks) {
        loadedSnapshot.reserve(mConnectorLoaded.size());
        for (const auto& pair : mConnectorLoaded)
            if (withinStreamRadius(player.pos, pair.second.boundsMin,
                                   pair.second.boundsMax))
                loadedSnapshot.push_back(pair.second);
    }

    for (const ConnectorInstance& instance : loadedSnapshot) {
        if (mConnectorPending.size() >= maxPendingChunks)
            break;
        if (instance.chunkIndex < 0 ||
            static_cast<size_t>(instance.chunkIndex) >= candidates.size())
            continue;
        const AuthoredChunk* parent = candidates[instance.chunkIndex];
        if (!parent)
            continue;
        for (size_t connectorIndex = 0;
             connectorIndex < parent->connectors.size(); ++connectorIndex) {
            mimita_worldgen::ResolveV1 request{};
            request.tables = &tables;
            request.zone = zoneView;
            request.requestKind = 1;
            request.parentChunk = instance.chunkIndex;
            request.parentConnector = static_cast<int>(
                connectorOffsets[instance.chunkIndex] + connectorIndex);
            std::memcpy(request.parentTransform, &instance.transform[0][0],
                        sizeof(float) * 16);
            if (!ModuleHost::instance().worldgenResolve(&request) ||
                !request.outValid) {
                if (request.reason[0])
                    logEvent("world-stream.connector.missing", zone.id,
                             {{"zone", zone.id},
                              {"parent_chunk", parent->id},
                              {"parent_connector",
                               parent->connectors[connectorIndex].id},
                              {"reason", request.reason}});
                continue;
            }
            if (request.outChunk < 0 ||
                static_cast<size_t>(request.outChunk) >= candidates.size())
                continue;
            glm::mat4 transform(1.0f);
            std::memcpy(&transform[0][0], request.outTransform,
                        sizeof(float) * 16);
            const uint64_t key = placementKey(transform, request.outChunk);
            if (mConnectorLoaded.count(key) || mLoaded.count(key) ||
                mConnectorTried.count(key))
                continue;
            const glm::vec3 boundsMin(request.outBoundsMin[0],
                                      request.outBoundsMin[1],
                                      request.outBoundsMin[2]);
            const glm::vec3 boundsMax(request.outBoundsMax[0],
                                      request.outBoundsMax[1],
                                      request.outBoundsMax[2]);
            if (!withinStreamRadius(player.pos, boundsMin, boundsMax))
                continue;
            ConnectorInstance child;
            child.key = key;
            child.chunkIndex = request.outChunk;
            child.transform = transform;
            child.boundsMin = boundsMin;
            child.boundsMax = boundsMax;
            child.quarterTurns = request.outQuarterTurns;
            child.depth = instance.depth + 1;
            mConnectorTried.insert(key);
            mConnectorPending.push_back(child);
        }
    }

    const int hx = static_cast<int>(std::floor(
        (player.pos.x - mConnectorAnchor.x) /
        std::max(1.0f, zone.chunkSizeVec.x)));
    const int hy = static_cast<int>(std::floor(
        (player.pos.y - mConnectorAnchor.y) /
        std::max(1.0f, zone.chunkSizeVec.y)));
    const int hz = static_cast<int>(std::floor(
        (player.pos.z - mConnectorAnchor.z) /
        std::max(1.0f, zone.chunkSizeVec.z)));
    const bool cellChanged = !mHasCell || hx != mLastCellX || hy != mLastCellY ||
                             hz != mLastCellZ;
    if (cellChanged) {
        mLastCellX = hx;
        mLastCellY = hy;
        mLastCellZ = hz;
        mHasCell = true;
    }
    if (cellChanged || added > 0) {
        logEvent("world-stream.update", zone.id,
                 {{"zone", zone.id},
                  {"mode", "connector-distance"},
                  {"cell", {hx, hy, hz}},
                  {"player_position", {player.pos.x, player.pos.y, player.pos.z}},
                  {"anchored", std::to_string(mConnectorSeeded) == "1"},
                  {"added", added},
                  {"pending", mConnectorPending.size()},
                  {"attempted", attempts},
                  {"budget", kMaxChunksPerFrame},
                  {"loaded", mConnectorLoaded.size()},
                  {"radius_m", {zone.streamRadius.x, zone.streamRadius.y,
                                zone.streamRadius.z}},
                  {"revision", revision}});
    }
}

void InfiniteWorldRuntime::tick(const Player& player, World& world)
{
    StreamedWorldLayer& layer = StreamedWorldLayer::instance();
    static bool defaultsReady = false;
    if (!defaultsReady) {
        std::string error;
        MimitaWorldGen::WorldGenLibrary::instance().ensureDefaults(&error);
        defaultsReady = true;
    }

    MapConfigRegistry& registry = MapConfigRegistry::instance();
    const MapObjectiveConfig& config = registry.current();
    const uint64_t revision = registry.revision();
    const std::string& gameModeId =
        MimitaNet::CommunityMatchClient::instance().mode();
    mStreaming = GamemodeRegistry::instance().get(gameModeId).worldStreaming;
    mFocusPos = player.pos;
    mFocusVel = player.vel;
    layer.setStreamFocus(
        player.pos, mStreaming.enabled ? mStreaming.collisionRadiusM : 0.0f);

    // The gamemode is the sole owner of whether chunk streaming exists. A map
    // zone or generateWholeWorld flag must never opt a non-connected gamemode
    // into streaming. This also keeps streamed collision out of ordinary maps.
    if (!mStreaming.enabled) {
        static std::string lastDisabledMode;
        if (lastDisabledMode != gameModeId) {
            lastDisabledMode = gameModeId;
            logEvent("world-stream.disabled", gameModeId,
                     {{"mode", gameModeId},
                      {"map", config.mapId},
                      {"reason", "gamemode_world_streaming_disabled"}});
        }
        if (mActive) reset();
        layer.flushCollision(world);
        return;
    }

    // A chunk collision compose that exceeds its frame budget pauses new chunk
    // installation for a short cooldown. Dirty collision remains queued and is
    // flushed after the cooldown instead of compounding a frame stall.
    if (layer.collisionCooldownActive())
        return;

    const bool connectedWorldMode = gameModeId == "connected_world";

    GenerationZone fallbackZone;
    const GenerationZone* mapZone = nullptr;
    for (const auto& zone : config.generationZones) {
        if (zone.enabled &&
            (connectedWorldMode || zoneContains(zone, player.pos))) {
            mapZone = &zone;
            break;
        }
    }
    if (!mapZone && connectedWorldMode) {
        fallbackZone.id = "connected_world";
        fallbackZone.name = "Connected World";
        fallbackZone.modeId = "connected_world";
        fallbackZone.enabled = true;
        for (const AuthoredChunk& chunk : config.chunks) {
            if (!chunk.enabled) continue;
            fallbackZone.boundsMin = glm::min(fallbackZone.boundsMin,
                                              chunk.boundsMin);
            fallbackZone.boundsMax = glm::max(fallbackZone.boundsMax,
                                              chunk.boundsMax);
        }
        fallbackZone.origin = fallbackZone.boundsMin;
        fallbackZone.cellSize = glm::vec3(104.0f);
        fallbackZone.chunkSizeVec = glm::vec3(104.0f);
        fallbackZone.repeatAxes = glm::ivec3(1, 1, 0);
        fallbackZone.streamRadius = glm::ivec3(1, 1, 0);
        fallbackZone.spacingMode = "center";
        mapZone = &fallbackZone;
        logEvent("world-stream.synthetic-zone", fallbackZone.id,
                 {{"map", config.mapId},
                  {"reason", "connected_world_without_map_zone"}});
    }
    if (!mapZone) {
        if (mActive) reset();
        layer.flushCollision(world);
        return;
    }

    const GenerationZone* activeZone = mapZone;
    if (connectedWorldMode) {
        const std::string mapId = config.mapId.empty()
            ? "connected_world" : config.mapId;
        std::string profileId = mapId;
        std::string error;
        bool profileApplied = false;
        if (mModeProfile.id.empty() || mModeProfile.id != mapId) {
            if (!MimitaWorldGen::WorldGenLibrary::instance().loadMode(
                    profileId, mModeProfile, &error)) {
                profileId = "connected_world";
                error.clear();
                if (!MimitaWorldGen::WorldGenLibrary::instance().loadMode(
                        profileId, mModeProfile, &error)) {
                    logEvent("world-stream.mode-profile.missing", profileId,
                             {{"mode", profileId}, {"error", error},
                              {"map", mapId}});
                    if (mActive) reset();
                    layer.flushCollision(world);
                    return;
                }
            }
            mModeProfile.id = profileId;
            std::vector<AuthoredChunk> chunks;
            if (!MimitaWorldGen::WorldGenLibrary::instance().loadChunksForMode(
                    mModeProfile, chunks, &error)) {
                logEvent("world-stream.mode-profile.missing", profileId,
                         {{"mode", profileId}, {"error", error},
                          {"map", mapId}});
                if (mActive) reset();
                layer.flushCollision(world);
                return;
            }
            mModeChunks = std::move(chunks);
            mModeCacheId = profileId;
            ++mModeProfileRevision;
            mSignatureDirty = true;
            profileApplied = true;
        } else {
            const uint64_t nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            if (nowMs - mLastProfilePollMs >= 250) {
                mLastProfilePollMs = nowMs;
                bool changed = false;
                if (MimitaWorldGen::WorldGenLibrary::instance().pollReload(
                        profileId, mModeProfile, mModeChunks, &changed,
                        &error) && changed) {
                    mModeProfile.id = profileId;
                    mModeCacheId = profileId;
                    ++mModeProfileRevision;
                    mSignatureDirty = true;
                    profileApplied = true;
                    logEvent("world-stream.profile.reloaded", profileId,
                             {{"profile", profileId},
                              {"revision", mModeProfileRevision}});
                }
            }
        }

        if (profileApplied || mModeZone.id != mapZone->id) {
            mModeZone = *mapZone;
            mModeZone.modeId = "connected_world";
            mModeZone.chunkDependencies.clear();
            mModeZone.plane = mModeProfile.grid.plane;
            mModeZone.repeatAxes = mModeProfile.grid.repeatAxes;
            mModeZone.origin = mModeProfile.grid.origin;
            mModeZone.cellSize = mModeProfile.grid.cellSize;
            mModeZone.chunkSizeVec = mModeProfile.grid.chunkSize;
            mModeZone.streamRadius = mModeProfile.grid.streamRadius;
            mModeZone.spacingMode = mModeProfile.grid.spacingMode;
            mModeZone.connectorGap = mModeProfile.grid.connectorGap;
            mModeZone.maxBranchDepth = mModeProfile.maxBranchDepth;
            mModeZone.allowCycles = mModeProfile.allowCycles;
            mModeZone.allowCrossings = mModeProfile.allowCrossings;
            const std::string descriptorIdentity = mapId + "|" +
                std::to_string(mapZone->seed) + "|" +
                mapZone->generatorVersion + "|" + mapZone->ruleSet + "|" +
                depsIdentity(*mapZone);
            logEvent("world-stream.identity", profileId,
                     {{"side", "client"}, {"map", mapId},
                      {"seed", mapZone->seed},
                      {"generator_version", mapZone->generatorVersion},
                      {"descriptor_hash",
                       std::hash<std::string>{}(descriptorIdentity)}});
            logEvent("world-stream.profile", profileId,
                     {{"map", mapId}, {"profile", profileId},
                      {"seed", mapZone->seed},
                      {"profile_revision", mModeProfileRevision},
                      {"cell_size", {mModeZone.cellSize.x, mModeZone.cellSize.y,
                                     mModeZone.cellSize.z}},
                      {"chunk_size", {mModeZone.chunkSizeVec.x,
                                      mModeZone.chunkSizeVec.y,
                                      mModeZone.chunkSizeVec.z}},
                      {"repeat_axes", {mModeZone.repeatAxes.x,
                                       mModeZone.repeatAxes.y,
                                       mModeZone.repeatAxes.z}},
                      {"stream_radius", {mModeZone.streamRadius.x,
                                         mModeZone.streamRadius.y,
                                         mModeZone.streamRadius.z}},
                      {"stream_radius_units",
                       mModeZone.spacingMode == "connector" ? "meters" : "cells"},
                      {"spacing_mode", mModeZone.spacingMode}});
        }
        activeZone = &mModeZone;
        applyStreamingConfig(*activeZone, mStreaming);
    }

    if (!mActive || mZoneId != activeZone->id) {
        reset();
        mActive = true;
        mZoneId = activeZone->id;
        const bool haveOrigin = activeZone->origin.x != 0.0f ||
                                activeZone->origin.y != 0.0f ||
                                activeZone->origin.z != 0.0f;
        if (haveOrigin) {
            mAnchorX = (int)chunkCoord(activeZone->origin, 0, *activeZone);
            mAnchorY = (int)chunkCoord(activeZone->origin, 1, *activeZone);
            mAnchorZ = activeZone->repeatAxes.z
                ? (int)chunkCoord(activeZone->origin, 2, *activeZone) : 0;
        } else {
            mAnchorX = (int)chunkCoord(player.pos, 0, *activeZone);
            mAnchorY = (int)chunkCoord(player.pos, 1, *activeZone);
            mAnchorZ = activeZone->repeatAxes.z
                ? (int)chunkCoord(player.pos, 2, *activeZone) : 0;
        }
        logEvent("world-stream.anchor", activeZone->id,
                 {{"zone", activeZone->id},
                  {"cell", {mAnchorX, mAnchorY, mAnchorZ}},
                  {"origin", {activeZone->origin.x, activeZone->origin.y,
                              activeZone->origin.z}},
                  {"whole_world", config.generateWholeWorld ||
                                      connectedWorldMode},
                  {"from_player", !haveOrigin}});
    }

    std::vector<const AuthoredChunk*> candidates =
        resolveCandidates(*activeZone);
    if (candidates.empty()) {
        layer.flushCollision(world);
        return;
    }
    const bool independent = independentGrid(*activeZone, candidates);

    const bool needSignature = !mHasGeneration || mSignatureDirty ||
                               revision != mLastConfigRevision;
    if (needSignature) {
        const uint64_t signature =
            generationSignature(*activeZone, candidates);
        if (!mHasGeneration || signature != mGenerationSignature) {
            StreamedWorldLayer::instance().clear();
            mLoaded.clear();
            mRejected.clear();
            mResolved.clear();
            mConnectorLoaded.clear();
            mConnectorTried.clear();
            mConnectorPending.clear();
            mConnectorSeeded = false;
            mHasCell = false;
            mLastRadius = glm::ivec3(-1);
            mDesired.clear();
            mPendingOffsets.clear();
            mProxyKeys.clear();
            mGenerationSignature = signature;
            mHasGeneration = true;
            logEvent("world-stream.reset", activeZone->id,
                     {{"zone", activeZone->id},
                      {"reason", "generation-signature-changed"},
                      {"revision", revision}});
        }
        mSignatureDirty = false;
        mLastConfigRevision = revision;
    }

    if (activeZone->spacingMode == "connector") {
        tickConnectorStream(player, *activeZone, candidates, revision);
        upgradeProxyChunks(player, candidates);
        layer.setCoarseColliders(
            buildCoarseColliders(player, *activeZone, candidates));
        layer.flushCollision(world);
        return;
    }

    const int hx = (int)chunkCoord(player.pos, 0, *activeZone);
    const int hy = (int)chunkCoord(player.pos, 1, *activeZone);
    const int hz = activeZone->repeatAxes.z
        ? (int)chunkCoord(player.pos, 2, *activeZone) : 0;

    const bool cellChanged = !mHasCell || hx != mLastCellX ||
                             hy != mLastCellY || hz != mLastCellZ;
    if (cellChanged) {
        mLastCellX = hx;
        mLastCellY = hy;
        mLastCellZ = hz;
        mHasCell = true;
    }

    const glm::ivec3 radius(activeZone->streamRadius.x,
                            activeZone->streamRadius.y,
                            activeZone->repeatAxes.z
                                ? activeZone->streamRadius.z : 0);

    // Cache the desired neighborhood and its distance-sorted install order.
    // Rebuild only when the player crosses a cell or the radius changes; the
    // install pass below still services a few cells every frame.
    const int rz = std::clamp(radius.z, 0, 3);
    if (cellChanged || radius != mLastRadius) {
        mLastRadius = radius;
        mDesired.clear();
        mPendingOffsets.clear();
        for (int dz = -rz; dz <= rz; ++dz)
            for (int dy = -radius.y; dy <= radius.y; ++dy)
                for (int dx = -radius.x; dx <= radius.x; ++dx) {
                    const int x = hx + dx;
                    const int y = hy + dy;
                    const int z = hz + dz;
                    mDesired.insert(cellKey(x, y, z));
                    mPendingOffsets.emplace_back(x, y, z);
                }
        std::sort(mPendingOffsets.begin(), mPendingOffsets.end(), [hx, hy, hz](
            const glm::ivec3& a, const glm::ivec3& b) {
            const int ad = std::abs(a.x - hx) + std::abs(a.y - hy) +
                           std::abs(a.z - hz);
            const int bd = std::abs(b.x - hx) + std::abs(b.y - hy) +
                           std::abs(b.z - hz);
            if (ad != bd) return ad < bd;
            if (a.z != b.z) return a.z < b.z;
            if (a.y != b.y) return a.y < b.y;
            return a.x < b.x;
        });
    }

    // Remove cells that left the radius.
    std::vector<uint64_t> toRemove;
    for (uint64_t key : mLoaded)
        if (!mDesired.count(key)) toRemove.push_back(key);
    for (uint64_t key : toRemove) {
        StreamedWorldLayer::instance().removeChunk(key);
        mLoaded.erase(key);
    }
    if (independent) {
        for (auto it = mResolved.begin(); it != mResolved.end();)
            it = mDesired.count(it->first) ? std::next(it) : mResolved.erase(it);
        for (auto it = mRejected.begin(); it != mRejected.end();)
            it = mDesired.count(*it) ? std::next(it) : mRejected.erase(it);
    }

    int added = 0;
    size_t attempted = 0;
    const int maxChunksPerFrame = mStreaming.enabled
        ? mStreaming.maxChunksPerFrame : kMaxChunksPerFrame;
    const auto installStarted = std::chrono::steady_clock::now();
    for (const glm::ivec3& cell : mPendingOffsets) {
        if (added >= maxChunksPerFrame) break;
        if (mStreaming.enabled && mStreaming.installBudgetMs > 0.0f) {
            const double elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - installStarted).count();
            if (elapsed >= mStreaming.installBudgetMs)
                break;
        }
        const uint64_t key = cellKey(cell.x, cell.y, cell.z);
        if (mLoaded.count(key) || mRejected.count(key)) continue;
        ++attempted;
        if (placeChunk(cell.x, cell.y, cell.z, *activeZone, candidates,
                       independent))
            ++added;
    }

    if (cellChanged || added > 0 || !toRemove.empty())
        logEvent("world-stream.update", activeZone->id,
                 {{"zone", activeZone->id},
                  {"plane", activeZone->plane},
                  {"cell", {hx, hy, hz}},
                  {"player_position", {player.pos.x, player.pos.y,
                                       player.pos.z}},
                  {"anchor", {mAnchorX, mAnchorY, mAnchorZ}},
                  {"spacing_mode", activeZone->spacingMode},
                  {"independent_grid", independent},
                  {"whole_world", config.generateWholeWorld ||
                                      connectedWorldMode},
                  {"added", added},
                  {"pending", mPendingOffsets.size() - attempted},
                  {"attempted", attempted},
                  {"budget", kMaxChunksPerFrame},
                  {"removed", toRemove.size()},
                  {"loaded", mLoaded.size()},
                  {"rejected", mRejected.size()},
                  {"resolved", mResolved.size()},
                  {"desired", mDesired.size()},
                  {"radius", {radius.x, radius.y, radius.z}}});
    upgradeProxyChunks(player, candidates);
    layer.setCoarseColliders(
        buildCoarseColliders(player, *activeZone, candidates));
    layer.flushCollision(world);
}

void registerInfiniteWorldCommands()
{
    Terminal::instance().registerCommand({
        "infiniteworld",
        "List, inspect, or request a named Infinite World",
        "infiniteworld <list|status|load <world>>",
        [](const std::vector<std::string>& args) {
            Terminal& terminal = Terminal::instance();
            if (args.empty() || args[0] == "list") {
                std::ifstream file("config/gamemodes/connected_world.json");
                if (!file.is_open()) {
                    terminal.addLog("[INFINITEWORLD] gamemode config missing");
                    return;
                }
                try {
                    const nlohmann::json root = nlohmann::json::parse(file);
                    terminal.addLog("[INFINITEWORLD] named worlds:");
                    if (root.contains("maps") && root["maps"].is_array()) {
                        for (const auto& item : root["maps"]) {
                            if (!item.is_string()) continue;
                            std::string id = item.get<std::string>();
                            std::string display = id;
                            const std::string prefix = "infinite_";
                            if (display.rfind(prefix, 0) == 0)
                                display = display.substr(prefix.size());
                            terminal.addLog("[INFINITEWORLD]   " + display +
                                            " -> " + id);
                        }
                    }
                } catch (const std::exception& e) {
                    terminal.addLog(std::string("[INFINITEWORLD] parse error: ") +
                                    e.what());
                }
                return;
            }
            if (args[0] == "status") {
                const nlohmann::json status =
                    InfiniteWorldRuntime::instance().statusJson();
                terminal.addLog("[INFINITEWORLD] map=" +
                    MapConfigRegistry::instance().mapId() +
                    " " + status.dump());
                return;
            }
            if (args[0] == "load" && args.size() >= 2) {
                std::string mapId = args[1];
                if (mapId.rfind("infinite_", 0) != 0)
                    mapId = "infinite_" + mapId;
                if (::gpMpContext && ::gpMpContext->active) {
                    MimitaNet::mpSendServerCommand(*::gpMpContext,
                                                   "changemap " + mapId);
                } else if (MimitaNet::isServerHost()) {
                    MimitaNet::serverGamemodeRequestMapChange(mapId);
                } else {
                    terminal.addLog(
                        "[INFINITEWORLD] load requires the host or an active "
                        "multiplayer connection");
                    return;
                }
                logEvent("infiniteworld.load.requested", mapId,
                         {{"map", mapId}, {"host_only_request", true}});
                terminal.addLog("[INFINITEWORLD] requested load " + mapId +
                                " (server acceptance not yet proven)");
                return;
            }
            terminal.addLog(
                "[INFINITEWORLD] list | status | load <world>");
        },
        "2026-11-07", CommandCategory::Debug
    });
    Terminal::instance().registerCommand({
        "worldstream",
        "Control streamed chunk rendering backend and budget",
        "worldstream <status|render <instancing|vbo>|clear>",
        [](const std::vector<std::string>& args) {
            Terminal& terminal = Terminal::instance();
            StreamedWorldLayer& layer = StreamedWorldLayer::instance();
            if (args.empty() || args[0] == "status") {
                terminal.addLog(std::string("[WORLDSTREAM] backend=") +
                                (layer.instancing() ? "instancing" : "vbo") +
                                " chunks=" + std::to_string(layer.chunkCount()) +
                                " collided=" +
                                std::to_string(layer.collidedChunkCount()) +
                                " collision_triangles=" +
                                std::to_string(layer.collisionTriangleCount()) +
                                " collision_revision=" +
                                std::to_string(layer.collisionRevision()) +
                                " dirty=" + (layer.collisionDirty() ? "1" : "0"));
                return;
            }
            if (args[0] == "render" && args.size() >= 2) {
                if (args[1] == "instancing") layer.setInstancing(true);
                else if (args[1] == "vbo") layer.setInstancing(false);
                else { terminal.addLog("[WORLDSTREAM] render instancing|vbo"); return; }
                terminal.addLog(std::string("[WORLDSTREAM] backend=") +
                                (layer.instancing() ? "instancing" : "vbo"));
                return;
            }
            if (args[0] == "clear") {
                layer.clear();
                terminal.addLog("[WORLDSTREAM] cleared");
                return;
            }
            terminal.addLog("[WORLDSTREAM] status|render <instancing|vbo>|clear");
        },
        "2026-11-05", CommandCategory::Debug
    });
}

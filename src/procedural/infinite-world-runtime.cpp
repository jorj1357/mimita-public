// 2026-11-05
#include "procedural/infinite-world-runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "entities/player.h"
#include "gamemode/map-config.h"
#include "procedural/procedural-world.h"
#include "world/streamed-world-layer.h"
#include "worldgen/worldgen-library.h"

namespace {

constexpr int kMaxChunksPerFrame = 8;

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

void logEvent(const char* name, const std::string& subject,
              const nlohmann::json& fields)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important, name, subject,
        "authored infinite-world streaming", 0, fields, __FILE__, __LINE__,
        __func__);
}

} // namespace

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

void InfiniteWorldRuntime::reset()
{
    clearZone();
    mActive = false;
    mZoneId.clear();
    mHasCell = false;
    mHasRevision = false;
}

uint64_t InfiniteWorldRuntime::cellKey(int x, int y, int z) const
{
    return (uint64_t(uint32_t(x)) << 42) ^ (uint64_t(uint32_t(y)) << 21) ^
           uint64_t(uint32_t(z));
}

glm::vec3 InfiniteWorldRuntime::cellOrigin(int x, int y, int z,
                                           const GenerationZone& zone) const
{
    const bool haveOrigin = zone.origin.x != 0.0f || zone.origin.y != 0.0f ||
                            zone.origin.z != 0.0f;
    const glm::vec3 base = haveOrigin ? zone.origin : zone.boundsMin;
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
    const float base = axis == 0 ? (zone.origin.x != 0.0f ? zone.origin.x
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
}

void InfiniteWorldRuntime::placeChunk(
    int x, int y, int z, const GenerationZone& zone,
    const std::vector<const AuthoredChunk*>& candidates)
{
    const uint64_t key = cellKey(x, y, z);
    if (mLoaded.count(key)) return;
    const std::string deps = depsIdentity(zone);
    const uint64_t hash = mixCoord(zone.seed, zone.generatorVersion, deps,
                                   x, y, z);
    const AuthoredChunk* chunk = candidates[hash % candidates.size()];
    if (!chunk || chunk->parts.empty()) return;

    const glm::vec3 origin = cellOrigin(x, y, z, zone);
    StreamedWorldLayer::instance().addChunk(key, *chunk,
                                            glm::translate(glm::mat4(1.0f),
                                                           origin));
    mLoaded.insert(key);
}

void InfiniteWorldRuntime::tick(const Player& player, World&)
{
    static bool defaultsReady = false;
    if (!defaultsReady) {
        std::string error;
        MimitaWorldGen::WorldGenLibrary::instance().ensureDefaults(&error);
        defaultsReady = true;
    }

    MapConfigRegistry& registry = MapConfigRegistry::instance();
    const MapObjectiveConfig& config = registry.current();
    const uint64_t revision = registry.revision();
    if (mHasRevision && revision != mLastConfigRevision) {
        reset();
    }
    if (!mHasRevision) {
        mLastConfigRevision = revision;
        mHasRevision = true;
    }

    const GenerationZone* activeZone = nullptr;
    for (const auto& zone : config.generationZones) {
        if (zone.enabled && zoneContains(zone, player.pos)) {
            activeZone = &zone;
            break;
        }
    }
    if (!activeZone) {
        if (mActive) reset();
        return;
    }

    if (!mActive || mZoneId != activeZone->id) {
        reset();
        mActive = true;
        mZoneId = activeZone->id;
    }

    // Resolve chunk candidates: map-authored chunks, else mode library.
    std::vector<const AuthoredChunk*> candidates;
    for (const auto& chunk : config.chunks) {
        if (!chunk.enabled || chunk.parts.empty()) continue;
        if (!activeZone->chunkDependencies.empty() &&
            std::find(activeZone->chunkDependencies.begin(),
                      activeZone->chunkDependencies.end(),
                      chunk.id) == activeZone->chunkDependencies.end())
            continue;
        candidates.push_back(&chunk);
    }
    if (candidates.empty()) {
        MimitaWorldGen::WorldGenMode mode;
        const std::string modeId = activeZone->modeId.empty()
            ? "tunnel" : activeZone->modeId;
        std::string error;
        if (MimitaWorldGen::WorldGenLibrary::instance().loadMode(modeId, mode,
                                                                 &error)) {
            static std::vector<AuthoredChunk> modeChunks;
            modeChunks.clear();
            if (MimitaWorldGen::WorldGenLibrary::instance()
                    .loadChunksForMode(mode, modeChunks, &error)) {
                for (const auto& chunk : modeChunks)
                    if (chunk.enabled && !chunk.parts.empty())
                        candidates.push_back(&chunk);
            }
        }
    }
    if (candidates.empty()) return;

    const int hx = chunkCoord(player.pos, 0, *activeZone);
    const int hy = chunkCoord(player.pos, 1, *activeZone);
    const int hz = activeZone->repeatAxes.z ? chunkCoord(player.pos, 2,
                                                        *activeZone) : 0;

    if (mHasCell && hx == mLastCellX && hy == mLastCellY && hz == mLastCellZ)
        return;
    mLastCellX = hx;
    mLastCellY = hy;
    mLastCellZ = hz;
    mHasCell = true;

    const glm::ivec3 radius(activeZone->streamRadius.x, activeZone->streamRadius.y,
                            activeZone->repeatAxes.z ? activeZone->streamRadius.z
                                                     : 0);

    // Desired set.
    std::unordered_set<uint64_t> desired;
    const int rz = std::clamp(radius.z, 0, 3);
    for (int dz = -rz; dz <= rz; ++dz)
        for (int dy = -radius.y; dy <= radius.y; ++dy)
            for (int dx = -radius.x; dx <= radius.x; ++dx)
                desired.insert(cellKey(hx + dx, hy + dy, hz + dz));

    // Remove cells that left the radius.
    std::vector<uint64_t> toRemove;
    for (uint64_t key : mLoaded)
        if (!desired.count(key)) toRemove.push_back(key);
    for (uint64_t key : toRemove) {
        StreamedWorldLayer::instance().removeChunk(key);
        mLoaded.erase(key);
    }

    // Add new cells within a small per-frame budget.
    int added = 0;
    for (int dz = -rz; dz <= rz && added < kMaxChunksPerFrame; ++dz)
        for (int dy = -radius.y; dy <= radius.y && added < kMaxChunksPerFrame;
             ++dy)
            for (int dx = -radius.x; dx <= radius.x &&
                 added < kMaxChunksPerFrame; ++dx) {
                const uint64_t key = cellKey(hx + dx, hy + dy, hz + dz);
                if (mLoaded.count(key)) continue;
                placeChunk(hx + dx, hy + dy, hz + dz, *activeZone, candidates);
                ++added;
            }

    logEvent("world-stream.update", activeZone->id,
             {{"zone", activeZone->id},
              {"plane", activeZone->plane},
              {"cell", {hx, hy, hz}},
              {"spacing_mode", activeZone->spacingMode},
              {"added", added},
              {"removed", toRemove.size()},
              {"loaded", mLoaded.size()}});
}

void registerInfiniteWorldCommands()
{
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
                                " chunks=" + std::to_string(layer.chunkCount()));
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

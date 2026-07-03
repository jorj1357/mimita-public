#include "physics/movement/physics-collision-shared.h"

#include <chrono>
#include <vector>
#include <cstring>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"
#include "debug/debug-log.h"
#include "perf/perf.h"

#define BROAD_LOG(...) Debug::logThrottled(Debug::Category::Collision, "broadphase", 1.0f, __VA_ARGS__)

// Cache detection: track recent AABB queries to detect repeated work
static constexpr int QUERY_CACHE_SIZE = 32;
struct QueryCacheEntry {
    float minX, minY, minZ, maxX, maxY, maxZ;
    int frameNumber;
    const char* caller;
};
static QueryCacheEntry sQueryCache[QUERY_CACHE_SIZE];
static int sQueryCacheCount = 0;
static int sLastCacheFrame = -1;

static int findCachedQuery(const AABB& aabb, int currentFrame, const char* caller)
{
    for (int i = 0; i < sQueryCacheCount; ++i) {
        const auto& e = sQueryCache[i];
        if (e.frameNumber == currentFrame &&
            std::abs(e.minX - aabb.min.x) < 0.01f &&
            std::abs(e.minY - aabb.min.y) < 0.01f &&
            std::abs(e.minZ - aabb.min.z) < 0.01f &&
            std::abs(e.maxX - aabb.max.x) < 0.01f &&
            std::abs(e.maxY - aabb.max.y) < 0.01f &&
            std::abs(e.maxZ - aabb.max.z) < 0.01f)
        {
            return i;
        }
    }
    return -1;
}

static void recordQuery(const AABB& aabb, int currentFrame, const char* caller)
{
    if (currentFrame != sLastCacheFrame) {
        sQueryCacheCount = 0;
        sLastCacheFrame = currentFrame;
    }
    if (sQueryCacheCount < QUERY_CACHE_SIZE) {
        auto& e = sQueryCache[sQueryCacheCount++];
        e.minX = aabb.min.x; e.minY = aabb.min.y; e.minZ = aabb.min.z;
        e.maxX = aabb.max.x; e.maxY = aabb.max.y; e.maxZ = aabb.max.z;
        e.frameNumber = currentFrame;
        e.caller = caller;
    }
}

std::vector<int> gatherGLBTriangles(
    const World& world,
    const Capsule& cap,
    const glm::vec3& move,
    const char* caller
) {
    Perf::ScopedTimer _t("ChunkQuery");
    auto t0 = std::chrono::steady_clock::now();
    std::vector<int> out;

    AABB sweepBounds = makeSweptCapsuleAABB(cap, move);

    constexpr float EXTRA_XY = 1.0f;
    constexpr float EXTRA_Z_DOWN = 0.5f;
    constexpr float EXTRA_Z_UP = 0.5f;

    sweepBounds.min.x -= EXTRA_XY;
    sweepBounds.min.y -= EXTRA_XY;
    sweepBounds.max.x += EXTRA_XY;
    sweepBounds.max.y += EXTRA_XY;
    sweepBounds.min.z -= EXTRA_Z_DOWN;
    sweepBounds.max.z += EXTRA_Z_UP;

    // Cache detection: check for repeated query with same AABB
    int currentFrame = Perf::state().frameNumber;
    int cachedIdx = findCachedQuery(sweepBounds, currentFrame, caller);
    if (cachedIdx >= 0) {
        Perf::state().current.repeatedQueries++;
        BROAD_LOG("[CACHE REPEAT] caller=%s repeats previous %s query (same AABB)\n",
                   caller, sQueryCache[cachedIdx].caller);
    }
    recordQuery(sweepBounds, currentFrame, caller);

    appendChunkTrianglesForAABB(world, sweepBounds, cap.r + EXTRA_XY, out, "gatherGLBTriangles");

    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    Perf::state().current.broadphaseQueries++;
    Perf::state().current.chunkCellsVisited += (int)out.size() / 8;
    Perf::state().current.uniqueTriangles += (int)out.size();

    if (caller) {
        BROAD_LOG(
            "[GATHER] caller=%s candidates=%zu totalTris=%zu aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) elapsedMs=%.2f\n",
            caller, out.size(), world.collisionMesh.triangles.size(),
            sweepBounds.min.x, sweepBounds.min.y, sweepBounds.min.z,
            sweepBounds.max.x, sweepBounds.max.y, sweepBounds.max.z,
            elapsedMs);
    }

    if (out.size() > 500) {
        Debug::warn(Debug::Category::Collision,
            "[GATHER WARNING] caller=%s candidates=%zu exceeds threshold 500\n",
            caller ? caller : "?", out.size());
    }

    return out;
}

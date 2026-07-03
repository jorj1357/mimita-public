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

static void extractEntityInfo(const char* caller, char* entity, int entitySize,
    char* object, int objectSize, char* reason, int reasonSize)
{
    if (!caller) {
        std::snprintf(entity, entitySize, "?");
        std::snprintf(object, objectSize, "?");
        std::snprintf(reason, reasonSize, "?");
        return;
    }

    // Parse caller string in format: Entity_Object_Reason
    // or just a simple name
    const char* firstUnderscore = std::strchr(caller, '_');
    if (firstUnderscore) {
        int entityLen = (int)(firstUnderscore - caller);
        if (entityLen > entitySize - 1) entityLen = entitySize - 1;
        std::strncpy(entity, caller, entityLen);
        entity[entityLen] = '\0';

        const char* secondUnderscore = std::strchr(firstUnderscore + 1, '_');
        if (secondUnderscore) {
            int objLen = (int)(secondUnderscore - firstUnderscore - 1);
            if (objLen > objectSize - 1) objLen = objectSize - 1;
            std::strncpy(object, firstUnderscore + 1, objLen);
            object[objLen] = '\0';
            std::strncpy(reason, secondUnderscore + 1, reasonSize - 1);
            reason[reasonSize - 1] = '\0';
        } else {
            std::strncpy(object, firstUnderscore + 1, objectSize - 1);
            object[objectSize - 1] = '\0';
            std::snprintf(reason, reasonSize, "%s", caller);
        }
    } else {
        std::snprintf(entity, entitySize, "%s", caller);
        std::snprintf(object, objectSize, "?");
        std::snprintf(reason, reasonSize, "%s", caller);
    }
}

std::vector<int> gatherGLBTriangles(
    const World& world,
    const Capsule& cap,
    const glm::vec3& move,
    const char* caller
) {
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

    // Cache detection
    int currentFrame = Perf::state().frameNumber;
    int cachedIdx = findCachedQuery(sweepBounds, currentFrame, caller);
    if (cachedIdx >= 0) {
        Perf::state().current.repeatedQueries++;
        Perf::trackDuplicateQuery(caller, 0.0);
    }
    recordQuery(sweepBounds, currentFrame, caller);

    appendChunkTrianglesForAABB(world, sweepBounds, cap.r + EXTRA_XY, out, "gatherGLBTriangles");

    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    Perf::state().current.broadphaseQueries++;
    Perf::state().current.chunkCellsVisited += (int)out.size() / 8;
    Perf::state().current.uniqueTriangles += (int)out.size();

    // Record query for analysis
    float aabbMinF[3] = {sweepBounds.min.x, sweepBounds.min.y, sweepBounds.min.z};
    float aabbMaxF[3] = {sweepBounds.max.x, sweepBounds.max.y, sweepBounds.max.z};
    char entity[64], object[64], reason[64];
    extractEntityInfo(caller, entity, sizeof(entity), object, sizeof(object), reason, sizeof(reason));
    Perf::recordCollisionQuery(caller, reason, entity, object,
        aabbMinF, aabbMaxF, (int)out.size() / 8, (int)out.size(), elapsedMs);

    // Large AABB warning
    if (out.size() > 500) {
        Perf::checkLargeAABB(caller, entity, object, reason,
            (int)out.size() / 8, (int)out.size(), aabbMinF, aabbMaxF);
    }

    if (caller) {
        BROAD_LOG(
            "[GATHER] caller=%s candidates=%zu totalTris=%zu aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) elapsedMs=%.2f\n",
            caller, out.size(), world.collisionMesh.triangles.size(),
            sweepBounds.min.x, sweepBounds.min.y, sweepBounds.min.z,
            sweepBounds.max.x, sweepBounds.max.y, sweepBounds.max.z,
            elapsedMs);
    }

    return out;
}

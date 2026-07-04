#include "physics/movement/physics-collision-shared.h"

#include <chrono>
#include <cstdio>
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"
#include "debug/debug-log.h"
#include "perf/perf.h"

#define BROAD_LOG(...) Debug::logThrottled(Debug::Category::Collision, "broadphase", 1.0f, __VA_ARGS__)

// ── Current entity context ───────────────────────────────────
// Set before calling the collision pipeline, read by gatherGLBTriangles
// for entity identification in caller strings.
static const char* gCurrentEntityId = nullptr;
static unsigned int gCurrentEntityNumId = 0;
static bool gCurrentIsNpc = false;

void setCollisionEntityContext(const char* entityId, unsigned int entityNumId, bool isNpc)
{
    gCurrentEntityId = entityId;
    gCurrentEntityNumId = entityNumId;
    gCurrentIsNpc = isNpc;
}

void clearCollisionEntityContext()
{
    gCurrentEntityId = nullptr;
    gCurrentEntityNumId = 0;
    gCurrentIsNpc = false;
}

// ── Per-frame triangle cache ─────────────────────────────────
// Caches gatherGLBTriangles results keyed by a hash of the AABB.
// Persists across all calls within the same frame.
struct TriangleCacheEntry {
    uint64_t hash;
    std::vector<int> triangles;
    int frameNumber;
    const char* firstCaller;
};
static constexpr int TRIANGLE_CACHE_SIZE = 256;
static TriangleCacheEntry sTriCache[TRIANGLE_CACHE_SIZE];
static int sLastTriCacheFrame = -1;
static int sCacheHits = 0;
static int sCacheMisses = 0;

// Called once per frame from Perf::beginFrame or externally
void clearTriangleCache()
{
    int currentFrame = Perf::state().frameNumber;
    if (currentFrame != sLastTriCacheFrame) {
        for (int i = 0; i < TRIANGLE_CACHE_SIZE; ++i)
            sTriCache[i].frameNumber = -1;
        sLastTriCacheFrame = currentFrame;
        sCacheHits = 0;
        sCacheMisses = 0;
    }
}

static uint64_t hashAABB(const AABB& aabb)
{
    uint64_t h = 14695981039346656037ULL;
    auto mix = [&](float f) {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof(bits));
        h ^= bits;
        h *= 1099511628211ULL;
    };
    mix(aabb.min.x); mix(aabb.min.y); mix(aabb.min.z);
    mix(aabb.max.x); mix(aabb.max.y); mix(aabb.max.z);
    return h;
}

static bool getCachedTriangles(const AABB& aabb, int currentFrame, std::vector<int>& out)
{
    uint64_t h = hashAABB(aabb);
    for (int i = 0; i < TRIANGLE_CACHE_SIZE; ++i) {
        if (sTriCache[i].frameNumber == currentFrame && sTriCache[i].hash == h) {
            out = sTriCache[i].triangles;
            sCacheHits++;
            return true;
        }
    }
    return false;
}

static void cacheTriangles(const AABB& aabb, int currentFrame, const std::vector<int>& triangles, const char* caller)
{
    uint64_t h = hashAABB(aabb);
    for (int i = 0; i < TRIANGLE_CACHE_SIZE; ++i) {
        if (sTriCache[i].hash == h && sTriCache[i].frameNumber == currentFrame)
            return;
    }
    // Find eviction: replace oldest frame or first empty slot
    int evictIdx = 0;
    int oldestFrame = sTriCache[0].frameNumber;
    for (int i = 0; i < TRIANGLE_CACHE_SIZE; ++i) {
        if (sTriCache[i].frameNumber < 0) { evictIdx = i; break; }
        if (sTriCache[i].frameNumber < oldestFrame) {
            oldestFrame = sTriCache[i].frameNumber;
            evictIdx = i;
        }
    }
    sTriCache[evictIdx].hash = h;
    sTriCache[evictIdx].triangles = triangles;
    sTriCache[evictIdx].frameNumber = currentFrame;
    sTriCache[evictIdx].firstCaller = caller;
    sCacheMisses++;
}

// ── Query cache for duplicate detection ──────────────────────
static constexpr int QUERY_CACHE_SIZE = 32;
struct QueryCacheEntry {
    float minX, minY, minZ, maxX, maxY, maxZ;
    int frameNumber;
    const char* caller;
};
static QueryCacheEntry sQueryCache[QUERY_CACHE_SIZE];
static int sQueryCacheCount = 0;
static int sLastQueryCacheFrame = -1;

static int findCachedQuery(const AABB& aabb, int currentFrame)
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
    if (currentFrame != sLastQueryCacheFrame) {
        sQueryCacheCount = 0;
        sLastQueryCacheFrame = currentFrame;
    }
    if (sQueryCacheCount < QUERY_CACHE_SIZE) {
        auto& e = sQueryCache[sQueryCacheCount++];
        e.minX = aabb.min.x; e.minY = aabb.min.y; e.minZ = aabb.min.z;
        e.maxX = aabb.max.x; e.maxY = aabb.max.y; e.maxZ = aabb.max.z;
        e.frameNumber = currentFrame;
        e.caller = caller;
    }
}

// ── AABB validation ─────────────────────────────────────────
static bool isFinite(float v) { return std::isfinite(v); }
static bool isValidAABB(const AABB& aabb)
{
    return isFinite(aabb.min.x) && isFinite(aabb.min.y) && isFinite(aabb.min.z) &&
           isFinite(aabb.max.x) && isFinite(aabb.max.y) && isFinite(aabb.max.z) &&
           aabb.max.x >= aabb.min.x && aabb.max.y >= aabb.min.y && aabb.max.z >= aabb.min.z &&
           aabb.max.x - aabb.min.x < 10000.0f &&
           aabb.max.y - aabb.min.y < 10000.0f &&
           aabb.max.z - aabb.min.z < 10000.0f;
}

static AABB clampAABB(const AABB& aabb)
{
    AABB result = aabb;
    auto clampVal = [](float v) -> float {
        if (!isFinite(v)) return 0.0f;
        if (v > 5000.0f) return 5000.0f;
        if (v < -5000.0f) return -5000.0f;
        return v;
    };
    result.min.x = clampVal(result.min.x);
    result.min.y = clampVal(result.min.y);
    result.min.z = clampVal(result.min.z);
    result.max.x = clampVal(result.max.x);
    result.max.y = clampVal(result.max.y);
    result.max.z = clampVal(result.max.z);
    // Ensure min <= max
    if (result.max.x < result.min.x) std::swap(result.max.x, result.min.x);
    if (result.max.y < result.min.y) std::swap(result.max.y, result.min.y);
    if (result.max.z < result.min.z) std::swap(result.max.z, result.min.z);
    return result;
}

static void extractEntityInfo(const char* caller, char* entity, int entitySize,
    char* object, int objectSize, char* reason, int reasonSize, int* outEntityId)
{
    if (outEntityId) *outEntityId = -1;
    if (!caller || caller[0] == '\0') {
        std::snprintf(entity, entitySize, "Unknown");
        std::snprintf(object, objectSize, "Unknown");
        std::snprintf(reason, reasonSize, "Unknown");
        return;
    }

    // Try Entity_Object_Reason format
    const char* firstUnderscore = std::strchr(caller, '_');
    if (firstUnderscore) {
        int entityLen = (int)(firstUnderscore - caller);
        if (entityLen > entitySize - 1) entityLen = entitySize - 1;
        std::strncpy(entity, caller, entityLen);
        entity[entityLen] = '\0';
        // Ensure entity is never empty
        if (entity[0] == '\0') std::strncpy(entity, "Entity", entitySize - 1);

        const char* secondUnderscore = std::strchr(firstUnderscore + 1, '_');
        if (secondUnderscore) {
            int objLen = (int)(secondUnderscore - firstUnderscore - 1);
            if (objLen > objectSize - 1) objLen = objectSize - 1;
            std::strncpy(object, firstUnderscore + 1, objLen);
            object[objLen] = '\0';
            if (object[0] == '\0') std::strncpy(object, "Object", objectSize - 1);
            std::strncpy(reason, secondUnderscore + 1, reasonSize - 1);
            reason[reasonSize - 1] = '\0';
            if (reason[0] == '\0') std::strncpy(reason, "Reason", reasonSize - 1);
        } else {
            std::strncpy(object, firstUnderscore + 1, objectSize - 1);
            object[objectSize - 1] = '\0';
            if (object[0] == '\0') std::strncpy(object, "Object", objectSize - 1);
            std::snprintf(reason, reasonSize, "%s", caller);
        }
    } else {
        std::snprintf(entity, entitySize, "%s", caller);
        std::snprintf(object, objectSize, "General");
        std::snprintf(reason, reasonSize, "%s", caller);
    }

    // Try to extract entity ID from the entity field
    if (outEntityId) {
        const char* idStart = std::strchr(entity, '#');
        if (idStart) {
            *outEntityId = std::atoi(idStart + 1);
        }
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

    int currentFrame = Perf::state().frameNumber;
    clearTriangleCache();  // clears once per frame

    // Build entity-aware caller tag
    char augmentedCaller[128];
    if (gCurrentIsNpc && gCurrentEntityId) {
        std::snprintf(augmentedCaller, sizeof(augmentedCaller), "%s#%u_%s",
            gCurrentEntityId, gCurrentEntityNumId, caller ? caller : "gather");
    } else if (caller) {
        std::strncpy(augmentedCaller, caller, sizeof(augmentedCaller) - 1);
        augmentedCaller[sizeof(augmentedCaller) - 1] = '\0';
    } else {
        std::strncpy(augmentedCaller, "Unknown_Unknown_Unknown", sizeof(augmentedCaller) - 1);
    }
    const char* effectiveCaller = augmentedCaller;

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

    // Validate AABB - clamp invalid values
    bool aabbWasInvalid = !isValidAABB(sweepBounds);
    if (aabbWasInvalid) {
        glm::vec3 origMin = sweepBounds.min, origMax = sweepBounds.max;
        sweepBounds = clampAABB(sweepBounds);
        Debug::warn(Debug::Category::Collision,
            "[AABB INVALID] caller=%s entity=%s origMin=(%.2f %.2f %.2f) origMax=(%.2f %.2f %.2f) "
            "clampedMin=(%.2f %.2f %.2f) clampedMax=(%.2f %.2f %.2f)\n",
            effectiveCaller,
            gCurrentEntityId ? gCurrentEntityId : "?",
            origMin.x, origMin.y, origMin.z,
            origMax.x, origMax.y, origMax.z,
            sweepBounds.min.x, sweepBounds.min.y, sweepBounds.min.z,
            sweepBounds.max.x, sweepBounds.max.y, sweepBounds.max.z);
    }

    // Check cache first
    if (getCachedTriangles(sweepBounds, currentFrame, out)) {
        auto t1 = std::chrono::steady_clock::now();
        float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
        BROAD_LOG("[GATHER CACHE HIT] caller=%s candidates=%zu elapsedMs=%.3f\n",
                   effectiveCaller, out.size(), elapsedMs);
        return out;
    }

    // Cache detection for duplicate queries
    int cachedIdx = findCachedQuery(sweepBounds, currentFrame);
    if (cachedIdx >= 0) {
        Perf::state().current.repeatedQueries++;
        Perf::trackDuplicateQuery(effectiveCaller, 0.0);
    }
    recordQuery(sweepBounds, currentFrame, effectiveCaller);

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
    int parsedEntityId = -1;
    extractEntityInfo(effectiveCaller, entity, sizeof(entity), object, sizeof(object),
                      reason, sizeof(reason), &parsedEntityId);
    Perf::recordCollisionQuery(effectiveCaller, reason, entity, object,
        aabbMinF, aabbMaxF, (int)out.size() / 8, (int)out.size(), elapsedMs);

    // Large AABB warning
    if (out.size() > 500) {
        Perf::checkLargeAABB(effectiveCaller, entity, object, reason,
            (int)out.size() / 8, (int)out.size(), aabbMinF, aabbMaxF);
    }

    // Extreme explosion warning
    if (out.size() > 50000) {
        glm::vec3 size = sweepBounds.max - sweepBounds.min;
        Debug::warn(Debug::Category::Collision,
            "\n[COLLISION EXPLOSION] "
            "\n  caller=%s  entity=%s  object=%s  reason=%s"
            "\n  AABB size=(%.1f, %.1f, %.1f)"
            "\n  candidates=%zu  chunkCells=%d"
            "\n  elapsedMs=%.2f"
            "\n  aabbMin=(%.1f, %.1f, %.1f)  aabbMax=(%.1f, %.1f, %.1f)"
            "\n  wasInvalid=%d"
            "\n  isNpc=%d  npcId=%u\n",
            effectiveCaller, entity, object, reason,
            size.x, size.y, size.z,
            out.size(), (int)out.size() / 8,
            elapsedMs,
            sweepBounds.min.x, sweepBounds.min.y, sweepBounds.min.z,
            sweepBounds.max.x, sweepBounds.max.y, sweepBounds.max.z,
            (int)aabbWasInvalid,
            (int)gCurrentIsNpc, gCurrentEntityNumId);
    }

    // Log every query (sorted later)
    {
        glm::vec3 size = sweepBounds.max - sweepBounds.min;
        BROAD_LOG(
            "[GATHER] caller=%s candidates=%zu totalTris=%zu "
            "aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) aabbSize=(%.1f %.1f %.1f) "
            "move=(%.2f %.2f %.2f) elapsedMs=%.2f\n",
            effectiveCaller, out.size(), world.collisionMesh.triangles.size(),
            sweepBounds.min.x, sweepBounds.min.y, sweepBounds.min.z,
            sweepBounds.max.x, sweepBounds.max.y, sweepBounds.max.z,
            size.x, size.y, size.z,
            move.x, move.y, move.z,
            elapsedMs);
    }

    // Cache result for this frame
    cacheTriangles(sweepBounds, currentFrame, out, effectiveCaller);

    return out;
}

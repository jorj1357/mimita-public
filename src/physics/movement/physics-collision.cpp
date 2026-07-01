#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <cmath>
#include <cfloat>
#include <limits>
#include <string>
#include <unordered_set>
#include <glm/glm.hpp>

#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "config/player-settings.h"
#include "perf/perf.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"

#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)
#define CHUNK_LOG(...) Debug::logThrottled(Debug::Category::Collision, "chunk-query", 1.0f, __VA_ARGS__)
#define CHUNK_WARN(...) Debug::warn(Debug::Category::Collision, __VA_ARGS__)

CollisionTraceSnapshot gLastCollisionTrace;

void appendUniqueTriangleIndices(std::vector<int>& dst, const std::vector<int>& src)
{
    for (int triIndex : src)
    {
        if (std::find(dst.begin(), dst.end(), triIndex) == dst.end())
            dst.push_back(triIndex);
    }
}

static inline AABB makeBlockAABB(const Block& b)
{
    glm::vec3 half = b.size * 0.5f;
    return { b.pos - half, b.pos + half };
}

static inline bool isFiniteAABB(const AABB& a)
{
    return std::isfinite(a.min.x) && std::isfinite(a.min.y) && std::isfinite(a.min.z) &&
           std::isfinite(a.max.x) && std::isfinite(a.max.y) && std::isfinite(a.max.z);
}

void appendChunkTrianglesForAABB(
    const World& world,
    const AABB& queryBounds,
    float expansion,
    std::vector<int>& out
) {
    auto t0 = std::chrono::steady_clock::now();

    // ── NaN/Inf guard ─────────────────────────────────
    if (!isFiniteAABB(queryBounds))
    {
        CHUNK_WARN("[CHUNK GUARD] Non-finite queryBounds min=(%f %f %f) max=(%f %f %f)\n",
                    queryBounds.min.x, queryBounds.min.y, queryBounds.min.z,
                    queryBounds.max.x, queryBounds.max.y, queryBounds.max.z);
        return;
    }

    // ── Clamp extreme values ───────────────────────────
    constexpr float MAX_EXTENT = 10000.0f;
    AABB clamped = queryBounds;
    clamped.min = glm::clamp(clamped.min, glm::vec3(-MAX_EXTENT), glm::vec3(MAX_EXTENT));
    clamped.max = glm::clamp(clamped.max, glm::vec3(-MAX_EXTENT), glm::vec3(MAX_EXTENT));
    if (clamped.min.x > clamped.max.x) std::swap(clamped.min.x, clamped.max.x);
    if (clamped.min.y > clamped.max.y) std::swap(clamped.min.y, clamped.max.y);
    if (clamped.min.z > clamped.max.z) std::swap(clamped.min.z, clamped.max.z);

    if (world.collisionChunks.empty() || world.collisionChunkSize <= 0.001f)
    {
        for (int i = 0; i < (int)world.collisionMesh.triangles.size(); ++i)
        {
            AABB triBounds = makeTriangleAABB(world.collisionMesh.triangles[i]);
            triBounds.min -= glm::vec3(expansion);
            triBounds.max += glm::vec3(expansion);
            if (overlaps(clamped, triBounds))
                out.push_back(i);
        }
        auto t1 = std::chrono::steady_clock::now();
        float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
        CHUNK_LOG("[CHUNK FALLBACK] totalTris=%zu candidates=%zu elapsedMs=%.3f\n",
                   world.collisionMesh.triangles.size(), out.size(), ms);
        return;
    }

    glm::ivec3 c0 = collisionChunkCoord(clamped.min, world.collisionChunkSize);
    glm::ivec3 c1 = collisionChunkCoord(clamped.max, world.collisionChunkSize);

    // ── Cell count guard ───────────────────────────────
    constexpr int MAX_CELLS_PER_AXIS = 15;
    int64_t cellsX = (int64_t)c1.x - (int64_t)c0.x + 1;
    int64_t cellsY = (int64_t)c1.y - (int64_t)c0.y + 1;
    int64_t cellsZ = (int64_t)c1.z - (int64_t)c0.z + 1;

    // If the query spans too many cells, clamp the range to the world bounds.
    // This prevents the triple loop from running for billions of iterations when
    // an AABB contains NaN, Inf, or extreme coordinates.
    // 15 cells per axis = max ~3375 cell lookups — safe even on low-end hardware.
    if (cellsX <= 0 || cellsY <= 0 || cellsZ <= 0 ||
        cellsX > MAX_CELLS_PER_AXIS || cellsY > MAX_CELLS_PER_AXIS || cellsZ > MAX_CELLS_PER_AXIS)
    {
        CHUNK_WARN("[CHUNK GUARD] Excessive cell range clamped: "
                    "aabb min=(%.1f %.1f %.1f) max=(%.1f %.1f %.1f) "
                    "cells=(%lld %lld %lld) c0=(%d %d %d) c1=(%d %d %d)\n",
                    clamped.min.x, clamped.min.y, clamped.min.z,
                    clamped.max.x, clamped.max.y, clamped.max.z,
                    (long long)cellsX, (long long)cellsY, (long long)cellsZ,
                    c0.x, c0.y, c0.z, c1.x, c1.y, c1.z);
        // Clamp to a safe range around c0
        glm::ivec3 safeMin(
            glm::max(c0.x, c0.x - MAX_CELLS_PER_AXIS / 2),
            glm::max(c0.y, c0.y - MAX_CELLS_PER_AXIS / 2),
            glm::max(c0.z, c0.z - MAX_CELLS_PER_AXIS / 2)
        );
        glm::ivec3 safeMax(
            glm::min(c1.x, c0.x + MAX_CELLS_PER_AXIS / 2),
            glm::min(c1.y, c0.y + MAX_CELLS_PER_AXIS / 2),
            glm::min(c1.z, c0.z + MAX_CELLS_PER_AXIS / 2)
        );
        c0 = glm::min(safeMin, safeMax);
        c1 = glm::max(safeMin, safeMax);
        cellsX = (int64_t)c1.x - (int64_t)c0.x + 1;
        cellsY = (int64_t)c1.y - (int64_t)c0.y + 1;
        cellsZ = (int64_t)c1.z - (int64_t)c0.z + 1;
    }

    int64_t totalCells = cellsX * cellsY * cellsZ;
    int cellCount = 0;
    std::unordered_set<int> seen;
    (void)cellCount;

    for (int x = c0.x; x <= c1.x; ++x)
    for (int y = c0.y; y <= c1.y; ++y)
    for (int z = c0.z; z <= c1.z; ++z)
    {
        ++cellCount;
        auto it = world.collisionChunks.find(glm::ivec3(x, y, z));
        if (it == world.collisionChunks.end())
            continue;

        for (int triIndex : it->second)
        {
            if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                continue;
            if (!seen.insert(triIndex).second)
                continue;

            AABB triBounds = makeTriangleAABB(world.collisionMesh.triangles[triIndex]);
            triBounds.min -= glm::vec3(expansion);
            triBounds.max += glm::vec3(expansion);
            if (overlaps(clamped, triBounds))
                out.push_back(triIndex);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();

    // Log whenever cells or triangles are suspiciously high
    if (cellCount > 200 || out.size() > 500)
    {
        CHUNK_WARN("[CHUNK QUERY] aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) "
                    "cells=%d/%lld uniqueTris=%zu elapsedMs=%.3f\n",
                    clamped.min.x, clamped.min.y, clamped.min.z,
                    clamped.max.x, clamped.max.y, clamped.max.z,
                    cellCount, (long long)totalCells, out.size(), ms);
    }
    else
    {
        CHUNK_LOG("[CHUNK QUERY] aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) "
                   "cells=%d uniqueTris=%zu elapsedMs=%.3f\n",
                   clamped.min.x, clamped.min.y, clamped.min.z,
                   clamped.max.x, clamped.max.y, clamped.max.z,
                   cellCount, out.size(), ms);
    }
}

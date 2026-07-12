#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <cmath>
#include <cfloat>
#include <limits>
#include <string>
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
#include "physics/ray-utils.h"

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
    std::vector<int>& out,
    const char* caller
) {
    auto t0 = std::chrono::steady_clock::now();

    // ── NaN/Inf guard ─────────────────────────────────
    if (!isFiniteAABB(queryBounds))
    {
        CHUNK_WARN("[CHUNK GUARD] caller=%s Non-finite queryBounds min=(%f %f %f) max=(%f %f %f)\n",
                    caller ? caller : "?",
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
        CHUNK_LOG("[CHUNK FALLBACK] caller=%s totalTris=%zu candidates=%zu elapsedMs=%.3f\n",
                   caller ? caller : "?", world.collisionMesh.triangles.size(), out.size(), ms);
        return;
    }

    glm::ivec3 c0 = collisionChunkCoord(clamped.min, world.collisionChunkSize);
    glm::ivec3 c1 = collisionChunkCoord(clamped.max, world.collisionChunkSize);

    // ── Cell count guard ───────────────────────────────
    constexpr int MAX_CELLS_PER_AXIS = 100;
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
        CHUNK_WARN("[CHUNK GUARD] caller=%s Excessive cell range clamped: "
                    "aabb min=(%.1f %.1f %.1f) max=(%.1f %.1f %.1f) "
                    "cells=(%lld %lld %lld) c0=(%d %d %d) c1=(%d %d %d)\n",
                    caller ? caller : "?",
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
    (void)cellCount;

    // Dedup via generation counter — O(1) per triangle, zero heap allocations
    thread_local std::vector<uint32_t> s_triGen;
    thread_local uint32_t s_gen = 0;
    s_gen++;
    if (s_gen == 0) {
        s_triGen.assign(world.collisionMesh.triangles.size(), 0);
        s_gen = 1;
    }
    if (s_triGen.size() != world.collisionMesh.triangles.size())
        s_triGen.assign(world.collisionMesh.triangles.size(), 0);

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
            if (s_triGen[triIndex] == s_gen)
                continue;
            s_triGen[triIndex] = s_gen;

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
        CHUNK_WARN("[CHUNK QUERY] caller=%s aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) "
                    "cells=%d/%lld uniqueTris=%zu elapsedMs=%.3f\n",
                    caller ? caller : "?",
                    clamped.min.x, clamped.min.y, clamped.min.z,
                    clamped.max.x, clamped.max.y, clamped.max.z,
                    cellCount, (long long)totalCells, out.size(), ms);
    }
    else
    {
        CHUNK_LOG("[CHUNK QUERY] caller=%s aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) "
                   "cells=%d uniqueTris=%zu elapsedMs=%.3f\n",
                   caller ? caller : "?",
                   clamped.min.x, clamped.min.y, clamped.min.z,
                   clamped.max.x, clamped.max.y, clamped.max.z,
                   cellCount, out.size(), ms);
    }
}

bool rayTraverseGridCells(
    const World& world,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    float maxDist,
    float& hitDist)
{
    auto t0 = std::chrono::steady_clock::now();

    // Degenerate ray guard
    if (glm::dot(rayDir, rayDir) < 0.000001f) {
        hitDist = maxDist;
        return false;
    }

    if (world.collisionChunks.empty() || world.collisionChunkSize <= 0.001f ||
        world.collisionMesh.triangles.empty())
    {
        // No spatial grid — fall back to brute force over all triangles
        float nearest = maxDist;
        bool hit = false;
        for (size_t i = 0; i < world.collisionMesh.triangles.size(); ++i) {
            float d = 0.0f;
            if (rayTriangle(rayOrigin, rayDir, world.collisionMesh.triangles[i], d) && d < nearest) {
                nearest = d;
                hit = true;
            }
        }
        hitDist = nearest;
        return hit;
    }

    const float cs = world.collisionChunkSize;
    const glm::vec3 invDir(1.0f / rayDir.x, 1.0f / rayDir.y, 1.0f / rayDir.z);

    // Determine the range of cells the ray passes through
    glm::vec3 startPos = rayOrigin;
    glm::vec3 endPos = rayOrigin + rayDir * maxDist;

    glm::ivec3 cStart = collisionChunkCoord(startPos, cs);
    glm::ivec3 cEnd = collisionChunkCoord(endPos, cs);

    // Clamp end cell to ensure we don't iterate billions of cells
    auto clampCoord = [](int v) -> int {
        constexpr int MAX_COORD = 10000;
        return glm::clamp(v, -MAX_COORD, MAX_COORD);
    };
    cStart = glm::ivec3(clampCoord(cStart.x), clampCoord(cStart.y), clampCoord(cStart.z));
    cEnd = glm::ivec3(clampCoord(cEnd.x), clampCoord(cEnd.y), clampCoord(cEnd.z));

    // Amanatides & Woo 3D DDA: step across grid cells
    glm::ivec3 step(
        rayDir.x > 0 ? 1 : (rayDir.x < 0 ? -1 : 0),
        rayDir.y > 0 ? 1 : (rayDir.y < 0 ? -1 : 0),
        rayDir.z > 0 ? 1 : (rayDir.z < 0 ? -1 : 0)
    );

    // Distance along ray to the next grid boundary on each axis
    glm::vec3 tDelta(
        (rayDir.x != 0.0f) ? (cs / rayDir.x) : 1e30f,
        (rayDir.y != 0.0f) ? (cs / rayDir.y) : 1e30f,
        (rayDir.z != 0.0f) ? (cs / rayDir.z) : 1e30f
    );
    // Use fabs for tDelta to handle negative direction properly
    tDelta.x = std::fabs(tDelta.x);
    tDelta.y = std::fabs(tDelta.y);
    tDelta.z = std::fabs(tDelta.z);

    // Find the cell containing the ray origin
    glm::ivec3 cell = cStart;

    // Distance from ray origin to the first cell boundary on each axis
    auto cellBoundary = [&](int axis, float coord) -> float {
        float boundary = (float)cell[axis] * cs;
        if (step[axis] > 0) boundary += cs;
        return (boundary - rayOrigin[axis]) / rayDir[axis];
    };

    glm::vec3 tMax(
        (step.x != 0) ? cellBoundary(0, rayOrigin.x) : 1e30f,
        (step.y != 0) ? cellBoundary(1, rayOrigin.y) : 1e30f,
        (step.z != 0) ? cellBoundary(2, rayOrigin.z) : 1e30f
    );

    // Track which triangles we've already tested (generation counter method)
    thread_local std::vector<uint32_t> s_triGen;
    thread_local uint32_t s_gen = 0;
    s_gen++;
    if (s_gen == 0) {
        s_triGen.assign(world.collisionMesh.triangles.size(), 0);
        s_gen = 1;
    }
    if (s_triGen.size() != world.collisionMesh.triangles.size())
        s_triGen.assign(world.collisionMesh.triangles.size(), 0);

    float nearest = maxDist;
    bool hit = false;
    int cellCount = 0;

    // Step through cells until we exit the grid or pass the nearest hit
    while (true) {
        // Check if current cell is within range
        if (cell.x >= std::min(cStart.x, cEnd.x) - 1 && cell.x <= std::max(cStart.x, cEnd.x) + 1 &&
            cell.y >= std::min(cStart.y, cEnd.y) - 1 && cell.y <= std::max(cStart.y, cEnd.y) + 1 &&
            cell.z >= std::min(cStart.z, cEnd.z) - 1 && cell.z <= std::max(cStart.z, cEnd.z) + 1)
        {
            ++cellCount;
            auto it = world.collisionChunks.find(cell);
            if (it != world.collisionChunks.end()) {
                for (int triIndex : it->second) {
                    if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                        continue;
                    if (s_triGen[triIndex] == s_gen)
                        continue;
                    s_triGen[triIndex] = s_gen;

                    float d = 0.0f;
                    if (rayTriangle(rayOrigin, rayDir, world.collisionMesh.triangles[triIndex], d) && d < nearest) {
                        nearest = d;
                        hit = true;
                        // We found a hit — we could stop now if we wanted. But
                        // we continue to make sure no closer hit exists in nearer cells
                        // we haven't yet visited. However, since we visit cells in
                        // near-to-far order, any hit found is the closest possible
                        // (all nearer cells already visited). So we can stop.
                        goto done;
                    }
                }
            }
        }

        // Step to next cell along the ray
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                if (tMax.x > nearest) break;
                cell.x += step.x;
                tMax.x += tDelta.x;
            } else {
                if (tMax.z > nearest) break;
                cell.z += step.z;
                tMax.z += tDelta.z;
            }
        } else {
            if (tMax.y < tMax.z) {
                if (tMax.y > nearest) break;
                cell.y += step.y;
                tMax.y += tDelta.y;
            } else {
                if (tMax.z > nearest) break;
                cell.z += step.z;
                tMax.z += tDelta.z;
            }
        }

        // Check if we've exited the grid bounds
        if (std::abs(cell.x) > 10000 || std::abs(cell.y) > 10000 || std::abs(cell.z) > 10000)
            break;
    }

done:
    auto t1 = std::chrono::steady_clock::now();
    float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
    if (cellCount > 200) {
        CHUNK_WARN("[RAY QUERY] caller=computeAimTarget ray=%.1f %.1f %.1f dir=%.2f %.2f %.2f "
                    "cells=%d hit=%d elapsedMs=%.3f\n",
                    rayOrigin.x, rayOrigin.y, rayOrigin.z,
                    rayDir.x, rayDir.y, rayDir.z,
                    cellCount, hit, ms);
    } else {
        CHUNK_LOG("[RAY QUERY] caller=computeAimTarget cells=%d hit=%d elapsedMs=%.3f\n",
                   cellCount, hit, ms);
    }

    hitDist = nearest;
    return hit;
}

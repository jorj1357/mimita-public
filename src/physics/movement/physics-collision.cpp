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
#include "physics/movement/physics-collision-subgrid.h"
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

    if (!isFiniteAABB(queryBounds))
    {
        CHUNK_WARN("[CHUNK GUARD] caller=%s Non-finite queryBounds min=(%f %f %f) max=(%f %f %f)\n",
                    caller ? caller : "?",
                    queryBounds.min.x, queryBounds.min.y, queryBounds.min.z,
                    queryBounds.max.x, queryBounds.max.y, queryBounds.max.z);
        return;
    }

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

    constexpr int MAX_CELLS_PER_AXIS = 100;
    int64_t cellsX = (int64_t)c1.x - (int64_t)c0.x + 1;
    int64_t cellsY = (int64_t)c1.y - (int64_t)c0.y + 1;
    int64_t cellsZ = (int64_t)c1.z - (int64_t)c0.z + 1;

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

    thread_local std::vector<uint32_t> s_triGen;
    thread_local uint32_t s_gen = 0;
    s_gen++;
    if (s_gen == 0) {
        s_triGen.assign(world.collisionMesh.triangles.size(), 0);
        s_gen = 1;
    }
    if (s_triGen.size() != world.collisionMesh.triangles.size())
        s_triGen.assign(world.collisionMesh.triangles.size(), 0);

    // Expand the query AABB by `expansion` for sub-cell selection so triangles in
    // the overlap filter's expanded zone are never missed.
    AABB subQuery = clamped;
    subQuery.min -= glm::vec3(expansion);
    subQuery.max += glm::vec3(expansion);

    auto visitTriangle = [&](int triIndex) -> bool {
        if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
            return false;
        if (s_triGen[triIndex] == s_gen)
            return false;
        s_triGen[triIndex] = s_gen;

        AABB triBounds = makeTriangleAABB(world.collisionMesh.triangles[triIndex]);
        triBounds.min -= glm::vec3(expansion);
        triBounds.max += glm::vec3(expansion);
        if (overlaps(clamped, triBounds))
            out.push_back(triIndex);
        return false;
    };

    for (int x = c0.x; x <= c1.x; ++x)
    for (int y = c0.y; y <= c1.y; ++y)
    for (int z = c0.z; z <= c1.z; ++z)
    {
        ++cellCount;
        glm::ivec3 chunkCoord(x, y, z);
        auto it = world.collisionChunks.find(chunkCoord);
        if (it == world.collisionChunks.end())
            continue;

        collision_subgrid::forEachChunkTriOverlap(world, chunkCoord, it->second, subQuery, visitTriangle);
    }

    // Include large triangles that exceeded MAX_CHUNKS_PER_TRIANGLE
    if (!world.collisionLargeTriangles.empty())
    {
        for (int triIndex : world.collisionLargeTriangles)
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

// ── Swept-sphere vs point ─────────────────────────────────────────
static bool sweptSpherePoint(
    const glm::vec3& origin, const glm::vec3& direction,
    float radius, const glm::vec3& point,
    float maxDist, float& hitDist, glm::vec3& hitNormal)
{
    glm::vec3 rel = origin - point;
    float a = glm::dot(direction, direction);
    float b = 2.0f * glm::dot(rel, direction);
    float c = glm::dot(rel, rel) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;
    float sqrtDisc = sqrtf(disc);
    float t0 = (-b - sqrtDisc) / (2.0f * a);
    float t1 = (-b + sqrtDisc) / (2.0f * a);
    float tHit = (t0 >= 0.0f) ? t0 : t1;
    if (tHit < 0.0f || tHit > maxDist) return false;
    hitDist = tHit;
    glm::vec3 centerAtT = origin + direction * tHit;
    glm::vec3 n = centerAtT - point;
    float nLen = glm::length(n);
    if (nLen < 0.000001f)
        n = -direction;
    else
        n /= nLen;
    hitNormal = n;
    return true;
}

// ── Swept-sphere vs edge ──────────────────────────────────────────
static bool sweptSphereEdge(
    const glm::vec3& origin, const glm::vec3& direction, float radius,
    const glm::vec3& edgeA, const glm::vec3& edgeB, float maxDist,
    float& hitDist, glm::vec3& hitNormal, glm::vec3& hitPoint)
{
    glm::vec3 edgeDir = edgeB - edgeA;
    float edgeLen = glm::length(edgeDir);
    if (edgeLen < 0.000001f) return false;
    edgeDir /= edgeLen;

    glm::vec3 rel = origin - edgeA;
    float proj = glm::dot(rel, edgeDir);
    glm::vec3 relPerp = rel - edgeDir * proj;
    glm::vec3 movePerp = direction - edgeDir * glm::dot(direction, edgeDir);

    float a = glm::dot(movePerp, movePerp);
    if (a < 0.0000001f) return false;

    float b = 2.0f * glm::dot(relPerp, movePerp);
    float c = glm::dot(relPerp, relPerp) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;

    float tHit = (-b - sqrtf(disc)) / (2.0f * a);
    if (tHit < 0.0f || tHit > maxDist) return false;

    glm::vec3 centerAtT = origin + direction * tHit;
    glm::vec3 relAtT = centerAtT - edgeA;
    float projAtT = glm::dot(relAtT, edgeDir);
    if (projAtT < 0.0f || projAtT > edgeLen) return false;

    glm::vec3 closestOnEdge = edgeA + edgeDir * projAtT;
    glm::vec3 n = centerAtT - closestOnEdge;
    float nLen = glm::length(n);
    if (nLen < 0.000001f) return false;
    n /= nLen;

    hitDist = tHit;
    hitNormal = n;
    hitPoint = closestOnEdge;
    return true;
}

// ── Point-in-triangle test (barycentric) ──────────────────────────
static bool pointInTri(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::vec3 v0 = c - a;
    glm::vec3 v1 = b - a;
    glm::vec3 v2 = p - a;
    float dot00 = glm::dot(v0, v0);
    float dot01 = glm::dot(v0, v1);
    float dot02 = glm::dot(v0, v2);
    float dot11 = glm::dot(v1, v1);
    float dot12 = glm::dot(v1, v2);
    float denom = dot00 * dot11 - dot01 * dot01;
    if (std::fabs(denom) < 0.000000001f) return false;
    float invDenom = 1.0f / denom;
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    return (u >= -0.000001f) && (v >= -0.000001f) && (u + v <= 1.0f + 0.000001f);
}

// ── Swept-sphere vs triangle (public, declared in .h) ─────────────
bool sweptSphereTriangle(
    const glm::vec3& origin, const glm::vec3& direction, float radius,
    const CollisionTriangle& tri, float maxDist,
    float& hitDist, glm::vec3& hitNormal, glm::vec3& hitPoint)
{
    float bestT = maxDist;
    glm::vec3 bestN(0.0f);
    glm::vec3 bestP(0.0f);
    bool hit = false;

    // Face
    glm::vec3 n = tri.normal;
    float dist = glm::dot(origin - tri.a, n);
    if (dist < 0.0f) {
        n = -n;
        dist = -dist;
    }

    float denom = glm::dot(direction, n);
    if (denom < -0.000001f) {
        float t = (radius - dist) / denom;
        if (t >= 0.0f && t < bestT) {
            glm::vec3 centerAtT = origin + direction * t;
            glm::vec3 planePoint = centerAtT - n * radius;
            if (pointInTri(planePoint, tri.a, tri.b, tri.c)) {
                bestT = t;
                bestN = n;
                bestP = planePoint;
                hit = true;
            }
        }
    }

    // Edges
    glm::vec3 edgePairs[3][2] = {{tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}};
    for (auto& ep : edgePairs) {
        float t = maxDist;
        glm::vec3 en(0.0f);
        glm::vec3 epPt(0.0f);
        if (sweptSphereEdge(origin, direction, radius, ep[0], ep[1], maxDist, t, en, epPt) && t < bestT) {
            bestT = t;
            bestN = en;
            bestP = epPt;
            hit = true;
        }
    }

    // Vertices
    glm::vec3 verts[3] = {tri.a, tri.b, tri.c};
    for (auto& v : verts) {
        float t = maxDist;
        glm::vec3 vn(0.0f);
        if (sweptSpherePoint(origin, direction, radius, v, maxDist, t, vn) && t < bestT) {
            bestT = t;
            bestN = vn;
            bestP = v;
            hit = true;
        }
    }

    if (!hit) return false;
    hitDist = bestT;
    hitNormal = bestN;
    hitPoint = bestP;
    return true;
}

// ── FIXED thin-ray DDA ────────────────────────────────────────────
// Tests ALL triangles in each cell, tracks the closest hit.
// Stops when the next cell boundary is farther than the closest known hit.
bool rayTraverseGridCells(
    const World& world,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    float maxDist,
    float& hitDist,
    glm::vec3* outNormal)
{
    auto t0 = std::chrono::steady_clock::now();

    if (glm::dot(rayDir, rayDir) < 0.000001f) {
        hitDist = maxDist;
        return false;
    }

    if (world.collisionChunks.empty() || world.collisionChunkSize <= 0.001f ||
        world.collisionMesh.triangles.empty())
    {
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

    AABB rayAABB{
        glm::min(rayOrigin, rayOrigin + rayDir * maxDist),
        glm::max(rayOrigin, rayOrigin + rayDir * maxDist)
    };

    glm::ivec3 cStart = collisionChunkCoord(rayOrigin, cs);
    glm::ivec3 cEnd = collisionChunkCoord(rayOrigin + rayDir * maxDist, cs);

    auto clampCoord = [](int v) -> int {
        constexpr int MAX_COORD = 10000;
        return glm::clamp(v, -MAX_COORD, MAX_COORD);
    };
    cStart = glm::ivec3(clampCoord(cStart.x), clampCoord(cStart.y), clampCoord(cStart.z));
    cEnd = glm::ivec3(clampCoord(cEnd.x), clampCoord(cEnd.y), clampCoord(cEnd.z));

    glm::ivec3 step(
        rayDir.x > 0 ? 1 : (rayDir.x < 0 ? -1 : 0),
        rayDir.y > 0 ? 1 : (rayDir.y < 0 ? -1 : 0),
        rayDir.z > 0 ? 1 : (rayDir.z < 0 ? -1 : 0)
    );

    glm::vec3 tDelta(
        (rayDir.x != 0.0f) ? std::fabs(cs / rayDir.x) : 1e30f,
        (rayDir.y != 0.0f) ? std::fabs(cs / rayDir.y) : 1e30f,
        (rayDir.z != 0.0f) ? std::fabs(cs / rayDir.z) : 1e30f
    );

    glm::ivec3 cell = cStart;

    auto cellBoundary = [&](int axis) -> float {
        float boundary = (float)cell[axis] * cs;
        if (step[axis] > 0) boundary += cs;
        return (boundary - rayOrigin[axis]) / rayDir[axis];
    };

    glm::vec3 tMax(
        (step.x != 0) ? cellBoundary(0) : 1e30f,
        (step.y != 0) ? cellBoundary(1) : 1e30f,
        (step.z != 0) ? cellBoundary(2) : 1e30f
    );

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
    glm::vec3 bestN(0.0f);
    bool hit = false;
    int cellCount = 0;

    // FIXED: test ALL triangles in each cell, find closest,
    // then check if next cell boundary is past the closest hit
    while (true) {
        auto it = world.collisionChunks.find(cell);
        if (it != world.collisionChunks.end()) {
            ++cellCount;
            const glm::vec3 chunkMin((float)cell.x * cs, (float)cell.y * cs, (float)cell.z * cs);
            AABB cellAABB{chunkMin, chunkMin + glm::vec3(cs)};
            AABB overlap{glm::max(rayAABB.min, cellAABB.min), glm::min(rayAABB.max, cellAABB.max)};
            collision_subgrid::forEachChunkTriOverlap(world, cell, it->second, overlap, [&](int triIndex) -> bool {
                if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                    return false;
                if (s_triGen[triIndex] == s_gen)
                    return false;
                s_triGen[triIndex] = s_gen;

                float d = 0.0f;
                if (rayTriangle(rayOrigin, rayDir, world.collisionMesh.triangles[triIndex], d) && d < nearest) {
                    nearest = d;
                    bestN = world.collisionMesh.triangles[triIndex].normal;
                    hit = true;
                }
                return false;
            });
        }

        // Step to next cell — stop if next boundary is past closest hit
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

        if (std::abs(cell.x) > 10000 || std::abs(cell.y) > 10000 || std::abs(cell.z) > 10000)
            break;
    }

    // ── Test large triangles that exceed MAX_CHUNKS_PER_TRIANGLE ──────
    if (!world.collisionLargeTriangles.empty())
    {
        for (int triIndex : world.collisionLargeTriangles)
        {
            if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                continue;
            if (s_triGen[triIndex] == s_gen)
                continue;
            s_triGen[triIndex] = s_gen;

            float d = 0.0f;
            if (rayTriangle(rayOrigin, rayDir, world.collisionMesh.triangles[triIndex], d) &&
                d >= 0.0f && d <= nearest && d < nearest)
            {
                nearest = d;
                bestN = world.collisionMesh.triangles[triIndex].normal;
                hit = true;
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
    if (cellCount > 200) {
        CHUNK_WARN("[RAY DDA] caller=rayTraverseGridCells cells=%d hit=%d nearest=%.2f elapsedMs=%.3f\n",
                    cellCount, hit, nearest, ms);
    } else {
        CHUNK_LOG("[RAY DDA] caller=rayTraverseGridCells cells=%d hit=%d elapsedMs=%.3f\n",
                   cellCount, hit, ms);
    }

    hitDist = nearest;
    if (outNormal) *outNormal = bestN;
    return hit;
}

// ── NEW thick-ray (swept-sphere) DDA ──────────────────────────────
// Traverses centerline cells, also visiting neighbors within beam radius.
bool sweptSphereTraverseGridCells(
    const World& world,
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    float radius,
    float& hitDistance,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint)
{
    auto t0 = std::chrono::steady_clock::now();

    if (glm::dot(direction, direction) < 0.000001f) {
        hitDistance = maxDistance;
        hitPoint = origin + direction * maxDistance;
        return false;
    }

    if (world.collisionChunks.empty() || world.collisionChunkSize <= 0.001f ||
        world.collisionMesh.triangles.empty())
    {
        float nearest = maxDistance;
        glm::vec3 bestN(0.0f);
        glm::vec3 bestP = origin + direction * maxDistance;
        bool hit = false;
        for (size_t i = 0; i < world.collisionMesh.triangles.size(); ++i) {
            float d = 0.0f;
            glm::vec3 n, p;
            if (sweptSphereTriangle(origin, direction, radius,
                                     world.collisionMesh.triangles[i], maxDistance, d, n, p) && d < nearest) {
                nearest = d;
                bestN = n;
                bestP = p;
                hit = true;
            }
        }
        hitDistance = nearest;
        hitNormal = bestN;
        hitPoint = bestP;
        return hit;
    }

    const float cs = world.collisionChunkSize;

    // Beam swept AABB (centerline plus radius) used for sub-cell selection.
    AABB sweptAABB{
        glm::min(origin, origin + direction * maxDistance) - glm::vec3(radius),
        glm::max(origin, origin + direction * maxDistance) + glm::vec3(radius)
    };

    // Determine how many neighbor cells the beam radius can reach
    int neighborCells = (int)std::ceil(radius / cs) + 1;

    glm::ivec3 cStart = collisionChunkCoord(origin, cs);
    glm::ivec3 cEnd = collisionChunkCoord(origin + direction * maxDistance, cs);

    auto clampCoord = [](int v) -> int {
        constexpr int MAX_COORD = 10000;
        return glm::clamp(v, -MAX_COORD, MAX_COORD);
    };
    cStart = glm::ivec3(clampCoord(cStart.x), clampCoord(cStart.y), clampCoord(cStart.z));
    cEnd = glm::ivec3(clampCoord(cEnd.x), clampCoord(cEnd.y), clampCoord(cEnd.z));

    glm::ivec3 step(
        direction.x > 0 ? 1 : (direction.x < 0 ? -1 : 0),
        direction.y > 0 ? 1 : (direction.y < 0 ? -1 : 0),
        direction.z > 0 ? 1 : (direction.z < 0 ? -1 : 0)
    );

    glm::vec3 tDelta(
        (direction.x != 0.0f) ? std::fabs(cs / direction.x) : 1e30f,
        (direction.y != 0.0f) ? std::fabs(cs / direction.y) : 1e30f,
        (direction.z != 0.0f) ? std::fabs(cs / direction.z) : 1e30f
    );

    glm::ivec3 cell = cStart;

    auto cellBoundary = [&](int axis) -> float {
        float boundary = (float)cell[axis] * cs;
        if (step[axis] > 0) boundary += cs;
        return (boundary - origin[axis]) / direction[axis];
    };

    glm::vec3 tMax(
        (step.x != 0) ? cellBoundary(0) : 1e30f,
        (step.y != 0) ? cellBoundary(1) : 1e30f,
        (step.z != 0) ? cellBoundary(2) : 1e30f
    );

    // Triangle dedup
    thread_local std::vector<uint32_t> s_triGen;
    thread_local uint32_t s_gen = 0;
    s_gen++;
    if (s_gen == 0) {
        s_triGen.assign(world.collisionMesh.triangles.size(), 0);
        s_gen = 1;
    }
    if (s_triGen.size() != world.collisionMesh.triangles.size())
        s_triGen.assign(world.collisionMesh.triangles.size(), 0);

    // Cell dedup: store visited cell coordinates in a small local buffer.
    // Typical weapon queries visit < 100 unique cells; linear scan over a
    // small array is faster and more reliable than hashing.
    thread_local std::vector<glm::ivec3> s_visitedCells;
    s_visitedCells.clear();

    float nearest = maxDistance;
    glm::vec3 bestN(0.0f);
    glm::vec3 bestP = origin + direction * maxDistance;
    bool hit = false;
    int centerCellCount = 0;
    int neighborCellCount = 0;

    while (true) {
        // Visit center cell + neighbors within radius
        for (int dx = -neighborCells; dx <= neighborCells; ++dx)
        for (int dy = -neighborCells; dy <= neighborCells; ++dy)
        for (int dz = -neighborCells; dz <= neighborCells; ++dz)
        {
            glm::ivec3 nc(cell.x + dx, cell.y + dy, cell.z + dz);

            // Cell dedup: linear scan over already-visited cells.
            // The visited set is small (< 100 entries for weapon queries).
            bool seen = false;
            for (const auto& vc : s_visitedCells) {
                if (vc.x == nc.x && vc.y == nc.y && vc.z == nc.z) {
                    seen = true;
                    break;
                }
            }
            if (seen) continue;
            s_visitedCells.push_back(nc);

            auto it = world.collisionChunks.find(nc);
            if (it == world.collisionChunks.end())
                continue;

            if (dx == 0 && dy == 0 && dz == 0)
                ++centerCellCount;
            ++neighborCellCount;

            const glm::vec3 chunkMin((float)nc.x * cs, (float)nc.y * cs, (float)nc.z * cs);
            AABB cellAABB{chunkMin, chunkMin + glm::vec3(cs)};
            AABB overlap{glm::max(sweptAABB.min, cellAABB.min), glm::min(sweptAABB.max, cellAABB.max)};
            collision_subgrid::forEachChunkTriOverlap(world, nc, it->second, overlap, [&](int triIndex) -> bool {
                if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                    return false;
                if (s_triGen[triIndex] == s_gen)
                    return false;
                s_triGen[triIndex] = s_gen;

                float d = 0.0f;
                glm::vec3 n, p;
                if (sweptSphereTriangle(origin, direction, radius,
                                         world.collisionMesh.triangles[triIndex],
                                         maxDistance, d, n, p) && d < nearest) {
                    nearest = d;
                    bestN = n;
                    bestP = p;
                    hit = true;
                    // We found a hit, but continue checking remaining triangles
                    // in this cell's neighborhood — they could be closer.
                }
                return false;
            });
        }

        // Step to next centerline cell — stop if next boundary is past closest hit
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

        if (std::abs(cell.x) > 10000 || std::abs(cell.y) > 10000 || std::abs(cell.z) > 10000)
            break;
    }

    // ── Test large triangles that exceed MAX_CHUNKS_PER_TRIANGLE ──────
    if (!world.collisionLargeTriangles.empty())
    {
        for (int triIndex : world.collisionLargeTriangles)
        {
            if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                continue;
            if (s_triGen[triIndex] == s_gen)
                continue;
            s_triGen[triIndex] = s_gen;

            float d = maxDistance;
            glm::vec3 n(0.0f);
            glm::vec3 p(0.0f);
            if (sweptSphereTriangle(origin, direction, radius,
                                     world.collisionMesh.triangles[triIndex],
                                     maxDistance, d, n, p) && d < nearest)
            {
                nearest = d;
                bestN = n;
                bestP = p;
                hit = true;
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    float ms = std::chrono::duration<float, std::milli>(t1 - t0).count();
    if (neighborCellCount > 200) {
        CHUNK_WARN("[SPHERE DDA] radius=%.2f centerCells=%d neighborCells=%d hit=%d nearest=%.2f elapsedMs=%.3f\n",
                    radius, centerCellCount, neighborCellCount, hit, nearest, ms);
    } else {
        CHUNK_LOG("[SPHERE DDA] radius=%.2f centerCells=%d neighborCells=%d hit=%d elapsedMs=%.3f\n",
                   radius, centerCellCount, neighborCellCount, hit, ms);
    }

    hitDistance = nearest;
    hitNormal = bestN;
    hitPoint = bestP;
    return hit;
}

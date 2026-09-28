// 2026-09-28
/* purpose
* Implement the destructible-geometry owner: signed-distance helpers, initial
* box meshing, spherical cut storage, chunk dirtying, and partial rebuilds.
* Generated triangles are a cache written back into PhysicalEntity::localTriangles
* by the caller; the authoritative truth is the box minus the stored spheres.
* Does NOT decide cut size/damage (ImpactSystem) or own rigid-body motion.
*/

#include "impact/destructible-geometry.h"

#include <algorithm>
#include <cmath>

#include "impact/isosurface.h"

namespace MimitaImpact {

// ── Signed-distance helpers ─────────────────────────────────────────────
float boxDistance(glm::vec3 p, glm::vec3 halfSize)
{
    const glm::vec3 q = glm::abs(p) - halfSize;
    const float outside = glm::length(glm::max(q, glm::vec3(0.0f)));
    const float inside = std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
    return outside + inside;
}

float sphereDistance(glm::vec3 p, glm::vec3 center, float radius)
{
    return glm::length(p - center) - radius;
}

float destructibleCrateDistance(const DestructibleGeometry& geometry,
                                glm::vec3 localPoint)
{
    float result = boxDistance(localPoint, geometry.halfExtents);
    for (const DestructionCutSphere& cut : geometry.sphereCuts)
    {
        const float hole = sphereDistance(localPoint, cut.localCenter, cut.radius);
        // Subtract the sphere from the solid: inside a sphere becomes empty.
        result = std::max(result, -hole);
    }
    return result;
}

namespace {

bool sphereOverlapsAABB(const glm::vec3& center, float radius, const AABB& box)
{
    const glm::vec3 closest = glm::clamp(center, box.min, box.max);
    return glm::length(center - closest) <= radius;
}

} // anonymous namespace

DestructibleGeometrySystem& DestructibleGeometrySystem::instance()
{
    static DestructibleGeometrySystem system;
    return system;
}

void DestructibleGeometrySystem::initialize(DestructibleGeometry& geometry,
                                            glm::vec3 halfExtents)
{
    geometry.enabled = true;
    geometry.broken = false;
    geometry.halfExtents = halfExtents;
    geometry.localOrigin = -halfExtents;
    geometry.localExtent = halfExtents * 2.0f;
    geometry.sphereCuts.clear();
    geometry.nextCutId = 1;
    geometry.geometryRevision = 0;

    const int K = std::max(1, geometry.chunkCountPerAxis);
    const int C = std::max(1, geometry.cellsPerChunkAxis);
    geometry.chunkCountPerAxis = K;
    geometry.cellsPerChunkAxis = C;

    const float maxHalf = std::max(halfExtents.x, std::max(halfExtents.y, halfExtents.z));
    const float cellSize = maxHalf > 1e-6f ? (2.0f * maxHalf) / (float)(K * C) : 1.0f;
    const glm::vec3 gridOrigin = -halfExtents - glm::vec3(cellSize);
    const int totalCells = K * C + 2;

    geometry.chunks.clear();
    geometry.chunks.reserve((size_t)K * K * K);
    for (int kz = 0; kz < K; ++kz)
    for (int ky = 0; ky < K; ++ky)
    for (int kx = 0; kx < K; ++kx)
    {
        DestructionChunk chunk;
        const glm::ivec3 beginCell(kx * C, ky * C, kz * C);
        const glm::ivec3 endCell(
            kx == K - 1 ? totalCells - 1 : (kx + 1) * C - 1,
            ky == K - 1 ? totalCells - 1 : (ky + 1) * C - 1,
            kz == K - 1 ? totalCells - 1 : (kz + 1) * C - 1);
        chunk.localBounds.min = gridOrigin + glm::vec3(beginCell) * cellSize;
        chunk.localBounds.max = gridOrigin + glm::vec3(endCell + glm::ivec3(1)) * cellSize;
        chunk.dirty = true;
        geometry.chunks.push_back(std::move(chunk));
    }

    rebuildAll(geometry);
}

int DestructibleGeometrySystem::addCut(DestructibleGeometry& geometry,
                                       const DestructionCutSphere& cut)
{
    if (!geometry.enabled)
        return 0;

    DestructionCutSphere stored = cut;
    if (stored.cutId == 0)
        stored.cutId = geometry.nextCutId++;
    geometry.sphereCuts.push_back(stored);

    // Only chunks the cut can touch need remeshing.
    for (DestructionChunk& chunk : geometry.chunks)
    {
        if (sphereOverlapsAABB(stored.localCenter, stored.radius, chunk.localBounds))
            chunk.dirty = true;
    }

    const int rebuilt = rebuildDirtyChunks(geometry);
    return rebuilt;
}

void DestructibleGeometrySystem::refreshCachedArrays(DestructibleGeometry& geometry)
{
    geometry.renderVertices.clear();
    geometry.collisionTriangles.clear();
    size_t renderCount = 0;
    size_t triCount = 0;
    for (const DestructionChunk& chunk : geometry.chunks)
    {
        renderCount += chunk.renderVertices.size();
        triCount += chunk.collisionTriangles.size();
    }
    geometry.renderVertices.reserve(renderCount);
    geometry.collisionTriangles.reserve(triCount);
    for (const DestructionChunk& chunk : geometry.chunks)
    {
        geometry.renderVertices.insert(geometry.renderVertices.end(),
                                       chunk.renderVertices.begin(),
                                       chunk.renderVertices.end());
        geometry.collisionTriangles.insert(geometry.collisionTriangles.end(),
                                           chunk.collisionTriangles.begin(),
                                           chunk.collisionTriangles.end());
    }
}

int DestructibleGeometrySystem::rebuildAll(DestructibleGeometry& geometry)
{
    for (DestructionChunk& chunk : geometry.chunks)
        chunk.dirty = true;
    return rebuildDirtyChunks(geometry);
}

int DestructibleGeometrySystem::rebuildDirtyChunks(DestructibleGeometry& geometry)
{
    if (!geometry.enabled || geometry.chunks.empty())
        return 0;

    const int K = std::max(1, geometry.chunkCountPerAxis);
    const int C = std::max(1, geometry.cellsPerChunkAxis);
    const int totalCells = K * C + 2;
    const float maxHalf = std::max(geometry.halfExtents.x,
                          std::max(geometry.halfExtents.y, geometry.halfExtents.z));
    const float cellSize = maxHalf > 1e-6f ? (2.0f * maxHalf) / (float)(K * C) : 1.0f;
    const glm::vec3 gridOrigin = -geometry.halfExtents - glm::vec3(cellSize);

    size_t totalTriangles = 0;
    int rebuilt = 0;
    size_t index = 0;
    for (int kz = 0; kz < K; ++kz)
    for (int ky = 0; ky < K; ++ky)
    for (int kx = 0; kx < K; ++kx, ++index)
    {
        DestructionChunk& chunk = geometry.chunks[index];
        if (!chunk.dirty)
        {
            totalTriangles += chunk.collisionTriangles.size();
            continue;
        }

        const glm::ivec3 beginCell(kx * C, ky * C, kz * C);
        const glm::ivec3 cellCount(
            (kx == K - 1 ? totalCells - 1 : (kx + 1) * C - 1) - beginCell.x + 1,
            (ky == K - 1 ? totalCells - 1 : (ky + 1) * C - 1) - beginCell.y + 1,
            (kz == K - 1 ? totalCells - 1 : (kz + 1) * C - 1) - beginCell.z + 1);

        const size_t budget = totalTriangles >= maxTrianglesPerEntity
            ? 0 : maxTrianglesPerEntity - totalTriangles;
        GeneratedDestructionMesh mesh = meshDestructibleChunk(
            geometry, beginCell, cellCount, cellSize, gridOrigin, budget);
        chunk.renderVertices = std::move(mesh.renderVertices);
        chunk.collisionTriangles = std::move(mesh.collisionTriangles);
        chunk.dirty = false;
        totalTriangles += chunk.collisionTriangles.size();
        ++rebuilt;
    }

    if (rebuilt > 0)
        ++geometry.geometryRevision;
    refreshCachedArrays(geometry);
    return rebuilt;
}

} // namespace MimitaImpact

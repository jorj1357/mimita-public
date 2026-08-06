// 08 05 2026, 23 00
/* purpose
* Sub-grid broadphase query helpers shared by the collision pipeline and NPC
* line-of-sight traversal.
* Splits each 6-unit collision chunk into 4^3 sub-cells so queries near dense
* geometry (spheres, text, houses) only test the sub-cells their AABB touches.
* Does NOT own collision response, contact solving, or broadphase building.
* Does NOT own movement physics or rendering.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "world/world.h"
#include "physics/physics-types.h"

namespace collision_subgrid {

struct SubCellRange {
    glm::ivec3 s0{0};
    glm::ivec3 s1{0};
    bool valid = false;
};

// Maps an AABB (already clamped to the chunk's world bounds) to the range of
// sub-cells it overlaps inside the chunk. Returns valid=false for an empty box.
inline SubCellRange subCellRangeForAABB(const CollisionSubGrid& sub, int subdiv,
                                        const glm::vec3& chunkMin, const AABB& aabb)
{
    SubCellRange r;
    if (aabb.min.x > aabb.max.x || aabb.min.y > aabb.max.y || aabb.min.z > aabb.max.z)
        return r;
    glm::ivec3 s0((int)std::floor((aabb.min.x - chunkMin.x) / sub.subSize),
                  (int)std::floor((aabb.min.y - chunkMin.y) / sub.subSize),
                  (int)std::floor((aabb.min.z - chunkMin.z) / sub.subSize));
    glm::ivec3 s1((int)std::floor((aabb.max.x - chunkMin.x) / sub.subSize),
                  (int)std::floor((aabb.max.y - chunkMin.y) / sub.subSize),
                  (int)std::floor((aabb.max.z - chunkMin.z) / sub.subSize));
    r.s0 = glm::clamp(s0, glm::ivec3(0), glm::ivec3(subdiv - 1));
    r.s1 = glm::clamp(s1, glm::ivec3(0), glm::ivec3(subdiv - 1));
    r.valid = true;
    return r;
}

inline const CollisionSubGrid* findSubGrid(const World& world, const glm::ivec3& chunkCoord)
{
    auto it = world.collisionSubGrids.find(chunkCoord);
    if (it != world.collisionSubGrids.end() && it->second.subSize > 0.001f)
        return &it->second;
    return nullptr;
}

inline int subdivForChunk(const World& world, const CollisionSubGrid& sub)
{
    return std::max(1, (int)std::floor(world.collisionChunkSize / sub.subSize + 0.5f));
}

// Iterate the triangles of a chunk, visiting only sub-cells that overlap `aabb`
// (or all triangles when no sub-grid exists). `visit(triIndex)` returns true to
// stop iterating early (used by ray traversals that find a hit).
template <typename F>
void forEachChunkTriOverlap(const World& world, const glm::ivec3& chunkCoord,
                            const std::vector<int>& chunkTris, const AABB& aabb, F&& visit)
{
    const CollisionSubGrid* sub = findSubGrid(world, chunkCoord);
    if (!sub)
    {
        for (int triIndex : chunkTris)
            if (visit(triIndex)) return;
        return;
    }
    const int subdiv = subdivForChunk(world, *sub);
    const glm::vec3 chunkMin = glm::vec3(chunkCoord) * world.collisionChunkSize;
    const SubCellRange sr = subCellRangeForAABB(*sub, subdiv, chunkMin, aabb);
    if (!sr.valid)
        return;
    for (int sx = sr.s0.x; sx <= sr.s1.x; ++sx)
    for (int sy = sr.s0.y; sy <= sr.s1.y; ++sy)
    for (int sz = sr.s0.z; sz <= sr.s1.z; ++sz)
    {
        auto scIt = sub->cells.find(glm::ivec3(sx, sy, sz));
        if (scIt == sub->cells.end())
            continue;
        for (int triIndex : scIt->second)
            if (visit(triIndex)) return;
    }
}

} // namespace collision_subgrid

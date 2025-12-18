// C:\important\quiet\n\mimita-public\mimita-public\src\world\world.h not cpp 
// dec 16 2025
/**
 * purpose
 * make chunks 
 * helper for chunks
 * so we can do multi pass
 * so collisions work 
 * so i can walk on spheres and triangles and cones and whatever 
 * not just boring csgo flat land walls maaaaube some  surfing NO
 * mi game is COOL and works with BELNDER 
 * ...biiiiiiitch
 */

#pragma once

#include <unordered_map>
#include <vector>
// this throws errors for no reason ignore 
#include "glm/glm.hpp"

#include "world-types.h"
#include "map/map_common.h"
#include "../physics/config.h"
#include "physics/collision-capsule-triangle.h"

struct Chunk
{
    std::vector<Triangle> tris;
};

struct IVec3Hash
{
    size_t operator()(const glm::ivec3& v) const noexcept
    {
        size_t h1 = std::hash<int>{}(v.x);
        size_t h2 = std::hash<int>{}(v.y);
        size_t h3 = std::hash<int>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct World
{
    float chunkSize = CHUNK_SIZE;
    std::unordered_map<glm::ivec3, Chunk, IVec3Hash> chunks;

    void buildFromMesh(const Mesh& mesh);
    void getNearbyTriangles(glm::vec3 pos, std::vector<Triangle>& out) const;
    void getNearbyTrianglesForCapsule(
        const Capsule& cap,
        const glm::vec3& move,
        std::vector<Triangle>& out
    ) const;
};

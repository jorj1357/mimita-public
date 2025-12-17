// C:\important\quiet\n\mimita-public\mimita-public\src\world\world.h
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
#include <glm/glm.hpp>
#include "world-types.h"
#include "map/map_common.h"

struct Chunk
{
    std::vector<Triangle> tris;
};

struct World
{
    float chunkSize = 8.0f;
    std::unordered_map<glm::ivec3, Chunk> chunks;

    void buildFromMesh(const Mesh& mesh);
    void getNearbyTriangles(glm::vec3 pos, std::vector<Triangle>& out) const;
};

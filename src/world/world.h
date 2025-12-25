// C:\important\quiet\n\mimita-public\mimita-public\src\world\world.h not cpp 
// dec 16 2025
/**
 * purpose
 * define World data (blocks, spheres)
 * make chunks 
 * helper for chunks
 * so we can do multi pass
 * so collisions work 
 */

// dec 19 2025
/*
// Coordinate system (engine-wide):
// X = right
// Y = forward
// Z = up
// Blender exports are already Z-up.
// No basis conversion anywhere.
*/

#pragma once

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "map/map_common.h"

#include "physics/config.h"

// todo for jorj figure out what  this does 
// and how to make cross platform happy for entire whole big fat repo 
// Hash function for glm::ivec3 to use as key in unordered_map
namespace std {
    template<>
    struct hash<glm::ivec3> {
        size_t operator()(const glm::ivec3& v) const {
            size_t h1 = std::hash<int>()(v.x);
            size_t h2 = std::hash<int>()(v.y);
            size_t h3 = std::hash<int>()(v.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

// --------------------
// Position helpers 
// --------------------

// no static goes here only struct and stuff 

// --------------------
// World primitives
// --------------------

struct Block {
    glm::vec3 pos;
    glm::vec3 size;
    // toYUp THE ONLY CALL OF toYUp, blenderPosToEngine, blenderRotToEngine, convertToEngineSpace, OR ANY OTHER WORLD FLIPPING SHOULD BE IN WORLD.CPP dec 19 2025
    glm::vec3 rotEuler; // from JSON, degrees, Blender space
    glm::mat3 rot;
    // TEMP: per-face texture indices
    // order: +X, -X, +Y, -Y, +Z, -Z
    uint8_t tex[6];
};

struct Sphere {
    glm::vec3 pos;
    float radius;
    uint8_t tex; // texture index
};

// --------------------
// Chunking
// --------------------

struct Chunk {
    std::vector<Block*> blocks;
    std::vector<Sphere*> spheres;
};

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const noexcept {
        size_t h1 = std::hash<int>{}(v.x);
        size_t h2 = std::hash<int>{}(v.y);
        size_t h3 = std::hash<int>{}(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// --------------------
// World
// --------------------

struct World {
    float chunkSize = CHUNK_SIZE;

    std::vector<Block> blocks;
    std::vector<Sphere> spheres;
    std::unordered_map<glm::ivec3, Chunk, IVec3Hash> chunks;

    // lifecycle
    void clear() {
        blocks.clear();
        spheres.clear();
        chunks.clear();
    }

    // chunking
    void rebuildChunks();

    // queries
    void getNearby(
        const glm::vec3& pos,
        std::vector<Block*>& outBlocks,
        std::vector<Sphere*>& outSpheres
    ) const;
    void finalize(); 



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

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

#pragma once

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "physics/config.h"

// --------------------
// World primitives
// --------------------

struct Block {
    glm::vec3 pos;
    glm::vec3 size;
    glm::vec3 rot;
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

    // toYUp THE ONLY CALL OF toYUp, convertToEngineSpace, OR ANY OTHER WORLD FLIPPING SHOULD BE IN WORLD.CPP dec 19 2025
    // this is allowed because it defines the function for world.cpp AND ONLY WORLD.CPP TO USE
    void convertToEngineSpace(); 

    // chunking
    void rebuildChunks();

    // queries
    void getNearby(
        const glm::vec3& pos,
        std::vector<Block*>& outBlocks,
        std::vector<Sphere*>& outSpheres
    ) const;
};

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
// Position helpers 
// --------------------

static glm::ivec3 chunkCoord(const glm::vec3& p, float size)
{
    return glm::ivec3(
        (int)floor(p.x / size),
        0,
        (int)floor(p.z / size)
    );
}

static glm::vec3 blenderPosToEngine(const glm::vec3& p)
{
    // Blender: X right, Y forward, Z up
    // Engine:  X right, Y up,      Z forward
    return glm::vec3(-p.x, p.z, p.y);
}

static glm::vec3 blenderRotToEngine(const glm::vec3& r)
{
    return glm::vec3(-r.x + 90.0f, r.z, r.y);
}

static glm::mat3 eulerToMat(const glm::vec3& deg)
{
    glm::vec3 r = glm::radians(deg);

    // THE LAW: Y -> X -> Z
    return
        glm::mat3(glm::rotate(glm::mat4(1.0f), r.y, glm::vec3(0,1,0))) *
        glm::mat3(glm::rotate(glm::mat4(1.0f), r.x, glm::vec3(1,0,0))) *
        glm::mat3(glm::rotate(glm::mat4(1.0f), r.z, glm::vec3(0,0,1)));
}

// --------------------
// World primitives
// --------------------

struct Block {
    glm::vec3 pos;
    glm::vec3 size;
    // toYUp THE ONLY CALL OF toYUp, blenderPosToEngine, blenderRotToEngine, convertToEngineSpace, OR ANY OTHER WORLD FLIPPING SHOULD BE IN WORLD.CPP dec 19 2025
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
};

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
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "map/map_common.h"

#include "physics/config.h"
#include "physics/physics-types.h"

#include <string>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>

struct Triangle;

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
    // glm::mat3 rot;
    // setting rot as 1.0f for the default to keep things safe 
    glm::mat3 rot = glm::mat3(1.0f);  // <- default identity
    // TEMP: per-face texture indices
    // order: +X, -X, +Y, -Y, +Z, -Z
    // nevermind we do smth else
    // uint8_t tex[6];
    // todo use this feb 4 2026
    // nvm feb 9 2026 we just going to skip slopes bc its stupid and make me sad 
    bool isSlope = false;
    std::string texName;
    // Optional per-face texture names in render order:
    // bottom, top, left, right, front, back. Empty entries fall back to texName.
    std::array<std::string, 6> faceTexName;
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

// Second-level uniform sub-grid built inside each collision chunk so broadphase
// queries only touch sub-cells overlapping their AABB (dense Blender objects that
// pack thousands of triangles into one 6-unit chunk no longer cost thousands of
// triangle tests per query).
struct CollisionSubGrid {
    float subSize = 0.0f;
    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> cells;
};

struct Plane {
    glm::vec3 point;
    glm::vec3 normal;

    glm::vec3 tangent;   // direction along ramp
    glm::vec3 bitangent; // width direction

    float halfLength;    // along tangent
    float halfWidth;     // along bitangent
};

// --------------------
// Spawn Points
// --------------------

struct SpawnPoint {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    std::string tag;
    int arenaIndex = -1;
};

// --------------------
// World
// --------------------

struct World {
    float chunkSize = CHUNK_SIZE;

    std::vector<Block> blocks;
    std::vector<Sphere> spheres;
    std::unordered_map<glm::ivec3, Chunk, IVec3Hash> chunks;

    Mesh mesh;
    Mesh skyMesh;
    CollisionMeshCache collisionMesh;
    float collisionChunkSize = 6.0f;
    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> collisionChunks;
    std::vector<int> collisionLargeTriangles;
    std::unordered_map<glm::ivec3, CollisionSubGrid, IVec3Hash> collisionSubGrids;

    // Coarse grid for triangles that span too many normal chunks to be indexed in
    // collisionChunks. Keeps large ground/floor triangles location-filtered so a
    // broadphase query only tests the ones near it instead of re-scanning every
    // large triangle every query.
    std::unordered_map<glm::ivec3, std::vector<int>, IVec3Hash> collisionLargeChunks;
    std::vector<int> collisionAlwaysLargeTriangles;

    std::vector<SpawnPoint> spawnPoints;
    int selectedSpawnIndex = -1;
    std::uint64_t renderRevision = 0;

    void clear();
    void rebuildChunks();

    void getNearby(
        const glm::vec3& pos,
        std::vector<Block*>& outBlocks,
        std::vector<Sphere*>& outSpheres
    ) const;

    SpawnPoint* pickSpawnPoint(const std::string& tag = "", int arenaIndex = -1);

    std::vector<Plane> planes;

    void finalize();
};

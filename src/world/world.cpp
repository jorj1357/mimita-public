// C:\important\quiet\n\mimita-public\mimita-public\src\world\world.cpp
// dec 18 2025
/**
 * purpose
 * define chunking functions and world laoding i think
 * for the json conversion
 * it said delete this file and then recreate it ok buddy
 * vibe coder andy
 * dec 19 2025 we are doing z = up like blender, no more converting
 */

#include "world.h"

#include <cmath>
#include <cstdio>

// --------------------
// Helpers
// --------------------

static glm::ivec3 chunkCoord(const glm::vec3& p, float size)
{
    return glm::ivec3(
        (int)floor(p.x / size),
        (int)floor(p.y / size),
        0 // z is up dec 19 2025 
    );
}


static glm::mat3 eulerXYZDegToMat3(const glm::vec3& deg)
{
    glm::vec3 r = glm::radians(deg);

    glm::mat3 Rx = glm::mat3(glm::rotate(glm::mat4(1.0f), r.x, glm::vec3(1,0,0)));
    glm::mat3 Ry = glm::mat3(glm::rotate(glm::mat4(1.0f), r.y, glm::vec3(0,1,0)));
    glm::mat3 Rz = glm::mat3(glm::rotate(glm::mat4(1.0f), r.z, glm::vec3(0,0,1)));

    return Rz * Ry * Rx;
}

// --------------------
// World methods
// --------------------

void World::rebuildChunks()
{
    chunks.clear();

    for (auto& b : blocks) {

        glm::vec3 half = b.size * 0.5f;

        glm::vec3 minP = b.pos - half;
        glm::vec3 maxP = b.pos + half;

        glm::ivec3 c0 = chunkCoord(minP, chunkSize);
        glm::ivec3 c1 = chunkCoord(maxP, chunkSize);

        for (int x = c0.x; x <= c1.x; x++)
        for (int y = c0.y; y <= c1.y; y++) {
            glm::ivec3 c(x, y, 0);
            chunks[c].blocks.push_back(&b);

        }
    }

    for (auto& s : spheres) {
        glm::ivec3 c = chunkCoord(s.pos, chunkSize);
        chunks[c].spheres.push_back(&s);
    }
}

// dec 19 2025 todo understand why const 
void World::getNearby(
    const glm::vec3& pos,
    std::vector<Block*>& outBlocks,
    std::vector<Sphere*>& outSpheres
) const
{
    outBlocks.clear();
    outSpheres.clear();

    glm::ivec3 base = chunkCoord(pos, chunkSize);

    // z up dec 19 2025 
    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    {
        glm::ivec3 c = base + glm::ivec3(x, y, 0);
        auto it = chunks.find(c);
        if (it == chunks.end()) continue;

        outBlocks.insert(
            outBlocks.end(),
            it->second.blocks.begin(),
            it->second.blocks.end()
        );

        outSpheres.insert(
            outSpheres.end(),
            it->second.spheres.begin(),
            it->second.spheres.end()
        );
    }
    

}

void World::clear()
{
    blocks.clear();
    spheres.clear();
    chunks.clear();
}

void World::finalize()
{
    for (auto& b : blocks) {
        // U BETTER NOT frick:)  EVERTHING UP DEC 24 2025 JORJ bc rotations
        b.rot = eulerXYZDegToMat3(b.rotEuler);
    }
}

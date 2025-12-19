// C:\important\quiet\n\mimita-public\mimita-public\src\world\world.cpp
// dec 18 2025
/**
 * purpose
 * define chunking functions and world laoding i think
 * for the json conversion
 * it said delete this file and then recreate it ok buddy
 * vibe coder andy
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
        0,
        (int)floor(p.z / size)
    );
}

// its a 90 deg angle mismatch, not 180 like a sign flip
static glm::mat3 blenderToEngineBasis()
{
    // Rotate -90° around X:
    // Blender Z-up → Engine Y-up
    return glm::mat3(
        glm::rotate(
            glm::mat4(1.0f),
            glm::radians(-90.0f),
            glm::vec3(1,0,0)
        )
    );
}

static glm::mat3 eulerXYZDegToMat3(const glm::vec3& deg)
{
    // remap Blender Euler → Engine Euler
    // Blender: X right, Y forward, Z up
    // Engine:  X right, Y up,      Z forward
    glm::vec3 r = glm::radians(glm::vec3(
        deg.x,  // X stays X
        deg.z,  // Z (up) becomes Y (up)
        deg.y   // Y (forward) becomes Z (forward)
    ));

    // Apply X then Y then Z (right-multiply local vector)
    glm::mat3 Rx = glm::mat3(glm::rotate(glm::mat4(1.0f), r.x, glm::vec3(1,0,0)));
    glm::mat3 Ry = glm::mat3(glm::rotate(glm::mat4(1.0f), r.y, glm::vec3(0,1,0)));
    glm::mat3 Rz = glm::mat3(glm::rotate(glm::mat4(1.0f), r.z, glm::vec3(0,0,1)));

    return Rz * Ry * Rx;
}

// --------------------
// World methods
// --------------------

// toYUp THE ONLY CALL OF toYUp, blenderPosToEngine, blenderRotToEngine, convertToEngineSpace, OR ANY OTHER WORLD FLIPPING SHOULD BE IN WORLD.CPP dec 19 2025
// void World::convertToEngineSpace()
// {
//     // empty on purpose dec 19 2025 
// }

void World::rebuildChunks()
{
    chunks.clear();

    for (auto& b : blocks) {
        glm::ivec3 c = chunkCoord(b.pos, chunkSize);
        chunks[c].blocks.push_back(&b);
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

    for (int x = -1; x <= 1; x++)
    for (int z = -1; z <= 1; z++)
    {
        glm::ivec3 c = base + glm::ivec3(x, 0, z);
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

void World::finalize()
{
    glm::mat3 basis = blenderToEngineBasis();

    for (auto& b : blocks) {
        glm::mat3 R = eulerXYZDegToMat3(b.rotEuler);
        b.rot = basis * R;
    }
}

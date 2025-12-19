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
            glm::radians(90.0f),
            glm::vec3(1,0,0)
        )
    );
}

static glm::mat3 eulerXYZDegToMat3(const glm::vec3& deg)
{
    
    // old one 
    // glm::vec3 r = glm::radians(deg);

    // Blender → Engine axis remap
    // Blender: X right, Y forward, Z up
    // Engine:  X right, Y up,      Z forward
    // glm::vec3 r = glm::radians(glm::vec3(
    //     deg.x,  // X stays X
    //     deg.z,  // Blender Z (up) → Engine Y
    //     deg.y   // Blender Y (forward) → Engine Z
    // ));

    glm::vec3 r = glm::radians(glm::vec3(
        // test 2
        // ehh
        // test 6 only add it to x and z
        deg.x + 90.0f, 
        // test 5 
        // only add it to z and y?no thats bad
        // deg.x,
        // dec 19 2025 all tests
        // old was just x z y, we are adding stuff to fix maybe
        // test 1
        // good?
        // deg.z + 90.0f,
        // test 4 add 90 to them all
        // deg.z + 90.0f,
        deg.z,
        // test 3 
        // Y IS PERFECT BECAUSE WE ADDED 90
        deg.y + 90.0f   
    ));

    glm::mat3 Rx = glm::mat3(glm::rotate(glm::mat4(1.0f), r.x, glm::vec3(1,0,0)));
    glm::mat3 Ry = glm::mat3(glm::rotate(glm::mat4(1.0f), r.y, glm::vec3(0,1,0)));
    glm::mat3 Rz = glm::mat3(glm::rotate(glm::mat4(1.0f), r.z, glm::vec3(0,0,1)));

    // z first
    // bad
    // return Rz * Ry * Rx;
    // bad
    // return Rz * Rx * Ry;

    // y first
    // better but bad?
    // return Ry * Rx * Rz;
    // i dont think it changes bad
    // return Ry * Rz * Rx;

    // x first
    // bad 
    // dec 19 2025 keep this as it bc its simple x y z
    // the old one is y x z but idk
    return Rx * Ry * Rz;
    // bad doesnt change anthing
    // return Rx * Rz * Ry;

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
    glm::mat3 B = blenderToEngineBasis();

    for (auto& b : blocks) {
        glm::mat3 R = eulerXYZDegToMat3(b.rotEuler);

        // convert from Blender frame → Engine frame
        b.rot = B * R * glm::transpose(B);
    }
}

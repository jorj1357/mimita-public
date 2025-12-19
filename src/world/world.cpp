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
#include "world/coord.h"

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

// --------------------
// World methods
// --------------------

void World::rebuildChunks()
{
    chunks.clear();

    for (auto& b : blocks) {
        glm::vec3 pe = toYUp(b.pos);                // <-- key fix
        glm::ivec3 c = chunkCoord(pe, chunkSize);
        chunks[c].blocks.push_back(&b);
    }

    for (auto& s : spheres) {
        glm::vec3 pe = toYUp(s.pos);                // <-- key fix
        glm::ivec3 c = chunkCoord(pe, chunkSize);
        chunks[c].spheres.push_back(&s);
    }

    printf("[WORLD] chunks=%zu blocks=%zu spheres=%zu\n",
        chunks.size(), blocks.size(), spheres.size());
}

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

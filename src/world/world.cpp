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

static glm::vec3 blenderPosToEngine(const glm::vec3& p)
{
    // Blender: X right, Y forward, Z up
    // Engine:  X right, Y up,      Z forward
    return glm::vec3(p.x, p.z, -p.y);
}

static glm::vec3 blenderRotToEngine(const glm::vec3& r)
{
    return glm::vec3(r.x, r.z, -r.y);
}

void World::convertToEngineSpace()
{
    for (auto& b : blocks) {
        b.pos = blenderPosToEngine(b.pos);
        b.rot = blenderRotToEngine(b.rot);
    }

    for (auto& s : spheres) {
        s.pos = blenderPosToEngine(s.pos);
    }
}

// --------------------
// World methods
// --------------------

// toYUp THE ONLY CALL OF toYUp, blenderPosToEngine, blenderRotToEngine, convertToEngineSpace, OR ANY OTHER WORLD FLIPPING SHOULD BE IN WORLD.CPP dec 19 2025
void World::convertToEngineSpace()
{
    for (auto& b : blocks) {
        b.pos = toYUp(b.pos);
        // b.rot stays the same if it's already degrees
        // size usually stays the same
    }

    for (auto& s : spheres) {
        s.pos = toYUp(s.pos);
    }
}

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

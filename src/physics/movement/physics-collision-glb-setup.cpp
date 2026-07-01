#include "physics/movement/physics-collision-shared.h"

#include <vector>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"

// testing this 7 1 2026 bc cant
// on funworld map i cant go up onto teh slope like right at the spawn
// and its lagginga nd stuff but whateve bro whatverr bro 
std::vector<int> gatherGLBTriangles(
    const World& world,
    const Capsule& cap,
    const glm::vec3& move
) {
    std::vector<int> out;

    AABB sweepBounds = makeSweptCapsuleAABB(cap, move);

    // TEST: give broadphase more room to find embedded ramp triangles.
    constexpr float EXTRA_XY = 1.0f;
    constexpr float EXTRA_Z_DOWN = 0.5f;
    constexpr float EXTRA_Z_UP = 0.5f;

    sweepBounds.min.x -= EXTRA_XY;
    sweepBounds.min.y -= EXTRA_XY;
    sweepBounds.max.x += EXTRA_XY;
    sweepBounds.max.y += EXTRA_XY;
    sweepBounds.min.z -= EXTRA_Z_DOWN;
    sweepBounds.max.z += EXTRA_Z_UP;

    appendChunkTrianglesForAABB(world, sweepBounds, cap.r + EXTRA_XY, out);
    return out;
}

// std::vector<int> gatherGLBTriangles(
//     const World& world,
//     const Capsule& cap,
//     const glm::vec3& move
// ) {
//     std::vector<int> out;
//     AABB sweepBounds = makeSweptCapsuleAABB(cap, move);
//     appendChunkTrianglesForAABB(world, sweepBounds, cap.r, out);
//     return out;
// }

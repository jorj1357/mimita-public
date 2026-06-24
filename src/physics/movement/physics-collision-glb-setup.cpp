#include "physics/movement/physics-collision-shared.h"

#include <vector>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"

std::vector<int> gatherGLBTriangles(
    const World& world,
    const Capsule& cap,
    const glm::vec3& move
) {
    std::vector<int> out;
    AABB sweepBounds = makeSweptCapsuleAABB(cap, move);
    appendChunkTrianglesForAABB(world, sweepBounds, cap.r, out);
    return out;
}



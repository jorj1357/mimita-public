#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "physics/movement/physics-collision-shared.h"
#include "physics/physics-types.h"

// Thread-local scratch buffers for physics collision functions.
// Eliminates ~10,000 heap allocations per frame by reusing buffers.
// Usage: auto& scratch = physicsScratch();
//        gatherGLBTriangles(scratch.ints, world, cap, move, caller);
struct PhysicsScratch {
    std::vector<int> ints;
    std::vector<int> ints2;
    std::vector<int> ints3;
    std::vector<RecoveryContact> contacts;
    std::vector<glm::vec3> vec3s;
    std::vector<glm::vec3> vec3s2;
    std::vector<SweepHit> sweepHits;
};

PhysicsScratch& physicsScratch();

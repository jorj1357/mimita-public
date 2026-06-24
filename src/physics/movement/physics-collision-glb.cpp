#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_set>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"
#include "entities/player.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "config/player-settings.h"
#include "perf/perf.h"
#include "physics/movement/physics-collision.h"

// =====================================================
// DEBUG TOGGLE
// =====================================================
#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

std::vector<int> gatherGLBTrianglesForSphere(
    const World& world,
    glm::vec3 center,
    float radius,
    const glm::vec3& move
) {
    std::vector<int> out;
    AABB sweepBounds;
    sweepBounds.min = glm::min(center, center + move) - glm::vec3(radius);
    sweepBounds.max = glm::max(center, center + move) + glm::vec3(radius);
    appendChunkTrianglesForAABB(world, sweepBounds, radius, out);
    return out;
}



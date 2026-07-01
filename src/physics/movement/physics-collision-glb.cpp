#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"

#include <chrono>
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

#define SPHERE_LOG(...) Debug::logThrottled(Debug::Category::Collision, "sphere-gather", 1.0f, __VA_ARGS__)

std::vector<int> gatherGLBTrianglesForSphere(
    const World& world,
    glm::vec3 center,
    float radius,
    const glm::vec3& move,
    const char* caller
) {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<int> out;
    AABB sweepBounds;
    sweepBounds.min = glm::min(center, center + move) - glm::vec3(radius);
    sweepBounds.max = glm::max(center, center + move) + glm::vec3(radius);
    appendChunkTrianglesForAABB(world, sweepBounds, radius, out, "gatherGLBTrianglesForSphere");
    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    if (caller) {
        SPHERE_LOG("[GATHER_SPHERE] caller=%s center=(%.1f %.1f %.1f) radius=%.2f candidates=%zu elapsedMs=%.2f\n",
                   caller, center.x, center.y, center.z, radius, out.size(), elapsedMs);
    }
    return out;
}



#include "physics/movement/physics-collision-shared.h"

#include <chrono>
#include <vector>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "world/world.h"
#include "debug/debug-log.h"

#define BROAD_LOG(...) Debug::logThrottled(Debug::Category::Collision, "broadphase", 1.0f, __VA_ARGS__)

std::vector<int> gatherGLBTriangles(
    const World& world,
    const Capsule& cap,
    const glm::vec3& move,
    const char* caller
) {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<int> out;

    AABB sweepBounds = makeSweptCapsuleAABB(cap, move);

    constexpr float EXTRA_XY = 1.0f;
    constexpr float EXTRA_Z_DOWN = 0.5f;
    constexpr float EXTRA_Z_UP = 0.5f;

    sweepBounds.min.x -= EXTRA_XY;
    sweepBounds.min.y -= EXTRA_XY;
    sweepBounds.max.x += EXTRA_XY;
    sweepBounds.max.y += EXTRA_XY;
    sweepBounds.min.z -= EXTRA_Z_DOWN;
    sweepBounds.max.z += EXTRA_Z_UP;

    appendChunkTrianglesForAABB(world, sweepBounds, cap.r + EXTRA_XY, out, "gatherGLBTriangles");

    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    if (caller) {
        BROAD_LOG(
            "[GATHER] caller=%s candidates=%zu totalTris=%zu aabb=(%.1f %.1f %.1f)-(%.1f %.1f %.1f) elapsedMs=%.2f\n",
            caller, out.size(), world.collisionMesh.triangles.size(),
            sweepBounds.min.x, sweepBounds.min.y, sweepBounds.min.z,
            sweepBounds.max.x, sweepBounds.max.y, sweepBounds.max.z,
            elapsedMs);
    }

    if (out.size() > 500) {
        Debug::warn(Debug::Category::Collision,
            "[GATHER WARNING] caller=%s candidates=%zu exceeds threshold 500\n",
            caller ? caller : "?", out.size());
    }

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

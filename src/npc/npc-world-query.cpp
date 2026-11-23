// 2026-11-09
// Thin world-query adapter for hot NPC perception. The module owns the sense
// policy; this file owns only the immutable world access and shared ray.
#include "npc/npc-world-query.h"

#include <cmath>

#include <glm/glm.hpp>

#include "physics/movement/physics-raycast.h"

namespace {
thread_local const World* gWorld = nullptr;
}

void setNpcWorldQueryWorld(const World* world)
{
    gWorld = world;
}

bool npcWorldLineOfSightBlocked(const float from[3], const float to[3])
{
    if (!gWorld || !from || !to)
        return false;
    const glm::vec3 fromPos(from[0], from[1], from[2]);
    const glm::vec3 toPos(to[0], to[1], to[2]);
    glm::vec3 direction = toPos - fromPos;
    const float segmentLength = glm::length(direction);
    if (segmentLength <= 0.5f)
        return false;
    direction /= segmentLength;
    float hitDistance = segmentLength;
    if (!rayTraverseGridCells(*gWorld, fromPos, direction, segmentLength,
                              hitDistance, nullptr))
        return false;
    return hitDistance > 0.1f && hitDistance < segmentLength - 0.5f;
}

bool npcWorldRaycastBlocked(const float from[3], const float direction[3],
                            float maxDistance, float* outDistance)
{
    if (!gWorld || !from || !direction || maxDistance <= 0.0f)
        return false;
    const glm::vec3 origin(from[0], from[1], from[2]);
    glm::vec3 dir(direction[0], direction[1], direction[2]);
    const float directionLength = glm::length(dir);
    if (directionLength <= 0.0001f)
        return false;
    dir /= directionLength;
    float rayHitDistance = maxDistance;
    const bool hit = rayTraverseGridCells(*gWorld, origin, dir, maxDistance,
                                          rayHitDistance, nullptr);
    if (outDistance)
        *outDistance = rayHitDistance;
    return hit;
}

// 2026-11-09
// Thin world-query adapter for hot NPC perception. The module owns the sense
// policy; this file owns only the immutable world access and shared ray.
#include "npc/npc-world-query.h"

#include <cmath>

#include <glm/glm.hpp>

#include "physics/movement/physics-collision.h"
#include "world/world.h"

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
    const float distance = glm::length(direction);
    if (distance <= 0.5f)
        return false;
    direction /= distance;
    float hitDistance = distance;
    if (!rayTraverseGridCells(*gWorld, fromPos, direction, distance,
                              hitDistance, nullptr))
        return false;
    return hitDistance > 0.1f && hitDistance < distance - 0.5f;
}

bool npcWorldRaycastBlocked(const float from[3], const float direction[3],
                            float maxDistance, float* outDistance)
{
    if (!gWorld || !from || !direction || maxDistance <= 0.0f)
        return false;
    const glm::vec3 origin(from[0], from[1], from[2]);
    glm::vec3 dir(direction[0], direction[1], direction[2]);
    const float length = glm::length(dir);
    if (length <= 0.0001f)
        return false;
    dir /= length;
    float hitDistance = maxDistance;
    const bool hit = rayTraverseGridCells(*gWorld, origin, dir, maxDistance,
                                          hitDistance, nullptr);
    if (outDistance)
        *outDistance = hitDistance;
    return hit;
}

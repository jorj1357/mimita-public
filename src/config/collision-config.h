// 09 17 2026
/* purpose
* Requests the hot collision-policy snapshot used by player/world response.
* Bounce tuning is owned by the replaceable game DLL, not JSON.
* Does NOT build collision meshes, own the world, or apply physics.
*/
#pragma once

#include <cstdint>

struct CollisionBouncePolicy
{
    bool enabled = false;
    float strength = 0.0f;
    float friction = 0.5f;
    float minSpeed = 7.0f;
    float maxSpeed = 45.0f;
    float cooldown = 0.05f;
};

const CollisionBouncePolicy& currentCollisionBouncePolicy(std::uint64_t simulationTick);

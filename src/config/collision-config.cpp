// 09 17 2026
/* purpose
* Requests the hot collision-policy snapshot used by player/world response.
* Keeps a safe compiled fallback when no hot collision module handles it.
*/

#include "config/collision-config.h"

#include <algorithm>

#include "hot-reload/hot-movement-policy.h"
#include "live-code/live-behavior.h"

namespace {

CollisionBouncePolicy gPolicy{};
std::uint64_t gPolicyTick = ~std::uint64_t{0};

} // namespace

const CollisionBouncePolicy& currentCollisionBouncePolicy(std::uint64_t simulationTick)
{
    if (simulationTick == gPolicyTick)
        return gPolicy;

    gPolicy = CollisionBouncePolicy{};

    CollisionPolicyV1 policy{};
    policy.bounceEnabled = gPolicy.enabled ? 1u : 0u;
    policy.bounceStrength = gPolicy.strength;
    policy.bounceFriction = gPolicy.friction;
    policy.bounceMinSpeed = gPolicy.minSpeed;
    policy.bounceMaxSpeed = gPolicy.maxSpeed;
    policy.bounceCooldown = gPolicy.cooldown;

    if (LiveBehavior::dispatchGameplayEvent64(
            GAME_EVENT_COLLISION_POLICY,
            &policy,
            sizeof(policy),
            simulationTick, 0, 0) && policy.handled)
    {
        gPolicy.enabled = policy.bounceEnabled != 0;
        gPolicy.strength = std::max(0.0f, policy.bounceStrength);
        gPolicy.friction = std::clamp(policy.bounceFriction, 0.0f, 1.0f);
        gPolicy.minSpeed = std::max(0.0f, policy.bounceMinSpeed);
        gPolicy.maxSpeed = std::max(gPolicy.minSpeed, policy.bounceMaxSpeed);
        gPolicy.cooldown = std::max(0.0f, policy.bounceCooldown);
    }

    gPolicyTick = simulationTick;
    return gPolicy;
}

// 09 16 2026
/* purpose
* movement.collision-policy: hot collision tunables (capsule size, grounded
* velocity epsilon, skin). Dispatched by the cold capsule solver. The solve
// algorithm itself can be replaced via the physics.capsuleSolve capability.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/packages/collision/collision-abi.h"

namespace {

// Bounce tuning is owned once by the hot collision kernel (single owner); the
// cold response path reads it through this event instead of a second copy.
void MIMITA_GAME_CALL onCollisionPolicy(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<CollisionPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1u;
    // Default: keep the cold-computed values. Edit live to retune.
    p->outRadius = p->radius;
    p->outHalfHeight = p->halfHeight;
    p->outGroundedVelocityEpsilon = p->groundedVelocityEpsilon;
    p->outSkin = p->skin;
    p->bounceEnabled = HotCollisionPackage::kBounceEnabled ? 1u : 0u;
    p->bounceStrength = HotCollisionPackage::kBounceStrength;
    p->bounceFriction = HotCollisionPackage::kBounceFriction;
    p->bounceMinSpeed = HotCollisionPackage::kBounceMinSpeed;
    p->bounceMaxSpeed = HotCollisionPackage::kBounceMaxSpeed;
    p->bounceCooldown = HotCollisionPackage::kBounceCooldown;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_collisionPolicyRegistration{
    {GAME_EVENT_COLLISION_POLICY, 0, 0, onCollisionPolicy,
     "movement.collision-policy"}};

#endif

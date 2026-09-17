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

namespace {

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
}

} // namespace

const MimitaHotPackage::EventRegistrar s_collisionPolicyRegistration{
    {GAME_EVENT_COLLISION_POLICY, 0, 0, onCollisionPolicy,
     "movement.collision-policy"}};

#endif

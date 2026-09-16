// 09 16 2026
/* purpose
* actor.lifecycle-policy: the hot decision for respawn placement and timing.
* Default: respawn at the cold-chosen position.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

namespace {

void MIMITA_GAME_CALL onActorLifecyclePolicy(void* /*host*/, const GameEventV1* event)
{
    auto* p = event ? static_cast<ActorLifecyclePolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;
    p->handled = 1u;
    p->respawn = 1u;
    p->position[0] = p->chosenPosition[0];
    p->position[1] = p->chosenPosition[1];
    p->position[2] = p->chosenPosition[2];
    p->yaw = p->chosenYaw;
    p->spawnProtectionSeconds = 0.0f;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_actorLifecyclePolicyRegistration{
    {GAME_EVENT_ACTOR_LIFECYCLE_POLICY, 0, 0, onActorLifecyclePolicy,
     "actor.lifecycle-policy"}};

#endif

// 09 16 2026
/* purpose
* actor.lifecycle-policy: the hot decision for respawn placement, timing, and
* spawn protection. Spawn protection is expressed in fixed 60 Hz ticks and
* applies to every actor (players and NPCs) because it is stored on the actor
* entity and enforced generically by the hot damage policy.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-package.h"

namespace {

// 60 ticks = 1 second at the fixed 60 Hz gameplay simulation rate. Edit live.
constexpr std::uint32_t kSpawnProtectionTicks = 60;

void MIMITA_GAME_CALL onActorLifecyclePolicy(void* host, const GameEventV1* event)
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
    p->spawnProtectionTicks = kSpawnProtectionTicks;

    // Arm spawn protection on the actor entity so the damage policy can ignore
    // incoming damage for the protection window.
    auto* ctx = static_cast<GameplayContextV1*>(host);
    if (ctx && ctx->dynamicWriteComponent && p->actorEntity != 0 &&
        kSpawnProtectionTicks > 0)
    {
        HotSpawnProtectionV1 sp{};
        sp.untilTick = (std::uint32_t)ctx->tick + kSpawnProtectionTicks;
        ctx->dynamicWriteComponent(ctx->host, p->actorEntity,
                                   HOT_SPAWN_PROTECTION_COMPONENT, &sp,
                                   sizeof(sp));
    }
}

} // namespace

const MimitaHotPackage::EventRegistrar s_actorLifecyclePolicyRegistration{
    {GAME_EVENT_ACTOR_LIFECYCLE_POLICY, 0, 0, onActorLifecyclePolicy,
     "actor.lifecycle-policy"}};

#endif

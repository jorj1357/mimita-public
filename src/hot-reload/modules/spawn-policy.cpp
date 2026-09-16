// 09 16 2026
/* purpose
* actor.spawn-policy: the hot decision for whether/where an actor spawns.
* Default: never spawn a startup NPC on a player spawn point (it overlaps the
* player and kills them at spawn). Live-editable.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

namespace {

void MIMITA_GAME_CALL onActorSpawnPolicy(void* host, const GameEventV1* event)
{
    auto* p = event ? static_cast<ActorSpawnPolicyV1*>(event->payload) : nullptr;
    if (!p)
        return;

    p->handled = 1u;
    p->position[0] = p->chosenPosition[0];
    p->position[1] = p->chosenPosition[1];
    p->position[2] = p->chosenPosition[2];
    p->yaw = p->chosenYaw;

    if (p->kind == GAME_ACTOR_SPAWN_NPC)
    {
        // Startup NPCs share the player spawn points, so they spawn inside the
        // player. Suppress them; flip this to 0 to re-enable NPC spawns live.
        p->suppress = 1u;
    }
    else
    {
        p->suppress = 0u;
    }
}

} // namespace

const MimitaHotPackage::EventRegistrar s_actorSpawnPolicyRegistration{
    {GAME_EVENT_ACTOR_SPAWN_POLICY, 0, 0, onActorSpawnPolicy,
     "actor.spawn-policy"}};

#endif

// 09 23 2026
/* purpose
* Establish the generic hot actor lifecycle boundary.
* This phase intentionally preserves the incoming state; later phases move
* avatar selection and spawn/respawn policy into this handler.
* Does NOT own EXE storage, networking, or Player/Npc object layouts.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-package.h"

namespace {

void MIMITA_GAME_CALL onActorLifecycle(void* /*host*/, const GameEventV1* event)
{
    auto* state = event ? static_cast<ActorLifecycleStateV1*>(event->payload) : nullptr;
    if (!state)
        return;

    // Boundary proof only. The state is deliberately unchanged so this phase
    // cannot alter existing NPC/player behavior.
    state->handled = 1u;
}

} // namespace

const MimitaHotPackage::EventRegistrar s_actorLifecycleRegistration{
    {GAME_EVENT_ACTOR_LIFECYCLE, 0, 0, onActorLifecycle,
     "actor.lifecycle"}};

#endif

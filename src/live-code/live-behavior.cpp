// 09 12 2026
/* purpose
* Implements the generic behavior event bridge to the hot gameplay module.
* Does NOT own gameplay policy or state.
*/
#include "live-code/live-behavior.h"

#include "hot-reload/hot-reload-system.h"
#include "live-code/live-modules.h"

namespace {

const GameGameplayModuleV1* gameplayModule()
{
    return static_cast<const GameGameplayModuleV1*>(
        LiveModules::findFunctions("gameplay", sizeof(GameGameplayModuleV1)));
}

} // namespace

namespace LiveBehavior {

bool available()
{
    const GameGameplayModuleV1* module = gameplayModule();
    return module && module->onEvent != nullptr;
}

bool dispatchDamagePolicy(DamagePolicyV1& payload, std::uint64_t tick)
{
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->onEvent)
        return false;

    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    payload.handled = 0;

    GameEventV1 event{};
    event.typeId = GAME_EVENT_DAMAGE_POLICY;
    event.payloadVersion = GAMEPLAY_EVENT_VERSION;
    event.payloadSize = sizeof(DamagePolicyV1);
    event.flags = 0;
    event.sourceEntity = payload.attackerEntity;
    event.targetEntity = payload.victimEntity;
    event.projectileEntity = payload.projectileEntity;
    event.tick = tick;
    event.payload = &payload;

    GameplayContextV1 context{};
    context.host = nullptr;
    context.tick = tick;
    context.generation = status.activeGeneration;
    context.codeHash = 0;
    context.emitEvent = nullptr;
    context.findEntities = nullptr;

    module->onEvent(&event, &context);
    return payload.handled != 0;
}

} // namespace LiveBehavior

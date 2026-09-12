// 09 12 2026
/* purpose
* Implements the EXE-side bridge to the hot gameplay policy module.
* Owns ABI-safe module invocation only.
*/
#include "live-code/live-gameplay.h"

#include "hot-reload/hot-reload-system.h"
#include "live-code/live-modules.h"

namespace {

const GameGameplayModuleV1* gameplayModule()
{
    return static_cast<const GameGameplayModuleV1*>(
        LiveModules::findFunctions("gameplay", sizeof(GameGameplayModuleV1)));
}

} // namespace

namespace LiveGameplay {

bool rocketFlight(const RocketFlightStateV1& state,
                  const RocketFlightParamsV1& base,
                  RocketFlightParamsV1& out)
{
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->adjustRocketFlight)
        return false;
    out = base;
    return module->adjustRocketFlight(
        &state, &base, &out, &HotReloadSystem::instance().gameMemory());
}

bool explosion(const ExplosionStateV1& state,
               const ExplosionParamsV1& base,
               ExplosionParamsV1& out)
{
    const GameGameplayModuleV1* module = gameplayModule();
    if (!module || !module->explosionParameters)
        return false;
    out = base;
    return module->explosionParameters(
        &state, &base, &out, &HotReloadSystem::instance().gameMemory());
}

} // namespace LiveGameplay

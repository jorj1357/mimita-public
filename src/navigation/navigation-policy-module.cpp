// 2026-11-08
/* purpose
* hot-reloadable navigation rebuild policy
* host owns Recast/Detour build and query mechanics
* this file DOES NOT own collision geometry, actors, or movement
*/
#include "modules/api/navigation-policy-api.h"

#include <algorithm>

#include <module-identity.h>

namespace {

const mimita_module::HostServicesV1* gServices = nullptr;

void decide(mimita_navigation_policy::NavigationPolicyV1& p)
{
    // Static map geometry is the navigation authority. Streamed/dynamic
    // collision must never force the published base mesh to disappear or to
    // rebuild in the fixed tick. Keep the old mesh and let the host's custom
    // planner cover temporary holes.
    p.outStaticOnly = 1;
    p.outAllowRebuild = p.hasMesh
        ? (p.dirtyTiles > 0 ? 1u : 0u)
        : (p.inFixedTick ? 0u : 1u);
    p.outTilesPerUpdate = p.hasMesh ? 1u : 0u;
    p.outMaxBuildMs = p.hasMesh ? 4.0f : 0.0f;
    p.outGeneration = MIMITA_MODULE_GENERATION;
}

int MIMITA_MODULE_CALL initialize(const mimita_module::HostServicesV1* services)
{
    if (!services || services->version != mimita_module::ABI_VERSION ||
        services->structSize != sizeof(*services) || !services->log)
        return 0;
    gServices = services;
    return 1;
}

void MIMITA_MODULE_CALL shutdown()
{
    gServices = nullptr;
}

int MIMITA_MODULE_CALL onEvent(mimita_module::ModuleEventV1* event)
{
    if (!event || !event->payload)
        return 0;
    if (event->typeId == mimita_module::EVENT_NAVIGATION_POLICY &&
        event->payloadSize ==
            sizeof(mimita_navigation_policy::NavigationPolicyV1)) {
        auto& p = *static_cast<
            mimita_navigation_policy::NavigationPolicyV1*>(event->payload);
        if (p.version != mimita_navigation_policy::ABI_VERSION ||
            p.structSize != sizeof(p))
            return 0;
        decide(p);
        return 1;
    }
    return 0;
}

std::uint32_t MIMITA_MODULE_CALL exportState(std::uint8_t*, std::uint32_t)
{
    return 0;
}

int MIMITA_MODULE_CALL importState(const std::uint8_t*, std::uint32_t size)
{
    return size == 0 ? 1 : 0;
}

const mimita_module::ModuleV1 gModule = {
    {mimita_module::ABI_VERSION, sizeof(mimita_module::ModuleV1),
     "navigation-policy", MIMITA_MODULE_GENERATION,
     MIMITA_MODULE_SOURCE_HASH, MIMITA_MODULE_STATE_SCHEMA},
    initialize, shutdown, onEvent, exportState, importState
};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion)
{
    return abiVersion == mimita_module::ABI_VERSION ? &gModule.base : nullptr;
}

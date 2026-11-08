#include "modules/api/network-policy-api.h"

#include <algorithm>

#include <module-identity.h>

namespace {

const mimita_module::HostServicesV1* gServices = nullptr;

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
    if (event->typeId != mimita_module::EVENT_NETWORK_RECONNECT_POLICY ||
        event->payloadSize != sizeof(mimita_network_policy::ReconnectPolicyV1))
        return 0;
    auto& p = *static_cast<mimita_network_policy::ReconnectPolicyV1*>(
        event->payload);
    if (p.version != mimita_network_policy::ABI_VERSION ||
        p.structSize != sizeof(p))
        return 0;
    const bool partialAlive = p.receivedChunks > 0 ||
        (p.expectedChunks > 0 && p.receivedChunks < p.expectedChunks);
    p.action = partialAlive ? 1 : 0;
    p.backoffMs = 1000;
    p.retryIntervalMs = 1000;
    p.graceMs = 10000;
    p.result = 1;
    return 1;
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
     "network-reconnect-policy", MIMITA_MODULE_GENERATION,
     MIMITA_MODULE_SOURCE_HASH, MIMITA_MODULE_STATE_SCHEMA},
    initialize, shutdown, onEvent, exportState, importState
};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion)
{
    return abiVersion == mimita_module::ABI_VERSION ? &gModule.base : nullptr;
}

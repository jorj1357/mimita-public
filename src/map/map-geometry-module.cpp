// 2026-11-08
#include "modules/api/map-geometry-api.h"

#include <cstdint>

#include "modules/api/module-abi.h"
#include <module-identity.h>

namespace {

int signature(mimita_map_geometry::SignatureV1& request)
{
    if (request.signatureVersion != mimita_map_geometry::SIGNATURE_VERSION)
        return 0;
    if (request.byteCount != 0 && !request.bytes)
        return 0;
    std::uint64_t hash = 1469598103934665603ull;
    for (std::uint32_t i = 0; i < request.byteCount; ++i) {
        hash ^= static_cast<unsigned char>(request.bytes[i]);
        hash *= 1099511628211ull;
    }
    request.outSignature = hash ? hash : 1ull;
    request.result = 1;
    return 1;
}

int onEvent(mimita_module::ModuleEventV1* event)
{
    if (!event || !event->payload)
        return 0;
    if (event->typeId == mimita_module::EVENT_MAP_GEOMETRY_SIGNATURE &&
        event->payloadSize == sizeof(mimita_map_geometry::SignatureV1))
        return signature(*static_cast<mimita_map_geometry::SignatureV1*>(
            event->payload));
    return 0;
}

int initialize(const mimita_module::HostServicesV1* host)
{
    return host && host->version == mimita_module::ABI_VERSION &&
           host->structSize == sizeof(*host);
}

void shutdown() {}

std::uint32_t exportState(std::uint8_t*, std::uint32_t) { return 0; }
int importState(const std::uint8_t*, std::uint32_t size) { return size == 0; }

const mimita_module::ModuleV1 module = {
    {mimita_module::ABI_VERSION, sizeof(mimita_module::ModuleV1),
     "map-geometry", MIMITA_MODULE_GENERATION, MIMITA_MODULE_SOURCE_HASH,
     MIMITA_MODULE_STATE_SCHEMA},
    initialize, shutdown, onEvent, exportState, importState};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t version)
{
    return version == mimita_module::ABI_VERSION ? &module.base : nullptr;
}

// 2026-11-13
// Game-DLL read-only world capability bridge.

#include "game/world-bridge-module.h"

namespace mimita::game {
namespace {

const mimita::host::WorldCapabilityV1* gWorld = nullptr;
void* gHostData = nullptr;

}  // namespace

void initializeHostWorld(const mimita::host::HostServicesV1* services) {
    gWorld = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability = static_cast<const mimita::host::WorldCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::World),
            mimita::host::kWorldCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kWorldCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::WorldCapabilityV1)) {
        gWorld = capability;
    }
}

mimita::host::HostResult readWorldInfo(mimita::host::WorldInfoV1* out) {
    if (!gWorld || !gWorld->info || !out) {
        return mimita::host::HostResult::Unsupported;
    }
    return gWorld->info(gHostData, out);
}

mimita::host::HostResult castHostWorldRay(const float origin[3],
                                          const float direction[3],
                                          float maxDistance,
                                          mimita::host::WorldRayHitV1* out) {
    if (!gWorld || !gWorld->raycast || !origin || !direction || !out) {
        return mimita::host::HostResult::Unsupported;
    }
    return gWorld->raycast(gHostData, origin, direction, maxDistance, out);
}

mimita::host::HostResult pickHostSpawn(std::uint32_t playerId,
                                       mimita::host::WorldSpawnV1* out) {
    if (!gWorld || !gWorld->pick_spawn || !out) {
        return mimita::host::HostResult::Unsupported;
    }
    return gWorld->pick_spawn(gHostData, playerId, out);
}

}  // namespace mimita::game

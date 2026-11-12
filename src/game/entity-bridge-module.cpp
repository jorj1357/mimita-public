// 2026-11-12
// Game-DLL state phone for CAP_ENTITY.
//
// Reads and writes bounded resident records by stable handle. No live C++
// gameplay object crosses the boundary.

#include "game/entity-bridge-module.h"

namespace mimita::game {

namespace {
const mimita::host::EntityCapabilityV1* gEntity = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostEntity(const mimita::host::HostServicesV1* services) {
    gEntity = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability = static_cast<const mimita::host::EntityCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Entity),
            mimita::host::kEntityCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kEntityCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::EntityCapabilityV1)) {
        gEntity = capability;
    }
}

mimita::host::HostResult readHostEntity(mimita::host::EntityHandle handle,
                                        mimita::host::EntitySnapshotV1* out) {
    if (!gEntity || !gEntity->read || !out) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEntity->read(gHostData, handle, out);
}

mimita::host::HostResult writeHostEntity(
    mimita::host::EntityHandle handle,
    const mimita::host::EntitySnapshotV1* in) {
    if (!gEntity || !gEntity->write || !in) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEntity->write(gHostData, handle, in);
}

}  // namespace mimita::game

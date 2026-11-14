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

std::uint32_t listHostEntities(mimita::host::EntityKind kind,
                               mimita::host::EntityHandle* out,
                               std::uint32_t capacity) {
    if (!gEntity || !gEntity->list || !out || capacity == 0) {
        return 0;
    }
    return gEntity->list(gHostData, static_cast<std::int32_t>(kind), out,
                         capacity);
}

mimita::host::HostResult createHostEntity(mimita::host::EntityKind kind,
                                          mimita::host::EntityHandle* out) {
    if (!gEntity || !gEntity->create || !out) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEntity->create(gHostData, static_cast<std::int32_t>(kind), out);
}

mimita::host::HostResult resetHostTransform(
    mimita::host::EntityHandle handle, const float position[3],
    const float velocity[3], float yaw, std::uint32_t onGround,
    std::uint32_t invulnerabilityTicks, std::uint32_t maxAirJumps,
    std::uint32_t awaitAck) {
    if (!gEntity || !gEntity->reset_transform) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEntity->reset_transform(gHostData, handle, position, velocity, yaw,
                                    onGround, invulnerabilityTicks,
                                    maxAirJumps, awaitAck);
}

mimita::host::HostResult findHostConnection(std::uint64_t peer,
                                            mimita::host::EntityHandle* out) {
    if (!gEntity || !gEntity->find_connection || !out) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEntity->find_connection(gHostData, peer, out);
}

mimita::host::HostResult findHostReconnectToken(
    const char* token, std::uint32_t includePrevious,
    mimita::host::EntityHandle* out) {
    if (!gEntity || !gEntity->find_reconnect_token || !out || !token) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEntity->find_reconnect_token(gHostData, token, includePrevious,
                                         out);
}

mimita::host::HostResult removeHostEntity(mimita::host::EntityHandle handle) {
    if (!gEntity || !gEntity->remove) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEntity->remove(gHostData, handle);
}

mimita::host::HostResult findHostJoinToken(const char* token,
                                           mimita::host::EntityHandle* out) {
    if (!gEntity || !gEntity->find_join_token || !out || !token) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEntity->find_join_token(gHostData, token, out);
}

mimita::host::HostResult sendHostAvatarManifests(
    mimita::host::EntityHandle receiver) {
    if (!gEntity || !gEntity->send_avatar_manifests) {
        return mimita::host::HostResult::Unsupported;
    }
    return gEntity->send_avatar_manifests(gHostData, receiver);
}

}  // namespace mimita::game

// 2026-11-12
// Game-side CAP_ENTITY helpers.

#pragma once

#include "host/abi/entity-capability.h"
#include "host/abi/game-host-abi.h"

namespace mimita::game {

void initializeHostEntity(const mimita::host::HostServicesV1* services);
mimita::host::HostResult readHostEntity(mimita::host::EntityHandle handle,
                                        mimita::host::EntitySnapshotV1* out);
mimita::host::HostResult writeHostEntity(
    mimita::host::EntityHandle handle,
    const mimita::host::EntitySnapshotV1* in);
std::uint32_t listHostEntities(mimita::host::EntityKind kind,
                               mimita::host::EntityHandle* out,
                               std::uint32_t capacity);
mimita::host::HostResult createHostEntity(mimita::host::EntityKind kind,
                                          mimita::host::EntityHandle* out);
mimita::host::HostResult resetHostTransform(
    mimita::host::EntityHandle handle, const float position[3],
    const float velocity[3], float yaw, std::uint32_t onGround,
    std::uint32_t invulnerabilityTicks, std::uint32_t maxAirJumps,
    std::uint32_t awaitAck);
mimita::host::HostResult removeHostEntity(mimita::host::EntityHandle handle);
mimita::host::HostResult findHostConnection(std::uint64_t peer,
                                            mimita::host::EntityHandle* out);
mimita::host::HostResult findHostReconnectToken(
    const char* token, std::uint32_t includePrevious,
    mimita::host::EntityHandle* out);
mimita::host::HostResult findHostJoinToken(const char* token,
                                           mimita::host::EntityHandle* out);

}  // namespace mimita::game

// 2026-11-13
// Game-side CAP_WORLD helpers.

#pragma once

#include "host/abi/game-host-abi.h"
#include "host/abi/world-capability.h"

namespace mimita::game {

void initializeHostWorld(const mimita::host::HostServicesV1* services);
mimita::host::HostResult readWorldInfo(mimita::host::WorldInfoV1* out);
mimita::host::HostResult castHostWorldRay(const float origin[3],
                                          const float direction[3],
                                          float maxDistance,
                                          mimita::host::WorldRayHitV1* out);
mimita::host::HostResult pickHostSpawn(std::uint32_t playerId,
                                       mimita::host::WorldSpawnV1* out);

}  // namespace mimita::game

// 2026-11-13
// Game-side CAP_SESSION_EVENTS helpers.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"
#include "host/abi/session-events-capability.h"

namespace mimita::game {

void initializeHostSessionEvents(const mimita::host::HostServicesV1* services);
std::uint32_t nextSessionEventId();
std::uint32_t sessionEventSessionId(std::uint32_t playerId);
mimita::host::SessionEventQueueResult queueSessionEventToPlayer(
    std::uint32_t playerId, const void* bytes, std::uint32_t size,
    std::uint32_t eventId, std::uint32_t maxPending);
mimita::host::SessionEventQueueResult queueSessionEventToAll(
    const void* bytes, std::uint32_t size, std::uint32_t eventId,
    std::uint32_t maxPending);
mimita::host::HostResult resetSessionEventPlayer(std::uint32_t playerId);

}  // namespace mimita::game

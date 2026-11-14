// 2026-11-13
// Resident reliable-event id/session owner and enqueue adapter.

#pragma once

#include <cstdint>
#include <unordered_map>

#include "host/abi/session-events-capability.h"
#include "network/server.h"

namespace mimita::host {

class HostServerSessionEvents {
public:
    static HostServerSessionEvents& instance();

    void bindPlayers(
        std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players);
    void clear();

    const SessionEventsCapabilityV1* capability() const { return &capability_; }
    void* owner() { return this; }

private:
    HostServerSessionEvents();

    static std::uint32_t nextEventIdThunk(void* owner);
    static std::uint32_t sessionIdThunk(void* owner, std::uint32_t playerId);
    static SessionEventQueueResult queueToPlayerThunk(
        void* owner, std::uint32_t playerId, const std::uint8_t* bytes,
        std::uint32_t size, std::uint32_t eventId, std::uint32_t maxPending);
    static SessionEventQueueResult queueToAllThunk(
        void* owner, const std::uint8_t* bytes, std::uint32_t size,
        std::uint32_t eventId, std::uint32_t maxPending);
    static HostResult resetPlayerThunk(void* owner, std::uint32_t playerId);

    SessionEventQueueResult queueToPlayer(std::uint32_t playerId,
                                          const std::uint8_t* bytes,
                                          std::uint32_t size,
                                          std::uint32_t eventId,
                                          std::uint32_t maxPending);

    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players_ = nullptr;
    std::uint32_t nextEventId_ = 1;
    std::uint32_t nextSessionId_ = 0x80000000u;
    SessionEventsCapabilityV1 capability_{};
};

}  // namespace mimita::host

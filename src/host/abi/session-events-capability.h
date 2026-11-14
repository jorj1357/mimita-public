// 2026-11-13
// Versioned reliable-event identity/enqueue capability.
//
// The host owns the per-player pending reliable-event queue (which lives in the
// resident ServerPlayer record) and the event/session id counters. The game
// generation only supplies bounded packet bytes and policy limits.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kSessionEventsCapabilityVersion = 2u;

enum class SessionEventQueueResult : std::int32_t {
    Ok = 0,
    Unavailable = 1,
    Saturated = 2,
    InvalidArgument = 3,
};

struct SessionEventsCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    std::uint32_t (*next_event_id)(void* hostUserData);

    // Ensures and returns the per-player reliable event session id.
    std::uint32_t (*session_id)(void* hostUserData, std::uint32_t playerId);

    // Queues into the resident player's pending queue and sends immediately.
    // `maxPending` is the game-owned backlog policy.
    SessionEventQueueResult (*queue_to_player)(
        void* hostUserData, std::uint32_t playerId,
        const std::uint8_t* bytes, std::uint32_t size, std::uint32_t eventId,
        std::uint32_t maxPending);

    SessionEventQueueResult (*queue_to_all)(
        void* hostUserData, const std::uint8_t* bytes, std::uint32_t size,
        std::uint32_t eventId, std::uint32_t maxPending);

    // Clears the pending queue and rotates the session id (reconnect path).
    HostResult (*reset_player)(void* hostUserData, std::uint32_t playerId);
};

}  // namespace mimita::host

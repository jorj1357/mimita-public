// 2026-11-13
// Resident listen-server/session capability.
//
// The resident host owns the listen-server state, socket, background thread,
// and ICE listener. The game generation receives an opaque/stable state handle
// and forwards bounded lifecycle operations.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kTransportSessionCapabilityVersion = 1u;

struct TransportSessionCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    void* (*server_state)(void* hostUserData);
    std::int32_t (*server_start)(void* hostUserData, void* state,
                                 std::uint16_t port, const char* publicIp,
                                 const char* hostSessionId, const void* settings);
    void (*server_stop)(void* hostUserData, void* state);
    void (*server_tick)(void* hostUserData, void* state, float dt);
    std::int32_t (*server_init_ice)(void* hostUserData, void* state);
    std::int32_t (*set_nonblocking)(void* hostUserData, std::uint64_t socketHandle);
    std::int32_t (*disable_udp_conn_reset)(void* hostUserData, std::uint64_t socketHandle);
    std::uint32_t (*server_host_player_id)(void* hostUserData);
    void (*server_set_host_player_id)(void* hostUserData, std::uint32_t playerId);
};

}  // namespace mimita::host

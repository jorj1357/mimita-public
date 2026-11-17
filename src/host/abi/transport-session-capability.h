// 2026-11-13
// Resident listen-server/session capability.
//
// The resident host owns the listen-server state, socket, background thread,
// and ICE listener. The game generation receives an opaque/stable state handle
// and forwards bounded lifecycle operations.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"
#include "host/abi/transport-capability.h"

namespace mimita::host {

inline constexpr std::uint32_t kTransportSessionCapabilityVersion = 2u;

struct TransportSessionCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    // Deprecated compatibility handle. Version 2 uses the opaque session
    // operations below; a cross-binary ListenServerState pointer is not part of
    // the reviewed boundary.
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

    // Version 2: opaque generation attach/poll/maintenance operations. The
    // resident core keeps the session alive across a generation swap.
    SessionHandle (*server_attach_generation)(void* hostUserData);
    void (*server_detach_generation)(void* hostUserData, SessionHandle session);
    std::int32_t (*server_poll_inbound)(void* hostUserData, SessionHandle session,
                                        TransportMessageV1* out,
                                        std::uint32_t capacity,
                                        std::uint32_t* count);
    std::int32_t (*server_poll_events)(void* hostUserData, SessionHandle session,
                                       TransportEventV1* out,
                                       std::uint32_t capacity,
                                       std::uint32_t* count);
    std::int32_t (*server_query_swap_state)(void* hostUserData,
                                            TransportSwapStateV1* out);
    std::int32_t (*server_set_maintenance_payload)(
        void* hostUserData, SessionHandle session, const std::uint8_t* bytes,
        std::uint32_t size, std::uint32_t intervalMs);
};

}  // namespace mimita::host

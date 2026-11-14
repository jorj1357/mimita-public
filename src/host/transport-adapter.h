// 2026-11-12
// Purpose: host-owned transport adapter registered by the active session owner.
//
// The adapter is the bridge between the resident transport (UDP/ICE/server
// socket) and CAP_TRANSPORT. It is synchronous and must not retain the byte
// pointer after returning. The game DLL never sees this struct.

#pragma once

#include <cstddef>
#include <cstdint>

#include "host/abi/transport-capability.h"

namespace mimita::host {

inline constexpr std::uint32_t kTransportAdapterVersion = 4u;

struct TransportAdapterV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    // Client -> server datagram (the host owns the active client transport).
    bool (*send_to_server)(void* owner, const void* data, std::size_t size);

    // Server -> one connected player (the host owns the player/socket map).
    bool (*send_to_player)(void* owner, std::uint32_t playerId, const void* data,
                           std::size_t size);

    // Server -> all connected players.
    bool (*broadcast)(void* owner, const void* data, std::size_t size);

    // Optional plain session snapshot.
    bool (*session_state)(void* owner, SessionStateV1* out);

    // Optional connected peer count.
    std::uint32_t (*peer_count)(void* owner);

    // Server -> exact source endpoint before the peer has a player record.
    // Plain network-order address/port; the owner owns the socket.
    bool (*send_to_endpoint)(void* owner, std::uint32_t addressNetworkOrder,
                             std::uint16_t portNetworkOrder, const void* data,
                             std::size_t size);

    // Server -> an already-known transport peer. The owner resolves the peer
    // to a player transport or the current inbound endpoint.
    bool (*send_to_peer)(void* owner, PeerHandle peer, const void* data,
                         std::size_t size);

    // Bind the current inbound peer/claimed transport to an existing player.
    bool (*bind_session)(void* owner, PeerHandle peer, std::uint32_t playerId);
};

}  // namespace mimita::host

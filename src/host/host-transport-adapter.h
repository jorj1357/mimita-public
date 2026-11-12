// 2026-11-12
// Purpose: resident adapter that connects CAP_TRANSPORT to the active client
// MultiplayerContext transport. It is the first T0 implementation; the server
// owner can register a different adapter with send_to_player/broadcast.
//
// This file stays in the resident host. It must not move into game.dll.

#pragma once

#include <cstdint>

#include "host/transport-adapter.h"

namespace MimitaNet {
struct MultiplayerContext;
}

namespace mimita::host {

class HostTransportAdapter {
public:
    static HostTransportAdapter& instance();

    void bind(MimitaNet::MultiplayerContext* context);
    void unbind();

    const TransportAdapterV1* adapter() const { return &adapter_; }
    void* owner() const { return context_; }

private:
    HostTransportAdapter();

    static bool sendToServer(void* owner, const void* data, std::size_t size);
    static bool sendToPlayer(void* owner, std::uint32_t playerId, const void* data,
                             std::size_t size);
    static bool broadcast(void* owner, const void* data, std::size_t size);
    static bool sessionState(void* owner, SessionStateV1* out);
    static std::uint32_t peerCount(void* owner);

    MimitaNet::MultiplayerContext* context_ = nullptr;
    TransportAdapterV1 adapter_{};
};

}  // namespace mimita::host

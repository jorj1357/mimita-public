// 2026-11-12
// Resident transport adapter for the dedicated server.
//
// The client adapter only reaches the server. This adapter owns the server ->
// player direction and is registered with HostRuntime while the server runs.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>

#include "host/transport-adapter.h"
#include "network/server.h"

namespace mimita::host {

using ServerSendFn =
    std::function<bool(std::uint32_t playerId, const void* data, std::size_t size)>;

class HostServerTransportAdapter {
public:
    static HostServerTransportAdapter& instance();

    void bindPlayers(std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players);
    void setSender(ServerSendFn sender);
    void clear();

    const TransportAdapterV1* adapter() const { return &adapter_; }
    void* owner() { return this; }

private:
    HostServerTransportAdapter();

    static bool sendToPlayer(void* owner, std::uint32_t playerId, const void* data,
                             std::size_t size);
    static bool broadcast(void* owner, const void* data, std::size_t size);
    static bool sessionState(void* owner, SessionStateV1* out);
    static std::uint32_t peerCount(void* owner);

    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players_ = nullptr;
    ServerSendFn sender_;
    TransportAdapterV1 adapter_{};
};

}  // namespace mimita::host

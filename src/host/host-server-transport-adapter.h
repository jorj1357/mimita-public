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
using ServerEndpointSendFn =
    std::function<bool(std::uint32_t addressNetworkOrder,
                       std::uint16_t portNetworkOrder, const void* data,
                       std::size_t size)>;

class HostServerTransportAdapter {
public:
    static HostServerTransportAdapter& instance();

    void bindPlayers(std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players);
    void setSender(ServerSendFn sender);
    void setEndpointSender(ServerEndpointSendFn sender);
    void noteInbound(std::uint64_t peer, std::uint32_t addressNetworkOrder,
                     std::uint16_t portNetworkOrder,
                     std::unique_ptr<IGameTransport>* claimedTransport = nullptr);
    void clearInbound();
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
    static bool sendToEndpoint(void* owner, std::uint32_t addressNetworkOrder,
                               std::uint16_t portNetworkOrder, const void* data,
                               std::size_t size);
    static bool sendToPeer(void* owner, std::uint64_t peer, const void* data,
                           std::size_t size);
    static bool bindSession(void* owner, std::uint64_t peer,
                            std::uint32_t playerId);

    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players_ = nullptr;
    ServerSendFn sender_;
    ServerEndpointSendFn endpointSender_;
    std::uint64_t inboundPeer_ = 0;
    std::uint32_t inboundAddress_ = 0;
    std::uint16_t inboundPort_ = 0;
    std::unique_ptr<IGameTransport>* claimedTransport_ = nullptr;
    TransportAdapterV1 adapter_{};
};

}  // namespace mimita::host

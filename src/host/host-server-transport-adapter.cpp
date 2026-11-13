// 2026-11-12
// Resident server transport adapter implementation.

#include "host/host-server-transport-adapter.h"

#include <cstring>

#include "network/packets.h"

namespace mimita::host {

HostServerTransportAdapter& HostServerTransportAdapter::instance() {
    static HostServerTransportAdapter adapter;
    return adapter;
}

HostServerTransportAdapter::HostServerTransportAdapter() {
    adapter_.version = kTransportAdapterVersion;
    adapter_.structSize = static_cast<std::uint32_t>(sizeof(TransportAdapterV1));
    adapter_.send_to_server = nullptr;
    adapter_.send_to_player = &HostServerTransportAdapter::sendToPlayer;
    adapter_.broadcast = &HostServerTransportAdapter::broadcast;
    adapter_.session_state = &HostServerTransportAdapter::sessionState;
    adapter_.peer_count = &HostServerTransportAdapter::peerCount;
    adapter_.send_to_endpoint = &HostServerTransportAdapter::sendToEndpoint;
    adapter_.send_to_peer = &HostServerTransportAdapter::sendToPeer;
}

void HostServerTransportAdapter::bindPlayers(
    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players) {
    players_ = players;
}

void HostServerTransportAdapter::setSender(ServerSendFn sender) {
    sender_ = std::move(sender);
}

void HostServerTransportAdapter::setEndpointSender(ServerEndpointSendFn sender) {
    endpointSender_ = std::move(sender);
}

void HostServerTransportAdapter::noteInbound(std::uint64_t peer,
                                             std::uint32_t addressNetworkOrder,
                                             std::uint16_t portNetworkOrder) {
    inboundPeer_ = peer;
    inboundAddress_ = addressNetworkOrder;
    inboundPort_ = portNetworkOrder;
}

void HostServerTransportAdapter::clearInbound() {
    inboundPeer_ = 0;
    inboundAddress_ = 0;
    inboundPort_ = 0;
}

void HostServerTransportAdapter::clear() {
    players_ = nullptr;
    sender_ = nullptr;
    endpointSender_ = nullptr;
    clearInbound();
}

bool HostServerTransportAdapter::sendToPlayer(void* owner, std::uint32_t playerId,
                                               const void* data, std::size_t size) {
    auto* adapter = static_cast<HostServerTransportAdapter*>(owner);
    if (!adapter || !adapter->sender_ || !data || size == 0) {
        return false;
    }
    return adapter->sender_(playerId, data, size);
}

bool HostServerTransportAdapter::broadcast(void* owner, const void* data,
                                            std::size_t size) {
    auto* adapter = static_cast<HostServerTransportAdapter*>(owner);
    if (!adapter || !adapter->sender_ || !adapter->players_ || !data ||
        size == 0) {
        return false;
    }
    bool sent = false;
    for (const auto& entry : *adapter->players_) {
        sent = adapter->sender_(entry.first, data, size) || sent;
    }
    return sent;
}

bool HostServerTransportAdapter::sessionState(void* owner, SessionStateV1* out) {
    auto* adapter = static_cast<HostServerTransportAdapter*>(owner);
    if (!adapter || !out || out->structSize < sizeof(SessionStateV1)) {
        return false;
    }
    std::memset(out, 0, sizeof(SessionStateV1));
    out->structSize = static_cast<std::uint32_t>(sizeof(SessionStateV1));
    out->version = kTransportCapabilityVersion;
    out->role = 1;
    out->connected = 1;
    out->peerCount = adapter->players_
        ? static_cast<std::uint32_t>(adapter->players_->size()) : 0;
    out->protocolVersion = MimitaNet::PROTOCOL_VERSION;
    return true;
}

std::uint32_t HostServerTransportAdapter::peerCount(void* owner) {
    auto* adapter = static_cast<HostServerTransportAdapter*>(owner);
    return adapter && adapter->players_
        ? static_cast<std::uint32_t>(adapter->players_->size()) : 0;
}

bool HostServerTransportAdapter::sendToEndpoint(
    void* owner, std::uint32_t addressNetworkOrder,
    std::uint16_t portNetworkOrder, const void* data, std::size_t size) {
    auto* adapter = static_cast<HostServerTransportAdapter*>(owner);
    if (!adapter || !adapter->endpointSender_ || !data || size == 0) {
        return false;
    }
    return adapter->endpointSender_(addressNetworkOrder, portNetworkOrder, data,
                                    size);
}

bool HostServerTransportAdapter::sendToPeer(void* owner, std::uint64_t peer,
                                            const void* data, std::size_t size) {
    auto* adapter = static_cast<HostServerTransportAdapter*>(owner);
    if (!adapter || !data || size == 0) {
        return false;
    }
    if (adapter->players_ && adapter->sender_) {
        for (const auto& entry : *adapter->players_) {
            if (entry.second.hasConnectionId &&
                entry.second.connectionId.value == peer) {
                return adapter->sender_(entry.first, data, size);
            }
        }
    }
    if (peer != 0 && peer == adapter->inboundPeer_ &&
        adapter->inboundAddress_ != 0 && adapter->endpointSender_) {
        return adapter->endpointSender_(adapter->inboundAddress_,
                                        adapter->inboundPort_, data, size);
    }
    return false;
}

}  // namespace mimita::host

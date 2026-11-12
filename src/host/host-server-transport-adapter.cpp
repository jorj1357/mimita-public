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
}

void HostServerTransportAdapter::bindPlayers(
    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players) {
    players_ = players;
}

void HostServerTransportAdapter::setSender(ServerSendFn sender) {
    sender_ = std::move(sender);
}

void HostServerTransportAdapter::clear() {
    players_ = nullptr;
    sender_ = nullptr;
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

}  // namespace mimita::host

// 2026-11-12
// Resident CAP_TRANSPORT adapter for the active client session.

#include "host/host-transport-adapter.h"

#include <cstring>
#include <functional>

#include "network/multiplayer-context.h"
#include "network/packets.h"

namespace mimita::host {

HostTransportAdapter& HostTransportAdapter::instance() {
    static HostTransportAdapter adapter;
    return adapter;
}

HostTransportAdapter::HostTransportAdapter() {
    adapter_.version = kTransportAdapterVersion;
    adapter_.structSize = static_cast<std::uint32_t>(sizeof(TransportAdapterV1));
    adapter_.send_to_server = &HostTransportAdapter::sendToServer;
    adapter_.send_to_player = &HostTransportAdapter::sendToPlayer;
    adapter_.broadcast = &HostTransportAdapter::broadcast;
    adapter_.session_state = &HostTransportAdapter::sessionState;
    adapter_.peer_count = &HostTransportAdapter::peerCount;
}

void HostTransportAdapter::bind(MimitaNet::MultiplayerContext* context) {
    context_ = context;
}

void HostTransportAdapter::unbind() {
    context_ = nullptr;
}

bool HostTransportAdapter::sendToServer(void* owner, const void* data,
                                        std::size_t size) {
    auto* ctx = static_cast<MimitaNet::MultiplayerContext*>(owner);
    if (!ctx || !ctx->transport || !ctx->transport->connected() || !data || size == 0) {
        return false;
    }
    return ctx->transport->send(data, size);
}

bool HostTransportAdapter::sendToPlayer(void*, std::uint32_t, const void*,
                                        std::size_t) {
    // The client context has no server player map. A listen/dedicated server
    // owner registers a server adapter for this operation.
    return false;
}

bool HostTransportAdapter::broadcast(void*, const void*, std::size_t) {
    return false;
}

bool HostTransportAdapter::sessionState(void* owner, SessionStateV1* out) {
    auto* ctx = static_cast<MimitaNet::MultiplayerContext*>(owner);
    if (!ctx || !out || out->structSize < sizeof(SessionStateV1)) {
        return false;
    }
    std::memset(out, 0, sizeof(SessionStateV1));
    out->structSize = static_cast<std::uint32_t>(sizeof(SessionStateV1));
    out->version = kTransportCapabilityVersion;
    out->session = ctx->sessionId.empty()
        ? reinterpret_cast<std::uintptr_t>(ctx)
        : static_cast<std::uint64_t>(std::hash<std::string>{}(ctx->sessionId));
    out->role = 0;
    out->connected = ctx->active ? 1u : 0u;
    out->peerCount = ctx->transport && ctx->transport->connected() ? 1u : 0u;
    out->localPlayerId = ctx->localPlayerId;
    out->tick = ctx->tick;
    out->protocolVersion = MimitaNet::PROTOCOL_VERSION;
    return true;
}

std::uint32_t HostTransportAdapter::peerCount(void* owner) {
    auto* ctx = static_cast<MimitaNet::MultiplayerContext*>(owner);
    return ctx && ctx->transport && ctx->transport->connected() ? 1u : 0u;
}

}  // namespace mimita::host

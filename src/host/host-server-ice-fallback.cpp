// Resident fallbacks for the cold ICE server/session cleanup path.
//
// The game generation owns the strong server helpers. These weak definitions
// keep the resident ICE transport usable without pulling game policy into the
// EXE; game-owned symbols win in the migration EXE.

#include "network/server.h"

#include <atomic>
#include <string>

#include <winsock2.h>

#include "debug/structured-log.h"

namespace MimitaNet {

__attribute__((weak)) TransportConnectionId allocateIceConnectionId() {
    static std::atomic<uint64_t> nextId{1};
    return {TransportKind::Ice, nextId.fetch_add(1)};
}

__attribute__((weak)) sockaddr_in legacyEndpointForTransportConnection(
    TransportConnectionId id) {
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    if (id.kind == TransportKind::Udp) {
        return endpoint;
    }
    const uint32_t low = static_cast<uint32_t>(id.value & 0x00ffffffu);
    endpoint.sin_addr.s_addr = htonl(0x0a000000u | low);
    endpoint.sin_port = htons(static_cast<uint16_t>(1 + (id.value & 0x7fffu)));
    return endpoint;
}

__attribute__((weak)) void serverLogPlayerSessionRemoval(
    const ServerPlayer& player, const char* reason, uint64_t removalTimeMs,
    uint32_t tick, std::size_t playersBefore) {
    const uint64_t silentMs = removalTimeMs >= player.lastHeardMs
        ? removalTimeMs - player.lastHeardMs : 0;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "network.server.player.removed", std::to_string(player.id),
        reason ? reason : "unspecified", tick,
        nlohmann::json{{"player_id", player.id},
                       {"removal_reason", reason ? reason : "unspecified"},
                       {"last_heard_ms", player.lastHeardMs},
                       {"silent_ms", silentMs},
                       {"connection_stale", player.connectionStale},
                       {"disconnect_notice_sent", player.connectionStateNotified},
                       {"reconnect_token_retained", !player.reconnectToken.empty()},
                       {"players_before", playersBefore}},
        __FILE__, __LINE__, __func__);
}

__attribute__((weak)) void serverClearActorPreset(uint32_t actorId) {
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Verbose,
        "network.actor.preset.clear", std::to_string(actorId),
        "cold ICE session removed an actor without a game generation", 0,
        nlohmann::json{{"actor_id", actorId}}, __FILE__, __LINE__, __func__);
}

}  // namespace MimitaNet

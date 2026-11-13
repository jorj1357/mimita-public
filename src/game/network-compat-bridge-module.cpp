// 2026-11-13
// Compatibility bridge for resident MimitaNet ownership calls.
//
// The host owns sockets, WSA lifetime, and server player endpoints. These
// wrappers keep game-side call sites on the host transport rather than opening
// a second network stack.

#include "game/transport-bridge-module.h"
#include "network/packets.h"
#include "network/server.h"

namespace MimitaNet {
namespace {

std::uint32_t packetTypeId(const void* data, std::size_t size) {
    if (!data || size < sizeof(PacketHeader)) {
        return 0;
    }
    return static_cast<std::uint32_t>(
        reinterpret_cast<const PacketHeader*>(data)->type);
}

}  // namespace

bool netStartup() {
    // The resident host owns process network lifetime.
    return true;
}

void netShutdown() {
    // The resident host owns process network lifetime.
}

ServerGameOverrides gServerOverrides;

ServerGameOverrides& serverGameOverrides() {
    return gServerOverrides;
}

bool isServerHost() {
    return mimita::game::sessionIsServerHost();
}

bool serverSendToPlayer(SOCKET, const ServerPlayer& player, const void* data,
                        std::size_t size) {
    return mimita::game::sendToPlayer(player.id, packetTypeId(data, size), data,
                                      size);
}

bool serverSendToEndpoint(SOCKET, const sockaddr_in& to, const void* data,
                          std::size_t size) {
    return mimita::game::sendToEndpoint(
        to.sin_addr.s_addr, to.sin_port, packetTypeId(data, size), data, size);
}

}  // namespace MimitaNet

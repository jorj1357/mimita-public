// 2026-11-12
// Game-DLL CAP_TRANSPORT bridge. The host owns the socket/session; this builds a
// packet and asks the host to send it. No socket, sockaddr, or context pointer
// is stored here.

#include "game/transport-bridge-module.h"

#include <cstring>

#include "host/abi/transport-capability.h"
#include "network/packets.h"

namespace mimita::game {
namespace {

const mimita::host::TransportCapabilityV1* gTransport = nullptr;
void* gHostData = nullptr;

}  // namespace

void initializeHostTransport(const mimita::host::HostServicesV1* services) {
    gTransport = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability = static_cast<const mimita::host::TransportCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Transport),
            mimita::host::kTransportCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kTransportCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::TransportCapabilityV1)) {
        gTransport = capability;
    }
}

bool sendServerCommand(const char* command, std::size_t size) {
    if (!gTransport || !gTransport->send || !gTransport->session_state || !command) {
        return false;
    }
    mimita::host::SessionStateV1 state{};
    state.structSize = static_cast<std::uint32_t>(sizeof(state));
    state.version = mimita::host::kTransportCapabilityVersion;
    if (gTransport->session_state(gHostData, &state) !=
            mimita::host::TransportResult::Ok ||
        !state.connected || state.localPlayerId == 0) {
        return false;
    }

    MimitaNet::ServerCommandPacket packet{};
    packet.header.type = MimitaNet::PACKET_SERVER_COMMAND;
    packet.header.tick = state.tick;
    packet.header.playerId = state.localPlayerId;
    std::memset(packet.commandText, 0, sizeof(packet.commandText));
    const std::size_t length = size < sizeof(packet.commandText) - 1
        ? size : sizeof(packet.commandText) - 1;
    std::memcpy(packet.commandText, command, length);
    packet.commandText[length] = '\0';

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&packet);
    return gTransport->send(gHostData, 0, mimita::host::TransportChannel::Control,
                            static_cast<std::uint32_t>(MimitaNet::PACKET_SERVER_COMMAND),
                            0, bytes, static_cast<std::uint32_t>(sizeof(packet))) ==
           mimita::host::TransportResult::Ok;
}

bool sendServerCommandResult(std::uint32_t playerId, std::int32_t accepted,
                             const char* status) {
    if (!gTransport || !gTransport->send_to_player) {
        return false;
    }
    MimitaNet::ServerCommandResultPacket result{};
    result.header.type = MimitaNet::PACKET_SERVER_COMMAND_RESULT;
    result.header.tick = 0;
    result.header.playerId = playerId;
    result.accepted = accepted;
    std::memset(result.statusText, 0, sizeof(result.statusText));
    if (status) {
        std::strncpy(result.statusText, status, sizeof(result.statusText) - 1);
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&result);
    return gTransport->send_to_player(
               gHostData, playerId,
               static_cast<std::uint32_t>(MimitaNet::PACKET_SERVER_COMMAND_RESULT),
               0, bytes, static_cast<std::uint32_t>(sizeof(result))) ==
           mimita::host::TransportResult::Ok;
}

}  // namespace mimita::game

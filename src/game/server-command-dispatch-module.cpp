// 2026-11-13
// Shared command policy for the probe and whole-game generations.

#include "game/server-command-dispatch.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "game/entity-bridge-module.h"
#include "game/transport-bridge-module.h"
#include "network/packets.h"

namespace mimita::game {

bool dispatchServerCommand(mimita::host::TransportDispatchV1* dispatch,
                           const mimita::host::HostServicesV1* services) {
    if (!dispatch) {
        return false;
    }
    const mimita::host::TransportMessageV1& message = dispatch->message;
    if (!message.payload || message.payloadSize < sizeof(MimitaNet::PacketHeader)) {
        return false;
    }
    const auto* header =
        reinterpret_cast<const MimitaNet::PacketHeader*>(message.payload);
    if (header->type != MimitaNet::PACKET_SERVER_COMMAND ||
        message.payloadSize < sizeof(MimitaNet::ServerCommandPacket)) {
        return false;
    }

    const auto* packet =
        reinterpret_cast<const MimitaNet::ServerCommandPacket*>(message.payload);
    char command[128] = {};
    std::strncpy(command, packet->commandText, sizeof(command) - 1);

    mimita::host::EntitySnapshotV1 player{};
    player.structSize = static_cast<std::uint32_t>(sizeof(player));
    const auto handle = mimita::host::makeEntityHandle(
        mimita::host::EntityKind::Player, header->playerId);
    const auto read = readHostEntity(handle, &player);
    const bool hasPlayer = read == mimita::host::HostResult::Ok;
    const bool isHost =
        hasPlayer &&
        (player.playerFlags &
         static_cast<std::uint32_t>(mimita::host::PlayerFlag::Host)) != 0;
    const bool enabled =
        hasPlayer &&
        ((player.playerFlags &
          static_cast<std::uint32_t>(mimita::host::PlayerFlag::AdminEnabled)) != 0);
    const bool granted =
        hasPlayer &&
        ((player.playerFlags &
          static_cast<std::uint32_t>(mimita::host::PlayerFlag::AdminGranted)) != 0);
    const bool admin = hasPlayer && (isHost ? enabled : (granted && enabled));

    bool gameAccepted = false;
    bool authoritative = false;
    char gameStatus[128] = "probe: policy not migrated";
    const std::string textCommand(command);
    if (textCommand == "admincmds") {
        gameAccepted = isHost || granted;
        authoritative = hasPlayer;
        if (isHost) {
            std::snprintf(gameStatus, sizeof(gameStatus),
                          enabled ? "admincmds=1 (host)" : "admincmds=0 (host)");
        } else if (hasPlayer) {
            std::snprintf(gameStatus, sizeof(gameStatus),
                          enabled ? "admincmds=1 (granted)"
                                  : "admincmds=0 (granted)");
        } else {
            std::snprintf(gameStatus, sizeof(gameStatus),
                          "rejected: no admin grant for this session");
        }
    } else if (textCommand.rfind("admincmds ", 0) == 0) {
        gameAccepted = admin;
        std::snprintf(gameStatus, sizeof(gameStatus), "%s",
                      admin ? "probe: admincmds argument allowed"
                            : "probe: no admin grant");
    } else if (textCommand == "player_list") {
        gameAccepted = admin;
        std::snprintf(gameStatus, sizeof(gameStatus), "%s",
                      admin ? "probe: player_list allowed"
                            : "probe: no admin access");
    }

    if (services && services->log) {
        char text[512] = {};
        std::snprintf(text, sizeof(text),
                      "game.transport.dispatch type=%u bytes=%u player=%u "
                      "command=%s accepted=%d authoritative=%d status=%s",
                      header->type, message.payloadSize, header->playerId, command,
                      gameAccepted ? 1 : 0, authoritative ? 1 : 0, gameStatus);
        services->log(services->hostUserData, 1, text);
    }

    if (!authoritative) {
        return false;
    }

    MimitaNet::ServerCommandResultPacket result{};
    result.header.type = MimitaNet::PACKET_SERVER_COMMAND_RESULT;
    result.header.tick = dispatch->serverTick;
    result.header.playerId = header->playerId;
    result.accepted = gameAccepted ? 1 : 0;
    std::memset(result.statusText, 0, sizeof(result.statusText));
    std::strncpy(result.statusText, gameStatus, sizeof(result.statusText) - 1);
    std::memcpy(dispatch->response.payload, &result, sizeof(result));
    dispatch->response.typeId = MimitaNet::PACKET_SERVER_COMMAND_RESULT;
    dispatch->response.flags = 0;
    dispatch->response.payloadSize = static_cast<std::uint32_t>(sizeof(result));
    return true;
}

bool dispatchPingMessage(const mimita::host::TransportMessageV1* message,
                         const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::PingPacket)) {
        return false;
    }
    const auto* header =
        reinterpret_cast<const MimitaNet::PacketHeader*>(message->payload);
    if (header->type != MimitaNet::PACKET_PING ||
        header->magic != MimitaNet::PROTOCOL_MAGIC ||
        header->version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }

    MimitaNet::PingPacket pong =
        *reinterpret_cast<const MimitaNet::PingPacket*>(message->payload);
    pong.header.tick = message->serverTick;
    const bool sent = sendToPeer(message->peer, MimitaNet::PACKET_PING, &pong,
                                 sizeof(pong));
    if (services && services->log) {
        static std::uint64_t lastPingLogMs = 0;
        if (message->receivedAtMs - lastPingLogMs >= 5000) {
            lastPingLogMs = message->receivedAtMs;
            char text[192] = {};
            std::snprintf(text, sizeof(text),
                          "game.transport.ping peer=%llu player=%u tick=%u sent=%d",
                          (unsigned long long)message->peer, header->playerId,
                          message->serverTick, sent ? 1 : 0);
            services->log(services->hostUserData, 1, text);
        }
    }
    return sent;
}

}  // namespace mimita::game

// 2026-11-13
// Shared command policy for the probe and whole-game generations.

#include "game/server-command-dispatch.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "config/networking-config.h"
#include "game/entity-bridge-module.h"
#include "game/world-bridge-module.h"
#include "host/abi/world-capability.h"
#include "game/session-events-bridge-module.h"
#include "game/session-identity-bridge-module.h"
#include "game/transport-bridge-module.h"
#include "network/community-server-config.h"
#include "network/coordinator-client.h"
#include "network/packets.h"
#include "network/server.h"
#include "physics/movement/movement-conversion.h"
#include "website/api-client.h"

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

namespace {

constexpr std::uint32_t kMaxInboundPlayers = 256;

bool readPlayer(std::uint32_t playerId,
                mimita::host::EntitySnapshotV1* out) {
    if (!out) {
        return false;
    }
    out->structSize = static_cast<std::uint32_t>(sizeof(*out));
    return readHostEntity(
               mimita::host::makeEntityHandle(mimita::host::EntityKind::Player,
                                              playerId),
               out) == mimita::host::HostResult::Ok;
}

bool dispatchChatMessage(const mimita::host::TransportMessageV1* message,
                         const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::ChatPacket)) {
        return false;
    }
    auto* chat = reinterpret_cast<MimitaNet::ChatPacket*>(
        const_cast<std::uint8_t*>(message->payload));
    if (chat->header.type != MimitaNet::PACKET_CHAT_MESSAGE ||
        chat->header.magic != MimitaNet::PROTOCOL_MAGIC ||
        chat->header.version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }
    mimita::host::EntitySnapshotV1 sender{};
    if (!readPlayer(chat->header.playerId, &sender)) {
        return false;
    }
    chat->header.tick = message->serverTick;
    if (services && services->log) {
        char text[256] = {};
        std::snprintf(text, sizeof(text), "chat.legacy player=%u name=%s text=%s",
                      chat->header.playerId, sender.displayName, chat->text);
        services->log(services->hostUserData, 2, text);
    }

    mimita::host::EntityHandle players[kMaxInboundPlayers] = {};
    const std::uint32_t count = listHostEntities(
        mimita::host::EntityKind::Player, players, kMaxInboundPlayers);
    const std::uint32_t limit =
        count < kMaxInboundPlayers ? count : kMaxInboundPlayers;
    for (std::uint32_t i = 0; i < limit; ++i) {
        const std::uint32_t playerId =
            mimita::host::entityId(players[i]);
        if (playerId == chat->header.playerId) {
            continue;
        }
        sendToPlayer(playerId, MimitaNet::PACKET_CHAT_MESSAGE, chat,
                     sizeof(*chat));
    }
    return true;
}

bool dispatchChatTyping(const mimita::host::TransportMessageV1* message,
                        const mimita::host::HostServicesV1*) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::ChatTypingStateRequestPacket)) {
        return false;
    }
    const auto* request =
        reinterpret_cast<const MimitaNet::ChatTypingStateRequestPacket*>(
            message->payload);
    if (request->header.type != MimitaNet::PACKET_CHAT_TYPING_STATE_REQUEST ||
        request->header.magic != MimitaNet::PROTOCOL_MAGIC ||
        request->header.version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }
    mimita::host::EntitySnapshotV1 sender{};
    if (!readPlayer(request->header.playerId, &sender)) {
        return false;
    }

    MimitaNet::ChatTypingStateEventPacket event{};
    event.header.type = MimitaNet::PACKET_CHAT_TYPING_STATE_EVENT;
    event.header.tick = message->serverTick;
    event.header.playerId = request->header.playerId;
    event.playerId = request->header.playerId;
    event.isTyping = request->isTyping;
    event.serverTick = message->serverTick;

    mimita::host::EntityHandle players[kMaxInboundPlayers] = {};
    const std::uint32_t count = listHostEntities(
        mimita::host::EntityKind::Player, players, kMaxInboundPlayers);
    const std::uint32_t limit =
        count < kMaxInboundPlayers ? count : kMaxInboundPlayers;
    for (std::uint32_t i = 0; i < limit; ++i) {
        const std::uint32_t playerId = mimita::host::entityId(players[i]);
        if (playerId == request->header.playerId) {
            continue;
        }
        sendToPlayer(playerId, MimitaNet::PACKET_CHAT_TYPING_STATE_EVENT,
                     &event, sizeof(event));
    }
    return true;
}

bool dispatchSpawnNpc(const mimita::host::TransportMessageV1* message,
                      const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::SpawnNpcRequestPacket)) {
        return false;
    }
    const auto* request =
        reinterpret_cast<const MimitaNet::SpawnNpcRequestPacket*>(
            message->payload);
    if (request->header.type != MimitaNet::PACKET_SPAWN_NPC_REQUEST ||
        request->header.magic != MimitaNet::PROTOCOL_MAGIC ||
        request->header.version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }
    mimita::host::EntitySnapshotV1 sender{};
    if (!readPlayer(request->header.playerId, &sender)) {
        return false;
    }

    if (MimitaNet::getServerMatchMode() == "ssbm") {
        mimita::host::EntityHandle npcs[kMaxInboundPlayers] = {};
        const std::uint32_t count = listHostEntities(
            mimita::host::EntityKind::Npc, npcs, kMaxInboundPlayers);
        const std::uint32_t limit =
            count < kMaxInboundPlayers ? count : kMaxInboundPlayers;
        int live = 0;
        for (std::uint32_t i = 0; i < limit; ++i) {
            mimita::host::EntitySnapshotV1 npc{};
            npc.structSize = static_cast<std::uint32_t>(sizeof(npc));
            if (readHostEntity(npcs[i], &npc) ==
                    mimita::host::HostResult::Ok &&
                npc.health > 0.0f) {
                ++live;
            }
        }
        if (live >= 1) {
            if (services && services->log) {
                char text[160] = {};
                std::snprintf(text, sizeof(text),
                              "spawn-npc.rejected reason=melee-one-npc live=%d",
                              live);
                services->log(services->hostUserData, 1, text);
            }
            return true;
        }
    }

    mimita::host::EntityHandle handle = mimita::host::kInvalidEntity;
    if (createHostEntity(mimita::host::EntityKind::Npc, &handle) !=
        mimita::host::HostResult::Ok) {
        return false;
    }
    mimita::host::EntitySnapshotV1 npc{};
    npc.structSize = static_cast<std::uint32_t>(sizeof(npc));
    if (readHostEntity(handle, &npc) != mimita::host::HostResult::Ok) {
        return false;
    }
    npc.position[0] = request->px;
    npc.position[1] = request->py;
    npc.position[2] = request->pz;
    npc.difficulty = request->difficulty;
    const std::uint32_t npcId = mimita::host::entityId(handle);
    std::snprintf(npc.displayName, sizeof(npc.displayName), "NPC %u", npcId);
    if (writeHostEntity(handle, &npc) != mimita::host::HostResult::Ok) {
        return false;
    }
    if (services && services->log) {
        char text[192] = {};
        std::snprintf(text, sizeof(text),
                      "spawn-npc.created id=%u pos=(%.2f,%.2f,%.2f) difficulty=%.2f",
                      npcId, request->px, request->py, request->pz,
                      request->difficulty);
        services->log(services->hostUserData, 1, text);
    }
    return true;
}

bool dispatchTeleport(const mimita::host::TransportMessageV1* message,
                      const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::TeleportRequestPacket)) {
        return false;
    }
    const auto* request =
        reinterpret_cast<const MimitaNet::TeleportRequestPacket*>(
            message->payload);
    if (request->header.type != MimitaNet::PACKET_TELEPORT_REQUEST ||
        request->header.magic != MimitaNet::PROTOCOL_MAGIC ||
        request->header.version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }
    mimita::host::EntitySnapshotV1 player{};
    if (!readPlayer(request->header.playerId, &player)) {
        return false;
    }
    if ((player.playerFlags &
         static_cast<std::uint32_t>(mimita::host::PlayerFlag::Dead)) != 0) {
        return true;
    }
    const float position[3] = {request->px, request->py, request->pz};
    constexpr float kSanityLimit = 1.0e9f;
    const bool finite = std::isfinite(position[0]) &&
                        std::isfinite(position[1]) &&
                        std::isfinite(position[2]);
    const bool within =
        std::abs(position[0]) <= kSanityLimit &&
        std::abs(position[1]) <= kSanityLimit &&
        std::abs(position[2]) <= kSanityLimit;
    if (!finite || !within) {
        if (services && services->log) {
            char text[192] = {};
            std::snprintf(text, sizeof(text),
                          "teleport.rejected player=%u reason=%s",
                          request->header.playerId,
                          finite ? "sanity-limit" : "non-finite");
            services->log(services->hostUserData, 1, text);
        }
        return true;
    }
    const MovementConfig config = makeCurrentRuntimeMovementConfig();
    const float velocity[3] = {0.0f, 0.0f, 0.0f};
    const auto handle = mimita::host::makeEntityHandle(
        mimita::host::EntityKind::Player, request->header.playerId);
    const mimita::host::HostResult result = resetHostTransform(
        handle, position, velocity, player.yaw, 0, 0,
        static_cast<std::uint32_t>(config.maximumAirJumps), 1);
    if (result != mimita::host::HostResult::Ok) {
        return false;
    }
    if (services && services->log) {
        char text[224] = {};
        std::snprintf(text, sizeof(text),
                      "teleport.applied player=%u pos=(%.2f,%.2f,%.2f)",
                      request->header.playerId, position[0], position[1],
                      position[2]);
        services->log(services->hostUserData, 1, text);
    }
    return true;
}

bool dispatchDisconnect(const mimita::host::TransportMessageV1* message,
                        const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::PacketHeader)) {
        return false;
    }
    const auto* header =
        reinterpret_cast<const MimitaNet::PacketHeader*>(message->payload);
    if (header->type != MimitaNet::PACKET_DISCONNECT ||
        header->magic != MimitaNet::PROTOCOL_MAGIC ||
        header->version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }
    mimita::host::EntitySnapshotV1 sender{};
    if (!readPlayer(header->playerId, &sender)) {
        return false;
    }
    const auto handle = mimita::host::makeEntityHandle(
        mimita::host::EntityKind::Player, header->playerId);
    const mimita::host::HostResult removed = removeHostEntity(handle);
    if (removed != mimita::host::HostResult::Ok &&
        removed != mimita::host::HostResult::NotFound) {
        return false;
    }

    MimitaNet::ChatMessageEventPacket event{};
    event.header.type = MimitaNet::PACKET_CHAT_MESSAGE_EVENT;
    event.header.tick = message->serverTick;
    event.header.playerId = 0;
    event.requestId = 0;
    const std::uint32_t eventId = nextSessionEventId();
    event.eventId = eventId;
    event.eventSessionId = 0;
    event.messageId = eventId;
    event.serverTick = message->serverTick;
    event.utcUnixMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    event.senderType = 1;
    std::strncpy(event.senderName, "server", sizeof(event.senderName) - 1);
    std::snprintf(event.utf8Message, sizeof(event.utf8Message),
                  "%s left the game", sender.displayName);
    const auto& reliable =
        NetworkingConfig::instance().data().reliableEvents;
    const std::uint32_t maxPending = static_cast<std::uint32_t>(
        reliable.maxPendingPerPlayer);
    if (eventId != 0 && maxPending > 0) {
        queueSessionEventToAll(&event, sizeof(event), eventId, maxPending);
    }
    if (services && services->log) {
        char text[192] = {};
        std::snprintf(text, sizeof(text), "disconnect player=%u name=%s",
                      header->playerId, sender.displayName);
        services->log(services->hostUserData, 1, text);
    }
    return true;
}

bool dispatchReconnect(const mimita::host::TransportMessageV1* message,
                       const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::ReconnectRequestPacket)) {
        return false;
    }
    const auto* request =
        reinterpret_cast<const MimitaNet::ReconnectRequestPacket*>(
            message->payload);
    if (request->header.type != MimitaNet::PACKET_RECONNECT_REQUEST ||
        request->header.magic != MimitaNet::PROTOCOL_MAGIC ||
        request->header.version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }
    std::string token(request->reconnectToken,
                      strnlen(request->reconnectToken,
                              sizeof(request->reconnectToken)));
    if (token.empty()) {
        return false;
    }
    mimita::host::EntityHandle handle = mimita::host::kInvalidEntity;
    if (findHostReconnectToken(token.c_str(), 1, &handle) !=
        mimita::host::HostResult::Ok) {
        return false;
    }
    const std::uint32_t playerId = mimita::host::entityId(handle);
    mimita::host::EntitySnapshotV1 player{};
    player.structSize = static_cast<std::uint32_t>(sizeof(player));
    if (readHostEntity(handle, &player) != mimita::host::HostResult::Ok) {
        return false;
    }
    const bool resendExistingAccept = player.reconnectToken != token;

    if (!bindSession(message->peer, playerId)) {
        return false;
    }
    if (!resendExistingAccept) {
        resetSessionEventPlayer(playerId);
        const std::uint64_t validUntil = MimitaNet::nowMs() +
            static_cast<std::uint64_t>(
                NetworkingConfig::instance().data().retries.reconnectGraceMs);
        const std::string nextToken = MimitaNet::generateReconnectToken();
        player.reconnectToken[0] = '\0';
        std::snprintf(player.reconnectToken, sizeof(player.reconnectToken),
                      "%s", nextToken.c_str());
        std::snprintf(player.previousReconnectToken,
                      sizeof(player.previousReconnectToken), "%s",
                      token.c_str());
        player.previousReconnectTokenValidUntilMs = validUntil;
        player.reliableEventSessionId = 0;
        player.lastHeardMs = MimitaNet::nowMs();
        if (writeHostEntity(handle, &player) !=
            mimita::host::HostResult::Ok) {
            return false;
        }
        const float position[3] = {player.position[0], player.position[1],
                                   player.position[2]};
        const float velocity[3] = {player.velocity[0], player.velocity[1],
                                   player.velocity[2]};
        if (resetHostTransform(handle, position, velocity, player.yaw,
                               player.onGround, 0, 0,
                               0) != mimita::host::HostResult::Ok) {
            return false;
        }
        player.structSize = static_cast<std::uint32_t>(sizeof(player));
        if (readHostEntity(handle, &player) != mimita::host::HostResult::Ok) {
            return false;
        }
    }

    const std::uint32_t sessionId = sessionEventSessionId(playerId);
    MimitaNet::ReconnectAcceptPacket accept{};
    accept.header.type = MimitaNet::PACKET_RECONNECT_ACCEPT;
    accept.header.tick = message->serverTick;
    accept.header.playerId = playerId;
    accept.header.transformEpoch = player.transformEpoch;
    accept.assignedPlayerId = playerId;
    accept.reliableEventSessionId = sessionId;
    accept.tickRate = MimitaNet::SERVER_TICK_RATE;
    accept.spawnGeneration = player.spawnGeneration;
    std::snprintf(accept.approvedName, sizeof(accept.approvedName), "%s",
                  player.displayName);
    std::snprintf(accept.reconnectToken, sizeof(accept.reconnectToken), "%s",
                  player.reconnectToken);
    std::snprintf(accept.mapId, sizeof(accept.mapId), "%s",
                  MimitaNet::getServerMapId().c_str());
    accept.restoredHealth = static_cast<std::int32_t>(player.health);
    accept.restoredKills = player.kills;
    accept.restoredDeaths = player.deaths;
    accept.restorePx = player.position[0];
    accept.restorePy = player.position[1];
    accept.restorePz = player.position[2];
    if (const char* runId = std::getenv("MIMITA_RUN_ID")) {
        std::snprintf(accept.runtimeRunId, sizeof(accept.runtimeRunId), "%s",
                      runId);
    }
    if (const char* buildId = std::getenv("MIMITA_BUILD_ID")) {
        std::snprintf(accept.runtimeBuildId, sizeof(accept.runtimeBuildId),
                      "%s", buildId);
    }
    const int copies = resendExistingAccept ? 1 : 4;
    for (int index = 0; index < copies; ++index) {
        sendToPeer(message->peer, MimitaNet::PACKET_RECONNECT_ACCEPT, &accept,
                   sizeof(accept));
    }
    if (services && services->log) {
        char text[224] = {};
        std::snprintf(text, sizeof(text),
                      "reconnect player=%u token=%s resend=%d session=%u",
                      playerId, token.substr(0, 12).c_str(),
                      resendExistingAccept ? 1 : 0, sessionId);
        services->log(services->hostUserData, 1, text);
    }
    return true;
}

std::string bounded_name(const char* text, std::size_t capacity,
                         std::uint32_t playerId) {
    std::string value(text ? text : "",
                      text ? strnlen(text, capacity) : 0);
    if (value.empty()) {
        value = "player" + std::to_string(playerId);
    }
    return value;
}

std::string unique_name(const std::string& requested, std::uint32_t ownId) {
    mimita::host::EntityHandle players[kMaxInboundPlayers] = {};
    const std::uint32_t count = listHostEntities(
        mimita::host::EntityKind::Player, players, kMaxInboundPlayers);
    const std::uint32_t limit =
        count < kMaxInboundPlayers ? count : kMaxInboundPlayers;
    std::string base = requested.empty()
        ? "player" + std::to_string(ownId)
        : requested;
    std::string candidate = base;
    int suffix = 2;
    for (;;) {
        bool used = false;
        for (std::uint32_t i = 0; i < limit; ++i) {
            const std::uint32_t otherId = mimita::host::entityId(players[i]);
            if (otherId == ownId) {
                continue;
            }
            mimita::host::EntitySnapshotV1 other{};
            other.structSize = static_cast<std::uint32_t>(sizeof(other));
            if (readHostEntity(players[i], &other) ==
                    mimita::host::HostResult::Ok &&
                candidate == other.displayName) {
                used = true;
                break;
            }
        }
        if (!used) {
            return candidate;
        }
        candidate = base + "(" + std::to_string(suffix++) + ")";
    }
}

bool dispatchHello(const mimita::host::TransportMessageV1* message,
                   const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::HelloPacket)) {
        return false;
    }
    const auto* hello =
        reinterpret_cast<const MimitaNet::HelloPacket*>(message->payload);
    if (hello->header.type != MimitaNet::PACKET_HELLO ||
        hello->header.magic != MimitaNet::PROTOCOL_MAGIC ||
        hello->header.version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }

    mimita::host::EntityHandle handle = mimita::host::kInvalidEntity;
    bool existing = findHostConnection(message->peer, &handle) ==
                    mimita::host::HostResult::Ok;
    if (!existing) {
        if (createHostEntity(mimita::host::EntityKind::Player, &handle) !=
            mimita::host::HostResult::Ok) {
            return false;
        }
    }
    const std::uint32_t playerId = mimita::host::entityId(handle);
    if (!bindSession(message->peer, playerId)) {
        return false;
    }

    mimita::host::EntitySnapshotV1 player{};
    player.structSize = static_cast<std::uint32_t>(sizeof(player));
    if (readHostEntity(handle, &player) != mimita::host::HostResult::Ok) {
        return false;
    }

    const std::string name = unique_name(
        bounded_name(hello->name, sizeof(hello->name), playerId), playerId);
    std::snprintf(player.displayName, sizeof(player.displayName), "%s",
                  name.c_str());
    std::string avatar(hello->avatarName,
                       strnlen(hello->avatarName, sizeof(hello->avatarName)));
    std::snprintf(player.avatarName, sizeof(player.avatarName), "%s",
                  avatar.c_str());

    const std::uint32_t hostId = MimitaNet::serverHostPlayerId();
    if (hostId == 0) {
        MimitaNet::setServerHostPlayerId(playerId);
    }
    const std::uint32_t hostMask =
        static_cast<std::uint32_t>(mimita::host::PlayerFlag::Host) |
        static_cast<std::uint32_t>(mimita::host::PlayerFlag::AdminEnabled);
    player.playerFlags &= ~hostMask;
    if (MimitaNet::serverHostPlayerId() == playerId) {
        player.playerFlags |= hostMask;
    }
    const std::string reconnectToken = MimitaNet::generateReconnectToken();
    std::snprintf(player.reconnectToken, sizeof(player.reconnectToken), "%s",
                  reconnectToken.c_str());
    if (!existing) {
        player.reliableEventSessionId = 0;
    }
    player.lastHeardMs = MimitaNet::nowMs();
    if (writeHostEntity(handle, &player) != mimita::host::HostResult::Ok) {
        return false;
    }

    if (!existing) {
        mimita::host::WorldSpawnV1 spawn{};
        spawn.structSize = static_cast<std::uint32_t>(sizeof(spawn));
        if (pickHostSpawn(playerId, &spawn) ==
                mimita::host::HostResult::Ok &&
            spawn.found != 0) {
            const float position[3] = {spawn.position[0], spawn.position[1],
                                       spawn.position[2]};
            const float velocity[3] = {0.0f, 0.0f, 0.0f};
            const MovementConfig config = makeCurrentRuntimeMovementConfig();
            resetHostTransform(handle, position, velocity, spawn.yaw, 0, 0,
                               static_cast<std::uint32_t>(config.maximumAirJumps),
                               1);
        }
    }

    player.structSize = static_cast<std::uint32_t>(sizeof(player));
    if (readHostEntity(handle, &player) != mimita::host::HostResult::Ok) {
        return false;
    }
    const std::uint32_t sessionId = sessionEventSessionId(playerId);

    MimitaNet::WelcomePacket welcome{};
    welcome.header.type = MimitaNet::PACKET_WELCOME;
    welcome.header.tick = message->serverTick;
    welcome.header.playerId = playerId;
    welcome.header.transformEpoch = player.transformEpoch;
    welcome.assignedPlayerId = playerId;
    welcome.reliableEventSessionId = sessionId;
    welcome.tickRate = MimitaNet::SERVER_TICK_RATE;
    std::snprintf(welcome.approvedName, sizeof(welcome.approvedName), "%s",
                  player.displayName);
    std::snprintf(welcome.reconnectToken, sizeof(welcome.reconnectToken), "%s",
                  player.reconnectToken);
    std::snprintf(welcome.mapId, sizeof(welcome.mapId), "%s",
                  MimitaNet::getServerMapId().c_str());
    if (const char* runId = std::getenv("MIMITA_RUN_ID")) {
        std::snprintf(welcome.runtimeRunId, sizeof(welcome.runtimeRunId), "%s",
                      runId);
    }
    if (const char* buildId = std::getenv("MIMITA_BUILD_ID")) {
        std::snprintf(welcome.runtimeBuildId, sizeof(welcome.runtimeBuildId),
                      "%s", buildId);
    }
    sendToPeer(message->peer, MimitaNet::PACKET_WELCOME, &welcome,
               sizeof(welcome));
    sendHostAvatarManifests(handle);

    if (!existing) {
        MimitaNet::ChatMessageEventPacket event{};
        event.header.type = MimitaNet::PACKET_CHAT_MESSAGE_EVENT;
        event.header.tick = message->serverTick;
        event.header.playerId = 0;
        const std::uint32_t eventId = nextSessionEventId();
        event.eventId = eventId;
        event.eventSessionId = 0;
        event.messageId = eventId;
        event.serverTick = message->serverTick;
        event.utcUnixMilliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        event.senderType = 1;
        std::strncpy(event.senderName, "server",
                     sizeof(event.senderName) - 1);
        std::snprintf(event.utf8Message, sizeof(event.utf8Message),
                      "%s joined the game", player.displayName);
        const auto& reliable =
            NetworkingConfig::instance().data().reliableEvents;
        const std::uint32_t maxPending = static_cast<std::uint32_t>(
            reliable.maxPendingPerPlayer);
        if (eventId != 0 && maxPending > 0) {
            queueSessionEventToAll(&event, sizeof(event), eventId, maxPending);
        }
    }
    if (services && services->log) {
        char text[192] = {};
        std::snprintf(text, sizeof(text), "hello player=%u name=%s existing=%d",
                      playerId, player.displayName, existing ? 1 : 0);
        services->log(services->hostUserData, 1, text);
    }
    return true;
}

void send_join_reject(const mimita::host::TransportMessageV1* message,
                      std::uint8_t reason) {
    MimitaNet::JoinRejectPacket reject{};
    reject.header.type = MimitaNet::PACKET_JOIN_REJECT;
    reject.header.tick = message->serverTick;
    reject.reason = reason;
    sendToPeer(message->peer, MimitaNet::PACKET_JOIN_REJECT, &reject,
               sizeof(reject));
}

void queue_server_chat(std::uint32_t serverTick, const std::string& text) {
    MimitaNet::ChatMessageEventPacket event{};
    event.header.type = MimitaNet::PACKET_CHAT_MESSAGE_EVENT;
    event.header.tick = serverTick;
    const std::uint32_t eventId = nextSessionEventId();
    event.eventId = eventId;
    event.eventSessionId = 0;
    event.messageId = eventId;
    event.serverTick = serverTick;
    event.utcUnixMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    event.senderType = 1;
    std::strncpy(event.senderName, "server", sizeof(event.senderName) - 1);
    std::snprintf(event.utf8Message, sizeof(event.utf8Message), "%s",
                  text.c_str());
    const auto& reliable = NetworkingConfig::instance().data().reliableEvents;
    const std::uint32_t maxPending =
        static_cast<std::uint32_t>(reliable.maxPendingPerPlayer);
    if (eventId != 0 && maxPending > 0) {
        queueSessionEventToAll(&event, sizeof(event), eventId, maxPending);
    }
}

void broadcast_vip_styles(std::uint32_t serverTick) {
    mimita::host::EntityHandle players[kMaxInboundPlayers] = {};
    const std::uint32_t count = listHostEntities(
        mimita::host::EntityKind::Player, players, kMaxInboundPlayers);
    const std::uint32_t limit =
        count < kMaxInboundPlayers ? count : kMaxInboundPlayers;
    for (std::uint32_t targetIndex = 0; targetIndex < limit; ++targetIndex) {
        const std::uint32_t targetId =
            mimita::host::entityId(players[targetIndex]);
        mimita::host::EntitySnapshotV1 target{};
        target.structSize = static_cast<std::uint32_t>(sizeof(target));
        if (readHostEntity(players[targetIndex], &target) !=
                mimita::host::HostResult::Ok ||
            target.vipStyleEpoch == 0) {
            continue;
        }
        MimitaNet::VipStyleEventPacket packet{};
        packet.header.type = MimitaNet::PACKET_VIP_STYLE_EVENT;
        packet.header.tick = serverTick;
        packet.header.playerId = targetId;
        packet.playerId = targetId;
        packet.styleEpoch = target.vipStyleEpoch;
        packet.styleKind = target.vipDetailStyleKind;
        packet.animation = target.vipDetailAnimation;
        packet.direction = target.vipDetailDirection;
        packet.rainbowSpeed = target.vipRainbowSpeed;
        const std::uint32_t colorCount =
            std::min<std::uint32_t>(target.vipColorCount, 32);
        packet.colorCount = static_cast<std::uint8_t>(colorCount);
        for (std::uint32_t index = 0; index < colorCount; ++index) {
            packet.colors[index].r = static_cast<std::uint8_t>(
                std::clamp(target.vipColors[index][0], 0.0f, 1.0f) * 255.0f);
            packet.colors[index].g = static_cast<std::uint8_t>(
                std::clamp(target.vipColors[index][1], 0.0f, 1.0f) * 255.0f);
            packet.colors[index].b = static_cast<std::uint8_t>(
                std::clamp(target.vipColors[index][2], 0.0f, 1.0f) * 255.0f);
        }
        for (std::uint32_t recipientIndex = 0; recipientIndex < limit;
             ++recipientIndex) {
            sendToPlayer(mimita::host::entityId(players[recipientIndex]),
                         MimitaNet::PACKET_VIP_STYLE_EVENT, &packet,
                         sizeof(packet));
        }
    }
}

bool dispatchJoin(const mimita::host::TransportMessageV1* message,
                  const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::JoinRequestPacket)) {
        return false;
    }
    const auto* join =
        reinterpret_cast<const MimitaNet::JoinRequestPacket*>(message->payload);
    if (join->header.type != MimitaNet::PACKET_JOIN_REQUEST ||
        join->header.magic != MimitaNet::PROTOCOL_MAGIC ||
        join->header.version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }
    const std::string joinToken(
        join->joinToken, strnlen(join->joinToken, sizeof(join->joinToken)));
    if (joinToken.empty()) {
        send_join_reject(message, 2);
        return true;
    }
    const std::string coordinatorCode = MimitaNet::getServerCoordinatorCode();
    if (coordinatorCode.empty()) {
        send_join_reject(message, 2);
        return true;
    }
    if (coordinatorCode != "LOCAL" &&
        !MimitaNet::coordinatorIceValidateJoin(coordinatorCode, joinToken)) {
        send_join_reject(message, 2);
        return true;
    }
    const std::string vipTicket(join->vipJoinTicket,
                                strnlen(join->vipJoinTicket,
                                        sizeof(join->vipJoinTicket)));
    VipJoinTicketResult vipResult;
    bool hasVipTicket = false;
    if (!vipTicket.empty()) {
        vipResult = verifyVipJoinTicket(vipTicket, coordinatorCode, "", 2000);
        hasVipTicket = vipResult.ok && vipResult.verified;
    }
    if (getSessionFlag(
            mimita::host::SessionFlagField::PasswordProtected) != 0) {
        const std::string supplied(
            join->password, strnlen(join->password, sizeof(join->password)));
        if (supplied.empty() ||
            !sessionPasswordMatches(supplied.c_str())) {
            send_join_reject(message, 5);
            return true;
        }
    }

    mimita::host::EntityHandle handle = mimita::host::kInvalidEntity;
    bool existing = findHostJoinToken(joinToken.c_str(), &handle) ==
                    mimita::host::HostResult::Ok;
    if (!existing) {
        existing = findHostReconnectToken(joinToken.c_str(), 0, &handle) ==
                   mimita::host::HostResult::Ok;
    }

    const MimitaNet::CommunityMode* mode =
        MimitaNet::CommunityServerConfig::instance().modeById(
            MimitaNet::getServerCommunityMode());
    const std::size_t limit = mode && mode->maxPlayers > 0
        ? std::min<std::size_t>(MimitaNet::MAX_PLAYERS,
                                static_cast<std::size_t>(mode->maxPlayers))
        : static_cast<std::size_t>(MimitaNet::MAX_PLAYERS);
    if (!existing) {
        mimita::host::EntityHandle players[kMaxInboundPlayers] = {};
        const std::uint32_t count = listHostEntities(
            mimita::host::EntityKind::Player, players, kMaxInboundPlayers);
        if (count >= limit) {
            send_join_reject(message, 1);
            return true;
        }
        if (createHostEntity(mimita::host::EntityKind::Player, &handle) !=
            mimita::host::HostResult::Ok) {
            return false;
        }
    }
    const std::uint32_t playerId = mimita::host::entityId(handle);
    if (!bindSession(message->peer, playerId)) {
        return false;
    }

    mimita::host::EntitySnapshotV1 player{};
    player.structSize = static_cast<std::uint32_t>(sizeof(player));
    if (readHostEntity(handle, &player) != mimita::host::HostResult::Ok) {
        return false;
    }
    const std::string name = unique_name(
        bounded_name(join->name, sizeof(join->name), playerId), playerId);
    std::snprintf(player.displayName, sizeof(player.displayName), "%s",
                  name.c_str());
    std::string avatar(join->avatarName,
                       strnlen(join->avatarName, sizeof(join->avatarName)));
    std::snprintf(player.avatarName, sizeof(player.avatarName), "%s",
                  avatar.c_str());
    std::snprintf(player.joinToken, sizeof(player.joinToken), "%s",
                  joinToken.c_str());
    player.joinTokenValidated = 1;
    if (hasVipTicket) {
        player.accountId = static_cast<std::uint32_t>(vipResult.accountId);
        player.vipAccountId = vipResult.accountId;
        std::snprintf(player.accountRole, sizeof(player.accountRole), "%s",
                      vipResult.role.c_str());
        player.vipTier = vipResult.vipAppearance.tier;
        player.vipStyleKind = vipResult.vipAppearance.styleKind;
        player.vipColorR = vipResult.vipAppearance.colorR;
        player.vipColorG = vipResult.vipAppearance.colorG;
        player.vipColorB = vipResult.vipAppearance.colorB;
        player.vipFlags = vipResult.vipAppearance.flags;
        player.vipStyleEpoch = vipResult.vipStyleDetail.styleEpoch > 0
            ? vipResult.vipStyleDetail.styleEpoch : 1;
        player.vipDetailStyleKind = vipResult.vipStyleDetail.styleKind;
        player.vipDetailAnimation = vipResult.vipStyleDetail.animation;
        player.vipDetailDirection = vipResult.vipStyleDetail.direction;
        player.vipRainbowSpeed = vipResult.vipStyleDetail.rainbowSpeed;
        player.vipSolidColor[0] = vipResult.vipStyleDetail.solidColor.r;
        player.vipSolidColor[1] = vipResult.vipStyleDetail.solidColor.g;
        player.vipSolidColor[2] = vipResult.vipStyleDetail.solidColor.b;
        player.vipSolidColor[3] = vipResult.vipStyleDetail.solidColor.a;
        const std::uint32_t colorCount = static_cast<std::uint32_t>(
            std::min<std::size_t>(vipResult.vipStyleDetail.colors.size(), 32));
        player.vipColorCount = colorCount;
        for (std::uint32_t index = 0; index < colorCount; ++index) {
            player.vipColors[index][0] =
                vipResult.vipStyleDetail.colors[index].r;
            player.vipColors[index][1] =
                vipResult.vipStyleDetail.colors[index].g;
            player.vipColors[index][2] =
                vipResult.vipStyleDetail.colors[index].b;
            player.vipColors[index][3] =
                vipResult.vipStyleDetail.colors[index].a;
        }
        std::snprintf(player.progressionTicket,
                      sizeof(player.progressionTicket), "%s",
                      vipTicket.c_str());
    } else if (!existing) {
        const MimitaVip::VipAppearance free = MimitaVip::freeAppearance();
        player.accountId = 0;
        player.vipAccountId = 0;
        player.accountRole[0] = 0;
        player.vipTier = free.tier;
        player.vipStyleKind = free.styleKind;
        player.vipColorR = free.colorR;
        player.vipColorG = free.colorG;
        player.vipColorB = free.colorB;
        player.vipFlags = free.flags;
        player.vipStyleEpoch = 0;
        player.vipDetailStyleKind = 0;
        player.vipDetailAnimation = 0;
        player.vipDetailDirection = 0;
        player.vipRainbowSpeed = 1.0f;
        player.vipColorCount = 0;
        player.progressionTicket[0] = 0;
    }
    const std::uint32_t hostId = MimitaNet::serverHostPlayerId();
    if (hostId == 0) {
        MimitaNet::setServerHostPlayerId(playerId);
    }
    const std::uint32_t hostMask =
        static_cast<std::uint32_t>(mimita::host::PlayerFlag::Host) |
        static_cast<std::uint32_t>(mimita::host::PlayerFlag::AdminEnabled);
    player.playerFlags &= ~hostMask;
    if (MimitaNet::serverHostPlayerId() == playerId) {
        player.playerFlags |= hostMask;
    }
    if (player.reconnectToken[0] == '\0') {
        const std::string token = MimitaNet::generateReconnectToken();
        std::snprintf(player.reconnectToken, sizeof(player.reconnectToken),
                      "%s", token.c_str());
    }
    player.spawned = existing ? 1u : 0u;
    player.lastHeardMs = MimitaNet::nowMs();
    if (writeHostEntity(handle, &player) != mimita::host::HostResult::Ok) {
        return false;
    }

    if (!existing) {
        mimita::host::WorldSpawnV1 spawn{};
        spawn.structSize = static_cast<std::uint32_t>(sizeof(spawn));
        if (pickHostSpawn(playerId, &spawn) ==
                mimita::host::HostResult::Ok &&
            spawn.found != 0) {
            const float position[3] = {spawn.position[0], spawn.position[1],
                                       spawn.position[2]};
            const float velocity[3] = {0.0f, 0.0f, 0.0f};
            const MovementConfig config = makeCurrentRuntimeMovementConfig();
            resetHostTransform(
                handle, position, velocity, spawn.yaw, 0, 0,
                static_cast<std::uint32_t>(config.maximumAirJumps), 1);
        }
    } else {
        const float position[3] = {player.position[0], player.position[1],
                                   player.position[2]};
        const float velocity[3] = {player.velocity[0], player.velocity[1],
                                   player.velocity[2]};
        const MovementConfig config = makeCurrentRuntimeMovementConfig();
        resetHostTransform(handle, position, velocity, player.yaw,
                           player.onGround, 0,
                           static_cast<std::uint32_t>(config.maximumAirJumps),
                           1);
    }

    player.structSize = static_cast<std::uint32_t>(sizeof(player));
    if (readHostEntity(handle, &player) != mimita::host::HostResult::Ok) {
        return false;
    }
    const std::uint32_t sessionId = sessionEventSessionId(playerId);
    MimitaNet::JoinAcceptPacket accept{};
    accept.header.type = MimitaNet::PACKET_JOIN_ACCEPT;
    accept.header.tick = message->serverTick;
    accept.header.playerId = playerId;
    accept.header.transformEpoch = player.transformEpoch;
    accept.assignedPlayerId = playerId;
    accept.reliableEventSessionId = sessionId;
    accept.tickRate = MimitaNet::SERVER_TICK_RATE;
    std::snprintf(accept.approvedName, sizeof(accept.approvedName), "%s",
                  player.displayName);
    std::snprintf(accept.reconnectToken, sizeof(accept.reconnectToken), "%s",
                  player.reconnectToken);
    std::snprintf(accept.mapId, sizeof(accept.mapId), "%s",
                  MimitaNet::getServerMapId().c_str());
    if (const char* runId = std::getenv("MIMITA_RUN_ID")) {
        std::snprintf(accept.runtimeRunId, sizeof(accept.runtimeRunId), "%s",
                      runId);
    }
    if (const char* buildId = std::getenv("MIMITA_BUILD_ID")) {
        std::snprintf(accept.runtimeBuildId, sizeof(accept.runtimeBuildId),
                      "%s", buildId);
    }
    sendToPeer(message->peer, MimitaNet::PACKET_JOIN_ACCEPT, &accept,
               sizeof(accept));
    sendHostAvatarManifests(handle);
    broadcast_vip_styles(message->serverTick);
    if (!existing) {
        queue_server_chat(message->serverTick,
                          std::string(player.displayName) +
                              " joined the game");
    }
    if (services && services->log) {
        char text[224] = {};
        std::snprintf(text, sizeof(text),
                      "join.local player=%u name=%s existing=%d",
                      playerId, player.displayName, existing ? 1 : 0);
        services->log(services->hostUserData, 1, text);
    }
    return true;
}

void send_chat_reject(std::uint32_t playerId, std::uint32_t requestId,
                      std::uint32_t tick, const char* text) {
    MimitaNet::ChatMessageEventPacket reject{};
    reject.header.type = MimitaNet::PACKET_CHAT_MESSAGE_EVENT;
    reject.header.tick = tick;
    reject.header.playerId = playerId;
    reject.requestId = requestId;
    reject.messageId = 0;
    reject.serverTick = tick;
    reject.senderType = 1;
    std::strncpy(reject.senderName, "server", sizeof(reject.senderName) - 1);
    std::snprintf(reject.utf8Message, sizeof(reject.utf8Message), "%s", text);
    sendToPlayer(playerId, MimitaNet::PACKET_CHAT_MESSAGE_EVENT, &reject,
                 sizeof(reject));
}

bool dispatchChatRequest(const mimita::host::TransportMessageV1* message,
                         const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::ChatRequestPacket)) {
        return false;
    }
    const auto* request =
        reinterpret_cast<const MimitaNet::ChatRequestPacket*>(message->payload);
    if (request->header.type != MimitaNet::PACKET_CHAT_REQUEST ||
        request->header.magic != MimitaNet::PROTOCOL_MAGIC ||
        request->header.version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }
    const std::uint32_t playerId = request->header.playerId;
    mimita::host::EntitySnapshotV1 player{};
    player.structSize = static_cast<std::uint32_t>(sizeof(player));
    if (readHostEntity(
            mimita::host::makeEntityHandle(mimita::host::EntityKind::Player,
                                           playerId),
            &player) != mimita::host::HostResult::Ok) {
        return false;
    }
    const auto& reliable = NetworkingConfig::instance().data().reliableEvents;
    const std::uint32_t maxPending =
        static_cast<std::uint32_t>(reliable.maxPendingPerPlayer);

    if (request->requestId != 0 && player.hasCachedChatEvent != 0 &&
        player.lastChatRequestId == request->requestId) {
        MimitaNet::ChatMessageEventPacket retry{};
        retry.header.type = MimitaNet::PACKET_CHAT_MESSAGE_EVENT;
        retry.header.tick = player.cachedChatServerTick;
        retry.header.playerId = playerId;
        retry.eventId = player.cachedChatEventId;
        retry.eventSessionId = 0;
        retry.requestId = player.cachedChatRequestId;
        retry.messageId = player.cachedChatMessageId;
        retry.serverTick = player.cachedChatServerTick;
        retry.utcUnixMilliseconds = player.cachedChatUtcMs;
        retry.senderEntityId = playerId;
        retry.senderAccountId = player.cachedChatSenderAccountId;
        retry.senderType = 0;
        std::snprintf(retry.senderName, sizeof(retry.senderName), "%s",
                      player.cachedChatSenderName);
        std::snprintf(retry.utf8Message, sizeof(retry.utf8Message), "%s",
                      player.cachedChatText);
        if (retry.eventId != 0 && maxPending > 0) {
            queueSessionEventToPlayer(playerId, &retry, sizeof(retry),
                                      retry.eventId, maxPending);
        }
        return true;
    }

    const char* msg = request->utf8Message;
    while (*msg == ' ' || *msg == '\t' || *msg == '\n' || *msg == '\r') {
        ++msg;
    }
    if (*msg == '\0') {
        send_chat_reject(playerId, request->requestId, message->serverTick,
                         "u cant send nothing, silly!");
        return true;
    }

    constexpr double kBucketSize = 5.0;
    constexpr double kRefillRate = 1.0 / 30.0;
    constexpr std::uint64_t kMaxRefillTicks = 300;
    const std::uint64_t tick = message->serverTick;
    std::uint64_t elapsed = tick >= player.chatLastUpdateTick
        ? tick - player.chatLastUpdateTick : 0;
    if (elapsed > kMaxRefillTicks) {
        elapsed = kMaxRefillTicks;
    }
    double tokens = player.chatTokens +
                    static_cast<double>(elapsed) * kRefillRate;
    if (tokens > kBucketSize) {
        tokens = kBucketSize;
    }
    player.chatLastUpdateTick = tick;
    if (tokens < 1.0) {
        const std::uint64_t remaining = static_cast<std::uint64_t>(
            std::ceil((1.0 - tokens) / kRefillRate));
        player.chatTokens = tokens;
        player.structSize = static_cast<std::uint32_t>(sizeof(player));
        writeHostEntity(
            mimita::host::makeEntityHandle(mimita::host::EntityKind::Player,
                                           playerId),
            &player);
        char text[160] = {};
        std::snprintf(text, sizeof(text),
                      "too fast! u have %llu ticks left before u can chat again",
                      (unsigned long long)remaining);
        send_chat_reject(playerId, request->requestId, message->serverTick,
                         text);
        return true;
    }
    tokens -= 1.0;
    player.chatTokens = tokens;

    const std::uint32_t eventId = nextSessionEventId();
    MimitaNet::ChatMessageEventPacket event{};
    event.header.type = MimitaNet::PACKET_CHAT_MESSAGE_EVENT;
    event.header.tick = message->serverTick;
    event.header.playerId = playerId;
    event.eventId = eventId;
    event.eventSessionId = 0;
    event.requestId = request->requestId;
    event.messageId = eventId;
    event.serverTick = message->serverTick;
    event.utcUnixMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    event.senderEntityId = playerId;
    event.senderAccountId = player.accountId;
    event.senderType = 0;
    std::snprintf(event.senderName, sizeof(event.senderName), "%s",
                  player.displayName);
    std::snprintf(event.utf8Message, sizeof(event.utf8Message), "%s", msg);

    player.lastChatRequestId = request->requestId;
    player.hasCachedChatEvent = 1;
    player.cachedChatEventId = eventId;
    player.cachedChatRequestId = request->requestId;
    player.cachedChatMessageId = event.messageId;
    player.cachedChatServerTick = event.serverTick;
    player.cachedChatUtcMs = event.utcUnixMilliseconds;
    player.cachedChatSenderAccountId = event.senderAccountId;
    std::snprintf(player.cachedChatText, sizeof(player.cachedChatText), "%s",
                  event.utf8Message);
    std::snprintf(player.cachedChatSenderName,
                  sizeof(player.cachedChatSenderName), "%s",
                  event.senderName);
    player.structSize = static_cast<std::uint32_t>(sizeof(player));
    writeHostEntity(
        mimita::host::makeEntityHandle(mimita::host::EntityKind::Player,
                                       playerId),
        &player);

    if (eventId != 0 && maxPending > 0) {
        queueSessionEventToAll(&event, sizeof(event), eventId, maxPending);
    }
    if (services && services->log) {
        char text[192] = {};
        std::snprintf(text, sizeof(text), "chat.v2 player=%u event=%u",
                      playerId, eventId);
        services->log(services->hostUserData, 2, text);
    }
    return true;
}

bool dispatchGodballState(const mimita::host::TransportMessageV1* message,
                          const mimita::host::HostServicesV1*) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::GodballStatePacket)) {
        return false;
    }
    const auto* packet =
        reinterpret_cast<const MimitaNet::GodballStatePacket*>(message->payload);
    if (packet->header.type != MimitaNet::PACKET_GODBALL_STATE ||
        packet->header.magic != MimitaNet::PROTOCOL_MAGIC ||
        packet->header.version != MimitaNet::PROTOCOL_VERSION) {
        return false;
    }
    const std::uint32_t playerId = packet->header.playerId;
    const auto handle = mimita::host::makeEntityHandle(
        mimita::host::EntityKind::Player, playerId);
    mimita::host::EntitySnapshotV1 player{};
    player.structSize = static_cast<std::uint32_t>(sizeof(player));
    if (readHostEntity(handle, &player) != mimita::host::HostResult::Ok) {
        return false;
    }
    player.godballX = packet->posX;
    player.godballY = packet->posY;
    player.godballZ = packet->posZ;
    player.godballVx = packet->velX;
    player.godballVy = packet->velY;
    player.godballVz = packet->velZ;
    player.godballActive = packet->active != 0 ? 1u : 0u;
    writeHostEntity(handle, &player);

    mimita::host::EntityHandle players[kMaxInboundPlayers] = {};
    const std::uint32_t count = listHostEntities(
        mimita::host::EntityKind::Player, players, kMaxInboundPlayers);
    const std::uint32_t limit =
        count < kMaxInboundPlayers ? count : kMaxInboundPlayers;
    for (std::uint32_t i = 0; i < limit; ++i) {
        const std::uint32_t recipient = mimita::host::entityId(players[i]);
        if (recipient == playerId) {
            continue;
        }
        sendToPlayer(recipient, MimitaNet::PACKET_GODBALL_STATE,
                     message->payload, message->payloadSize);
    }
    return true;
}

}  // namespace

bool dispatchInboundMessage(const mimita::host::TransportMessageV1* message,
                            const mimita::host::HostServicesV1* services) {
    if (!message || !message->payload ||
        message->payloadSize < sizeof(MimitaNet::PacketHeader)) {
        return false;
    }
    const auto* header =
        reinterpret_cast<const MimitaNet::PacketHeader*>(message->payload);
    switch (header->type) {
        case MimitaNet::PACKET_PING:
            return dispatchPingMessage(message, services);
        case MimitaNet::PACKET_CHAT_MESSAGE:
            return dispatchChatMessage(message, services);
        case MimitaNet::PACKET_CHAT_TYPING_STATE_REQUEST:
            return dispatchChatTyping(message, services);
        case MimitaNet::PACKET_SPAWN_NPC_REQUEST:
            return dispatchSpawnNpc(message, services);
        case MimitaNet::PACKET_TELEPORT_REQUEST:
            return dispatchTeleport(message, services);
        case MimitaNet::PACKET_DISCONNECT:
            return dispatchDisconnect(message, services);
        case MimitaNet::PACKET_RECONNECT_REQUEST:
            return dispatchReconnect(message, services);
        case MimitaNet::PACKET_HELLO:
            return dispatchHello(message, services);
        case MimitaNet::PACKET_JOIN_REQUEST:
            return dispatchJoin(message, services);
        case MimitaNet::PACKET_CHAT_REQUEST:
            return dispatchChatRequest(message, services);
        case MimitaNet::PACKET_GODBALL_STATE:
            return dispatchGodballState(message, services);
        default:
            return false;
    }
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

#pragma once

#include "network/net_common.h"
#include "network/packets.h"
#include "entities/player.h"

#include <string>
#include <unordered_map>

namespace MimitaNet {

struct PlayerInfo
{
    std::string name;
    uint32_t id = 0;
    int pingMs = 0;
};

struct MultiplayerContext
{
    bool active = false;
    SOCKET sock = INVALID_SOCKET;
    sockaddr_in serverAddr{};
    uint32_t localPlayerId = 0;
    uint32_t tick = 0;
    uint64_t lastHelloMs = 0;
    uint64_t lastSnapshotTick = 0;
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    std::unordered_map<uint32_t, Player> remotePlayers;
    std::unordered_map<uint32_t, Player> remoteNpcs;
    std::unordered_map<uint32_t, PlayerInfo> playerRegistry;
    glm::vec3 localServerPosition{0.0f};
    bool hasLocalServerPosition = false;
    std::string approvedLocalName;
    std::string serverAddress = "127.0.0.1:1357";
    bool showPlayerList = false;
    bool showDebugOverlay = false;
};

bool mpInit(MultiplayerContext& ctx, const std::string& address, const std::string& playerName);
void mpShutdown(MultiplayerContext& ctx);
void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt);
void mpRequestNpcSpawn(MultiplayerContext& ctx, const glm::vec3& position);

} // namespace MimitaNet

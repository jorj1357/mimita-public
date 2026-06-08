#pragma once

#include "network/net_common.h"
#include "network/packets.h"
#include "entities/player.h"

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace MimitaNet {

struct PlayerInfo
{
    std::string name;
    uint32_t id = 0;
    int pingMs = 0;
};

struct SnapshotTransform
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    int health = 100;
    bool onGround = false;
    uint32_t serverTick = 0;
    uint64_t receivedMs = 0;
};

struct EntityInterpolationState
{
    SnapshotTransform previous;
    SnapshotTransform target;
    bool hasPrevious = false;
    bool hasTarget = false;
    bool renderRegistered = false;
    std::string displayName;
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
    uint64_t lastSnapshotReceivedMs = 0;
    uint64_t connectStartMs = 0;
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    uint64_t snapshotsReceived = 0;
    uint64_t snapshotsMissed = 0;
    std::unordered_map<uint32_t, Player> remotePlayers;
    std::unordered_map<uint32_t, Player> remoteNpcs;
    std::unordered_map<uint32_t, EntityInterpolationState> remotePlayerInterpolation;
    std::unordered_map<uint32_t, EntityInterpolationState> remoteNpcInterpolation;
    std::unordered_map<uint32_t, PlayerInfo> playerRegistry;
    glm::vec3 localServerPosition{0.0f};
    glm::vec3 localServerVelocity{0.0f};
    float localServerYaw = 0.0f;
    bool localServerOnGround = false;
    bool hasLocalServerPosition = false;
    bool localPlayerReconciled = false;
    int localServerHealth = 100;
    std::string approvedLocalName;
    std::string serverAddress = "127.0.0.1:1357";
    std::string connectionStatus;
    bool connected = false;
    bool connectFailed = false;
    bool showPlayerList = false;
    bool showDebugOverlay = true;
};

bool mpInit(MultiplayerContext& ctx, const std::string& address, const std::string& playerName);
void mpShutdown(MultiplayerContext& ctx);
void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt);
void mpReconcileLocalPlayer(MultiplayerContext& ctx, Player& player, float dt);
void mpRequestNpcSpawn(MultiplayerContext& ctx, const glm::vec3& position);

} // namespace MimitaNet

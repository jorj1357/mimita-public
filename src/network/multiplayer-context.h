#pragma once

#include "network/net_common.h"
#include "network/packets.h"
#include "entities/player.h"

#include <string>
#include <unordered_map>
#include <vector>
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
    int equippedSlot = 0;
    uint8_t weaponState = 0;
    glm::vec3 aimDirection{1.0f, 0.0f, 0.0f};
    int pingMs = 0;
    uint32_t serverTick = 0;
    uint64_t receivedMs = 0;
    uint16_t lastDashSerial = 0;
};

struct QueuedPacket
{
    std::vector<char> bytes;
    uint64_t deliverAtMs = 0;
};

struct NetworkShotEvent
{
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint32_t shooterPlayerId = 0;
    uint32_t targetPlayerId = 0;
    int damage = 0;
    int targetHealth = 0;
    float power = 0.0f;
    uint16_t effectFlags = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_NONE;
    bool killed = false;
    bool damageConfirmed = false;
    glm::vec3 origin{0.0f};
    glm::vec3 hit{0.0f};
    glm::vec3 direction{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec3 knockback{0.0f};
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
    uint64_t lastLocalCorrectionLogMs = 0;
    glm::vec3 pendingTeleportPosition{0.0f};
    uint64_t pendingTeleportSentMs = 0;
    bool awaitingTeleportAck = false;
    bool awaitingExplodeDeath = false;
    int localServerHealth = 100;
    std::string approvedLocalName;
    std::string serverAddress = "127.0.0.1:1357";
    std::string connectionStatus;
    bool connected = false;
    bool connectFailed = false;
    bool showPlayerList = false;
    bool showDebugOverlay = true;
    int fakeLagMode = 0;
    int fakeLagStaticMs = 0;
    int fakeLagMinMs = 0;
    int fakeLagMaxMs = 0;
    int fakeLagCurrentMs = 0;
    uint64_t fakeLagNextRandomizeMs = 0;
    uint64_t lastFakeLagLogMs = 0;
    std::vector<QueuedPacket> outgoingQueue;
    std::vector<NetworkShotEvent> shotEvents;
    struct IncomingChatMessage
    {
        std::string senderName;
        std::string text;
    };
    std::vector<IncomingChatMessage> incomingChatMessages;
    std::unordered_map<uint32_t, uint32_t> lastReceivedShotSerial;
    uint32_t nextLocalShotSerial = 1;
    uint32_t latestServerTick = 0;
    uint64_t lastPingSentMs = 0;
    int localPingMs = 0;
};

bool mpInit(MultiplayerContext& ctx, const std::string& address, const std::string& playerName);
void mpShutdown(MultiplayerContext& ctx);
void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt);
void mpReconcileLocalPlayer(MultiplayerContext& ctx, Player& player, float dt);
void mpRequestNpcSpawn(MultiplayerContext& ctx, const glm::vec3& position, float difficulty = 1.0f);
void mpRequestTeleport(MultiplayerContext& ctx, const glm::vec3& position);
void mpRequestExplode(MultiplayerContext& ctx);
uint32_t mpSendShotEvent(
    MultiplayerContext& ctx,
    uint32_t targetPlayerId,
    int damage,
    float power,
    uint16_t effectFlags,
    uint8_t weapon,
    uint8_t impactType,
    const glm::vec3& origin,
    const glm::vec3& hit,
    const glm::vec3& direction,
    const glm::vec3& normal,
    const glm::vec3& knockbackImpulse = glm::vec3(0.0f));
void mpSendPacket(MultiplayerContext& ctx, const void* data, int bytes);
void mpSetFakeLagMode(MultiplayerContext& ctx, int mode);
void mpSetFakeLagStatic(MultiplayerContext& ctx, int milliseconds);
void mpSetFakeLagRange(MultiplayerContext& ctx, int minimumMs, int maximumMs);

// Debug flags for damage/hit/net diagnostics (extern, set from terminal commands)
extern bool gNetDamageDebug;
extern bool gNetHitDebug;

} // namespace MimitaNet

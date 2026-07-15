#pragma once

#include "network/net_common.h"
#include "network/packets.h"
#include "entities/player.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

namespace MimitaNet {

// ── Connection state machine ──────────────────────────────────────────
enum class ConnectionState : uint8_t
{
    Disconnected,
    ResolvingCode,
    RequestingJoin,
    WaitJoinAccept,
    NatNegotiating,
    Connecting,
    Connected,
    Reconnecting,
    DisconnectPending
};

const char* connectionStateName(ConnectionState state);

// ── Server disagreement event (for client-side visual effects) ────────
struct DisagreementEvent
{
    uint64_t timeMs = 0;
    DisagreementReason reason = DISAGREEMENT_NONE;
    glm::vec3 position{0.0f};
    glm::vec3 correction{0.0f};
    std::string description;
    float lifetime = 3.0f;
};

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
    float sizeScale = 1.0f;
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
    uint16_t localServerEpoch = 0;
    uint16_t lastAppliedEpoch = 0;
    bool hasLocalServerPosition = false;
    bool localPlayerReconciled = false;
    uint64_t lastLocalCorrectionLogMs = 0;
    glm::vec3 pendingTeleportPosition{0.0f};
    uint64_t pendingTeleportSentMs = 0;
    bool awaitingTeleportAck = false;
    bool awaitingExplodeDeath = false;
    bool teleportResync = false;
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
    uint64_t lastHeardServerMs = 0;
    uint64_t lastDisconnectLogMs = 0;

    // ── Migration: connection state machine ───────────────────────────
    ConnectionState connectionState = ConnectionState::Disconnected;
    std::string roomCode;
    std::string joinToken;
    std::string reconnectToken;
    std::string requiredMapId;
    int reconnectAttempts = 0;
    uint64_t lastReconnectAttemptMs = 0;
    uint64_t reconnectBackoffMs = 1000;

    // ── Migration: disagreement events from server ────────────────────
    std::vector<DisagreementEvent> disagreementEvents;

    // ── Migration: server process tracking ────────────────────────────
    bool serverProcessLaunched = false;
    uint64_t serverProcessLaunchMs = 0;
    uint16_t serverPort = 1357;

    // ── Transform epoch for spawn/resync detection ────────────────────
    uint32_t transformEpoch = 0;

    // ── Ghost: show authoritative server position ─────────────────────
    bool showServerGhost = false;
    bool waitingForMapLoad = false;

    // ── ClientMapReady tracking ───────────────────────────────────────
    bool clientMapReadySent = false;
    std::string clientMapReadySentForMap;
    uint32_t clientMapReadySentForPlayerId = 0;

    // ── Room code for HUD display ─────────────────────────────────────
    // Set on host register or client join, cleared on disconnect/leave.
    std::string currentRoomCode;
};

struct MpInput
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    glm::vec3 camForward{1.0f, 0.0f, 0.0f};
    float wishX = 0.0f;
    float wishY = 0.0f;
    bool jumpHeld = false;
    bool dashPressed = false;
    bool attackPressed = false;
    bool freezeHeld = false;
    int equippedSlot = 0;
    uint8_t weaponState = 0;
    float sizeScale = 1.0f;
};

bool mpInit(MultiplayerContext& ctx, const std::string& address, const std::string& playerName);
void mpShutdown(MultiplayerContext& ctx);
void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt, const MpInput* input = nullptr);
void mpReconcileLocalPlayer(MultiplayerContext& ctx, Player& player, float dt);

// ── Migration: connection with room code + join token ─────────────────
bool mpConnectWithToken(MultiplayerContext& ctx, const std::string& address,
    uint16_t port, const std::string& joinToken, const std::string& playerName);

// ── Migration: disagreement packet processing ─────────────────────────
void mpProcessDisagreementPacket(MultiplayerContext& ctx, const DisagreementPacket* packet);

// ── Migration: reconnect helpers ──────────────────────────────────────
void mpStartReconnect(MultiplayerContext& ctx);
void mpTickReconnect(MultiplayerContext& ctx);
void mpRequestNpcSpawn(MultiplayerContext& ctx, const glm::vec3& position, float difficulty = 1.0f);
void mpRequestTeleport(MultiplayerContext& ctx, const glm::vec3& position);
void mpRequestExplode(MultiplayerContext& ctx);
void mpSendNpcDamageRequest(MultiplayerContext& ctx, uint32_t npcEntityId, int damage,
    const glm::vec3& origin, const glm::vec3& hit, const glm::vec3& direction,
    const glm::vec3& normal, const glm::vec3& knockback, uint16_t effectFlags, uint8_t weapon);
void mpSendServerCommand(MultiplayerContext& ctx, const std::string& command);
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

// Packet queue flush (defined in multiplayer-packets.cpp)
void flushOutgoingPackets(MultiplayerContext& ctx);

void mpSetFakeLagMode(MultiplayerContext& ctx, int mode);
void mpSetFakeLagStatic(MultiplayerContext& ctx, int milliseconds);
void mpSetFakeLagRange(MultiplayerContext& ctx, int minimumMs, int maximumMs);

// Called from mpTick (defined in multiplayer-shots.cpp)
void mpProcessShotEventPacket(MultiplayerContext& ctx, const ShotEventPacket* event);
void mpProcessNpcDamageEventPacket(MultiplayerContext& ctx, const NpcDamageEventPacket* event);

// Called from mpTick (defined in multiplayer-chat.cpp)
void mpProcessChatPacket(MultiplayerContext& ctx, const ChatPacket* chat);

// Interpolation helpers (defined in multiplayer-interpolation.cpp)
void pushInterpolationTarget(EntityInterpolationState& interpolation, const SnapshotEntity& entity, uint32_t serverTick);
void updateRenderedReplica(Player& player, EntityInterpolationState& interpolation, float dt);
void mpUpdateRemoteEntities(MultiplayerContext& ctx, float dt);

// Debug flags for damage/hit/net diagnostics (extern, set from terminal commands)
extern bool gNetDamageDebug;
extern bool gNetHitDebug;

} // namespace MimitaNet

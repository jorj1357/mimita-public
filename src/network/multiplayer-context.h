#pragma once

#include "network/net_common.h"
#include "network/packets.h"
#include "entities/player.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "combat/weapon-swordsword.h"
#include "network/game-transport.h"

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
    uint32_t eventId = 0;
    uint32_t relatedSerial = 0;
    uint32_t sourcePlayerId = 0;
    uint32_t targetPlayerId = 0;
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
    uint16_t stateFlags = 0;
    float sizeScale = 1.0f;
    uint32_t networkEntityId = 0;
    uint16_t transformEpoch = 0;
    uint16_t dashSerial = 0;
    uint16_t groundJumpSerial = 0;
    uint16_t airJumpSerial = 0;
    uint16_t downDashSerial = 0;
    uint16_t directionChangeSerial = 0;
    uint16_t equipSerial = 0;
    uint16_t freezeSerial = 0;
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
    uint16_t targetTransformEpoch = 0;
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

struct NetworkProjectile
{
    uint32_t projectileId = 0;
    uint32_t ownerPlayerId = 0;
    uint32_t fireSerial = 0;
    uint8_t weaponType = NETWORK_WEAPON_NONE;

    // Authoritative/server state (directly from packets, NOT for rendering)
    glm::vec3 position{0.0f};
    glm::vec3 previousPosition{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity{0.0f};
    float age = 0.0f;
    float lifetime = 0.0f;
    float radius = 0.0f;
    float smokeAccumulator = 0.0f;
    bool predicted = false;
    bool exploded = false;

    // Interpolation state (for smooth visual rendering)
    glm::vec3 renderPosition{0.0f};
    glm::vec3 renderVelocity{0.0f};
    glm::quat renderRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 renderAngularVelocity{0.0f};

    uint32_t prevStateTick = 0;
    glm::vec3 prevStatePos{0.0f};
    glm::vec3 prevStateVel{0.0f};
    glm::quat prevStateRot{1.0f, 0.0f, 0.0f, 0.0f};

    uint32_t targetStateTick = 0;
    glm::vec3 targetStatePos{0.0f};
    glm::vec3 targetStateVel{0.0f};
    glm::quat targetStateRot{1.0f, 0.0f, 0.0f, 0.0f};

    uint32_t latestAcceptedTick = 0;
    uint64_t lastTargetReceivedMs = 0;
    bool hasTargetState = false;
};

struct EntityInterpolationState
{
    SnapshotTransform previous;
    SnapshotTransform target;
    bool hasPrevious = false;
    bool hasTarget = false;
    bool renderRegistered = false;
    uint32_t networkEntityId = 0;
    uint16_t lastTransformEpoch = 0;
    std::string displayName;
};

struct MultiplayerContext
{
    bool active = false;
    SOCKET sock = INVALID_SOCKET;
    std::unique_ptr<IGameTransport> transport;
    bool useIce = false;
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
    int lastSeenServerHealth = 100;
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
    std::unordered_map<uint32_t, NetworkProjectile> networkProjectiles;
    struct IncomingChatMessage
    {
        std::string senderName;
        std::string text;
    };
    std::vector<IncomingChatMessage> incomingChatMessages;
    std::unordered_map<uint32_t, uint32_t> lastReceivedShotSerial;
    uint32_t nextLocalShotSerial = 1;
    uint32_t nextLocalProjectileFireSerial = 1;
    uint32_t nextLocalMeleeAttackSerial = 1;
    uint32_t latestServerTick = 0;
    uint64_t lastPingSentMs = 0;
    int localPingMs = 0;
    uint64_t lastHeardServerMs = 0;
    uint64_t lastDisconnectLogMs = 0;

    // ── Connection lifecycle ──────────────────────────────────────────
    ConnectionState connectionState = ConnectionState::Disconnected;
    std::string roomCode;
    std::string joinToken;
    std::string reconnectToken;
    std::string requiredMapId;
    int reconnectAttempts = 0;
    uint64_t lastReconnectAttemptMs = 0;
    uint64_t reconnectBackoffMs = 1000;

    // ── Session identity (monotonically increasing, never reset) ──────
    uint32_t connectionAttemptId = 0;
    std::string sessionId; // server-session identifier for reconnect-token policy

    // ── Migration: disagreement events from server ────────────────────
    std::vector<DisagreementEvent> disagreementEvents;
    std::unordered_set<uint32_t> processedDisagreementIds;
    std::unordered_set<uint64_t> processedPelletBlastSerials;

    // ── Migration: server process tracking ────────────────────────────
    bool serverProcessLaunched = false;
    uint64_t serverProcessLaunchMs = 0;
    uint16_t serverPort = 1357;

    // ── Transform epoch for spawn/resync detection ────────────────────
    uint32_t transformEpoch = 0;

    // ── Ghost: show authoritative server position ─────────────────────
    bool showServerGhost = false;
    bool waitingForMapLoad = false;

    // ── Remote sword state for visual reconstruction ──────────────────
    std::unordered_map<uint32_t, SwordswordState> remoteSwordStates;

    // ── ClientMapReady tracking ───────────────────────────────────────
    bool clientMapReadySent = false;

    // ── Local event serials for remote replication ────────────────────
    // Persistent monotonic counters. Never reset to 0.
    // Incremented when the corresponding gameplay event flag is set.
    uint16_t nextLocalDashSerial = 0;
    uint16_t nextLocalGroundJumpSerial = 0;
    uint16_t nextLocalAirJumpSerial = 0;
    uint16_t nextLocalDownDashSerial = 0;
    uint16_t nextLocalFreezeSerial = 0;
    uint16_t nextLocalMovementDirectionSerial = 0;
    uint16_t nextLocalEquipSerial = 0;
    // ── Respawn lifecycle ─────────────────────────────────────────────
    uint16_t nextLocalRespawnSerial = 1;   // permanently monotonic, never reset to 0
    uint16_t pendingRespawnSerial = 0;     // 0 = no pending request
    uint16_t pendingRespawnStartEpoch = 0;
    uint64_t pendingRespawnStartedMs = 0;
    uint64_t pendingRespawnLastSendLogMs = 0;

    // Pending projectile knockback impulse (consumed in engineTickNet)
    glm::vec3 pendingKnockback{0.0f};
    std::string pendingKnockbackSource;
    std::string clientMapReadySentForMap;
    uint32_t clientMapReadySentForPlayerId = 0;

    // ── Snapshot lifecycle tracking for stale-state rejection ─────────
    uint32_t latestLocalSnapshotTick = 0;
    uint32_t latestAliveSnapshotTick = 0;

    // ── Pending projectile fire requests (retransmission) ────────────
    struct PendingFireRequest {
        uint32_t fireSerial = 0;
        uint8_t weapon = NETWORK_WEAPON_NONE;
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f};
        uint64_t firstSentMs = 0;
        uint64_t lastSentMs = 0;
        int attempts = 0;
        bool acknowledged = false;
    };
    std::unordered_map<uint32_t, PendingFireRequest> pendingFireRequests;

    // ── Predicted projectile IDs (locally simulated, suppress server interpolation) ──
    std::unordered_set<uint32_t> predictedProjectileIds;

    // ── Snapshot chunk reassembly buffers ─────────────────────────────
    struct SnapshotChunkBuffer {
        std::unordered_map<uint16_t, SnapshotChunkPacket> chunks;
        uint64_t lastReceiveMs = 0;
    };
    std::unordered_map<uint32_t, SnapshotChunkBuffer> snapshotChunkBuffers;

    // ── Rejected projectile fire requests (for ammo refund) ──────────
    struct FireRejection {
        uint32_t fireSerial = 0;
        uint8_t weapon = 0;
        uint8_t reason = 0;
        float cooldownRemaining = 0.0f;
    };
    std::vector<FireRejection> fireRejections;
    // Tracks processed fireSerials to prevent double-refund across frames
    std::unordered_set<uint32_t> processedRefundSerials;

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
    glm::vec3 godballPosition{0.0f};
    bool godballActive = false;
};

// ── Connection lifecycle — central session management ─────────────────
enum class DisconnectPolicy : uint8_t {
    Leave,               // explicit leave — clear all
    Timeout,             // unintentional — preserve reconnectToken
    NewConnection,       // new room/server — clear all
    Rejected,            // server/coordinator rejected — clear all
    AuthFailure,         // token/auth failure — clear all
    ConnectionFailure,   // transport failure — clear all
    ServerStopped        // local server stopped — clear all
};
const char* disconnectPolicyName(DisconnectPolicy policy);
void teardownPreviousSession(MultiplayerContext& ctx, DisconnectPolicy policy);
void beginConnectionAttempt(MultiplayerContext& ctx, const std::string& roomCode,
    const std::string& address, uint16_t port);

bool mpInit(MultiplayerContext& ctx, const std::string& address, const std::string& playerName);
void mpShutdown(MultiplayerContext& ctx);
void mpTick(MultiplayerContext& ctx, const std::string& playerName, float dt, const MpInput* input, const class World& world);
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
uint32_t mpSendProjectileFireRequest(
    MultiplayerContext& ctx,
    uint8_t weapon,
    const glm::vec3& origin,
    const glm::vec3& direction);
bool mpIceConnect(MultiplayerContext& ctx, const std::string& roomCode,
                  const std::string& playerName);
uint32_t mpSendMeleeHitRequest(
    MultiplayerContext& ctx,
    uint32_t targetPlayerId,
    int damage,
    uint8_t weapon,
    uint8_t attackType,
    const glm::vec3& hit,
    const glm::vec3& normal,
    const glm::vec3& knockback,
    float weaponSpeed);
void mpSendPacket(MultiplayerContext& ctx, const void* data, int bytes);

// Packet queue flush (defined in multiplayer-packets.cpp)
void flushOutgoingPackets(MultiplayerContext& ctx);

void mpSetFakeLagMode(MultiplayerContext& ctx, int mode);
void mpSetFakeLagStatic(MultiplayerContext& ctx, int milliseconds);
void mpSetFakeLagRange(MultiplayerContext& ctx, int minimumMs, int maximumMs);

// Called from mpTick (defined in multiplayer-shots.cpp)
void mpProcessShotEventPacket(MultiplayerContext& ctx, const ShotEventPacket* event);
void mpProcessNpcDamageEventPacket(MultiplayerContext& ctx, const NpcDamageEventPacket* event);
void mpProcessProjectileSpawnEventPacket(MultiplayerContext& ctx, const ProjectileSpawnEventPacket* event);
void mpProcessProjectileStateEventPacket(MultiplayerContext& ctx, const ProjectileStateEventPacket* event);
void mpProcessProjectileExplodeEventPacket(MultiplayerContext& ctx, const ProjectileExplodeEventPacket* event);
void mpProcessProjectileDespawnEventPacket(MultiplayerContext& ctx, const ProjectileDespawnEventPacket* event);
void mpProcessProjectileFireResultPacket(MultiplayerContext& ctx, const ProjectileFireResultPacket* event);
void mpProcessMeleeHitEventPacket(MultiplayerContext& ctx, const MeleeHitEventPacket* event);
void mpUpdateRemoteSwordStates(MultiplayerContext& ctx, float dt);
void mpSendPelletBlastRequest(MultiplayerContext& ctx, uint8_t weapon, const glm::vec3& origin, const glm::vec3& baseDirection, uint32_t spreadSeed);
void mpProcessPelletBlastEventPacket(MultiplayerContext& ctx, const PelletBlastEventPacket* event);
void mpUpdateNetworkProjectiles(MultiplayerContext& ctx, float dt);
void mpRenderNetworkProjectiles(const MultiplayerContext& ctx, const Camera& camera);

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

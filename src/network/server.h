// 07 19 2026, 11 05
/* purpose
* Declares authoritative server state, constants, and subsystem entry points.
* Shares fixed 60 Hz server interfaces with network, projectile, NPC, and player code.
* Exposes lightweight diagnostics required by server tick stability reporting.
* Does NOT implement packet transport, rendering, or gameplay simulation bodies.
* Does NOT define local-only gameplay paths or client prediction behavior.
* Does NOT own weapon definitions, website APIs, or asset loading policy.
*/

#pragma once

#include "network/net_common.h"
#include "network/packets.h"
#include "network/game-transport.h"
#include "network/ice/ice-agent.h"
#include "network/movement-validation.h"
#include "network/simulation-constants.h"
#include "physics/physics-types.h"
#include "physics/config.h"
#include "combat/weapon-swordsword.h"

#include <memory>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

namespace MimitaNet {

// Simulation rate shared with client via simulation-constants.h
constexpr float SERVER_TICK_RATE = static_cast<float>(GAMEPLAY_SIMULATION_HZ);
constexpr float SERVER_DT = GAMEPLAY_FIXED_DT;
constexpr float PLAYER_RADIUS = 0.65f;
constexpr float PLAYER_HEIGHT = 3.5f;

struct ServerSpawnPoint
{
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
};

// Hash for ivec3 keys used in HeadlessWorld spatial grid
struct HeadlessIVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ std::hash<int>()(v.y) ^ std::hash<int>()(v.z);
    }
};

struct HeadlessWorld
{
    std::vector<CollisionTriangle> triangles;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    std::vector<ServerSpawnPoint> spawnPoints;

    // Uniform spatial grid for accelerating projectile-triangle queries
    float collisionChunkSize = 6.0f;
    std::unordered_map<glm::ivec3, std::vector<int>, HeadlessIVec3Hash> collisionChunks;
    std::vector<int> collisionLargeTriangles;
};

struct ServerInput
{
    glm::vec2 wish{0.0f};
    glm::vec3 camForward{1.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float lookPitch = 0.0f;
    bool jumpHeld = false;
    bool dashPressed = false;
    bool downDashPressed = false;
    bool attackPressed = false;
    bool freezeHeld = false;
    uint32_t tick = 0;
};

enum class TransportKind : uint8_t
{
    Udp,
    Ice,
    Relay
};

struct TransportConnectionId
{
    TransportKind kind = TransportKind::Udp;
    uint64_t value = 0;

    bool operator==(const TransportConnectionId& other) const
    {
        return kind == other.kind && value == other.value;
    }
};

struct TransportReceiveEvent
{
    TransportConnectionId connectionId{};
    sockaddr_in remoteEndpoint{};
    const uint8_t* payload = nullptr;
    int payloadBytes = 0;
    uint64_t receivedAtMs = 0;
    TransportKind transportKind = TransportKind::Udp;
};

struct PendingServerTransport
{
    TransportConnectionId connectionId{};
    sockaddr_in diagnosticEndpoint{};
    uint64_t connectedAtMs = 0;
    std::unique_ptr<IGameTransport> transport;
};

struct ServerPacketStats
{
    uint64_t recvAttempts = 0;
    uint64_t recvWouldBlock = 0;
    uint64_t recvErrors = 0;
    uint64_t malformedPackets = 0;
    uint64_t protocolMismatches = 0;
    uint64_t unknownPacketTypes = 0;
    uint64_t helloPackets = 0;
    uint64_t joinPackets = 0;
    uint64_t reconnectPackets = 0;
    uint64_t inputPackets = 0;
};

struct ServerPacketProcessResult
{
    bool decoded = false;
    bool handled = false;
    bool transportConsumed = false;
    uint32_t playerId = 0;
};

struct PositionHistoryEntry
{
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    uint32_t tick = 0;
};

struct ServerPlayer
{
    uint32_t id = 0;
    std::string name;
    sockaddr_in addr{};
    TransportConnectionId connectionId{};
    bool hasConnectionId = false;
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float yaw = 0.0f;
    int health = 100;
    bool onGround = false;
    bool dashAvailable = true;
    bool attackQueued = false;
    bool dead = false;
    float respawnSeconds = 0.0f;
    uint64_t lastHeardMs = 0;
    bool clientStateUpdated = false;
    int equippedSlot = 0;
    uint8_t weaponState = 0;
    int pingMs = 0;
    uint32_t lastShotSerial = 0;
    uint32_t lastProjectileFireSerial = 0;

    // ── Idempotent projectile fire result cache ───────────────────────
    // When the client retries a fire request, the server must respond with
    // the same result instead of rejecting the duplicate.
    static constexpr uint8_t MAX_CACHED_FIRE_RESULTS = 32;
    struct CachedFireResult {
        uint32_t fireSerial = 0;
        bool accepted = false;
        uint32_t projectileId = 0;
        uint8_t weapon = NETWORK_WEAPON_NONE;
        uint8_t reason = 0;
        float cooldownRemaining = 0.0f;
        bool valid = false;
    };
    CachedFireResult cachedFireResults[MAX_CACHED_FIRE_RESULTS] = {};
    uint8_t nextCachedFireResultSlot = 0;
    uint32_t lastMeleeAttackSerial = 0;
    float projectileFireCooldown = 0.0f;
    uint64_t nextProjectileFireTick = 0; // tick-based cooldown for projectiles

    // ── Authoritative per-player per-weapon runtime ─────────────────────
    struct ServerWeaponRuntime {
        int magazineAmmo = 0;
        int reserveAmmo = 0;
        uint64_t nextAllowedFireTick = 0;
        bool reloading = false;
        uint64_t reloadCompleteTick = 0;
        uint32_t stateRevision = 0;
        bool initialized = false;
    };
    std::unordered_map<std::string, ServerWeaponRuntime> weaponRuntimes;
    uint32_t spawnGeneration = 0;
    std::vector<std::string> ownedWeaponIds;
    bool justRespawned = false;  // set by simulatePlayer when respawn timer fires

    // ── Spawn handshake lifecycle ───────────────────────────────────
    enum SpawnState : uint8_t {
        AwaitingMapReady,   // after JoinAccept, waiting for PACKET_CLIENT_MAP_READY
        AwaitingSpawnAck,   // spawn sent, waiting for position acknowledgement
        Active              // normal gameplay
    };
    SpawnState spawnState = AwaitingMapReady;
    uint16_t lastDashSerial = 0;
    uint16_t lastEquipSerial = 0;
    uint16_t lastRespawnSerial = 0;
    bool instantRespawnRequested = false;
    uint16_t inputStateFlags = 0;
    float dashCooldownTimer = 0.0f;
    float sizeScale = 1.0f;
    float godballX = 0.0f, godballY = 0.0f, godballZ = 0.0f;
    bool godballActive = false;
    // Presentation serials (replicated from client, pass through unchanged)
    uint16_t lastPresentationDashSerial = 0;
    uint16_t lastPresentationGroundJumpSerial = 0;
    uint16_t lastPresentationAirJumpSerial = 0;
    uint16_t lastPresentationDownDashSerial = 0;
    uint16_t lastPresentationDirectionChangeSerial = 0;
    uint16_t lastPresentationFreezeSerial = 0;
    ServerInput input;
    std::unique_ptr<IGameTransport> transport;
    std::string iceSessionId;
    SwordswordState swordswordState;
    float meleeCooldownTimer = 0.0f;
    std::deque<PositionHistoryEntry> posHistory;
    // ── Migration: auth + reconnect ───────────────────────────────────
    std::string joinToken;
    std::string reconnectToken;
    bool joinTokenValidated = false;
    bool spawned = false;
    int kills = 0;
    int deaths = 0;
    uint16_t transformEpoch = 0;

    // ── Shared movement parity and Stage 3A report validation ─────────
    MovementState movement;
    MovementValidationCounters movementValidation;
    uint32_t lastMovementSequence = 0;
    bool hasMovementSequence = false;

    // ── Authoritative transform acknowledgement gate ──────────────────
    // When the server sets a discontinuous transform (spawn, respawn,
    // teleport, map change), it increments the epoch and marks this flag.
    // Until the client acknowledges by sending a matching epoch with a
    // position near the authoritative one, the server rejects client
    // position updates and keeps its own authoritative position.
    bool awaitingAuthoritativeTransformAck = false;
    glm::vec3 authoritativeTransformPosition{0.0f};
    uint16_t authoritativeTransformEpoch = 0;
    uint64_t authoritativeTransformAssignedMs = 0;

    // ── Last accepted client transform (for trajectory validation) ────
    // The last client-reported position/velocity that the server accepted.
    // Used to compute elapsed-time movement envelopes instead of comparing
    // against the server-simulated p.pos, which may diverge after packet
    // loss or rejection.
    glm::vec3 lastAcceptedClientPosition{0.0f};
    glm::vec3 lastAcceptedClientVelocity{0.0f};
    uint64_t lastAcceptedClientTransformMs = 0;
    bool hasAcceptedClientTransform = false;

    // ── Reliable unordered gameplay events ───────────────────────────
    uint32_t reliableEventSessionId = 0;
    struct PendingReliableEvent
    {
        uint32_t eventId = 0;
        uint32_t eventSessionId = 0;
        uint8_t packetType = 0;
        uint64_t createdMs = 0;
        uint64_t lastSendMs = 0;
        uint8_t attempts = 0;
        std::vector<char> bytes;
    };
    std::deque<PendingReliableEvent> pendingReliableEvents;
};

enum class ReliableGameplayEventQueueResult : uint8_t
{
    Queued,
    ConnectionUnavailable,
    BacklogSaturated
};

enum class ServerNpcState {
    Chase,
    Strafe,
    Retreat,
    Orbit
};

struct ServerNpc
{
    uint32_t entityId = 0;
    std::string name;
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float yaw = 0.0f;
    int health = 100;
    bool onGround = false;
    float phase = 0.0f;
    float difficulty = 1.0f;
    float lastAttackTime = 0.0f;
    float strafeDir = 1.0f;
    float stateTimer = 0.0f;
    float orbitAngle = 0.0f;
    ServerNpcState aiState = ServerNpcState::Chase;
};

enum class ServerDamageSource : uint8_t
{
    Hitscan,
    Melee,
    RocketExplosion,
    GrenadeExplosion
};

struct ServerDamageResult
{
    bool applied = false;
    bool killed = false;
    int healthBefore = 0;
    int healthAfter = 0;
};

struct ServerProjectile
{
    uint32_t id = 0;
    uint32_t ownerPlayerId = 0;
    uint32_t fireSerial = 0;
    uint8_t weaponType = NETWORK_WEAPON_NONE;
    glm::vec3 position{0.0f};
    glm::vec3 previousPosition{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity{0.0f};
    float age = 0.0f;
    float lifetime = 0.0f;
    float radius = 0.0f;
    float splashRadius = 0.0f;
    float splashDamage = 0.0f;
    float splashExponent = 2.0f;
    float knockbackStrength = 0.0f;
    float selfKnockbackMultiplier = 1.0f;
    float gravity = 0.0f;
    float drag = 0.0f;
    float restitution = 0.0f;
    float friction = 0.0f;
    float armingDistance = 0.0f;
    float armingTime = 0.0f;
    float minBounceSpeed = 0.0f;
    float angularDrag = 0.0f;
    float distanceTraveled = 0.0f;
    float stateAccumulator = 0.0f;
    int bounceCount = 0;
    int maxBounceCount = 0;
    bool exploded = false;
    bool worldTouched = false;
    bool explodeOnPlayerImpact = true;
    bool explodeOnWorldImpact = false;
    bool explodeOnLifetime = true;
    uint32_t spawnTick = 0;
};

struct ServerProjectilePerfStats
{
    uint64_t projectileSimUs = 0;
    uint64_t triangleQueryCount = 0;
    uint64_t triangleCandidateTotal = 0;
    uint32_t triangleCandidateMax = 0;
    uint64_t playerCapsuleCandidateTotal = 0;
    uint32_t playerCapsuleCandidateMax = 0;
    uint64_t correctionPackets = 0;
    uint64_t correctionBytes = 0;
    uint32_t activeProjectiles = 0;
    uint32_t movingProjectiles = 0;
    uint32_t sleepingProjectiles = 0;
};

ServerProjectilePerfStats consumeServerProjectilePerfStats();

// Timestamp
const char* serverTimestamp();

// Address comparison
bool sameAddress(const sockaddr_in& a, const sockaddr_in& b);

// Name utilities
void copyName(char (&dst)[MAX_NAME_BYTES], const std::string& name);
std::string uniquePlayerName(
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::string& requested,
    uint32_t ownId);

// World loading
bool loadHeadlessWorld(const char* path, HeadlessWorld& world);

// Broadphase: gather candidate triangle indices intersecting an AABB
// Uses HeadlessWorld's uniform spatial grid. Shared by player and projectile collision.
void gatherHeadlessTrianglesForAABB(
    const HeadlessWorld& world,
    const AABB& queryBounds,
    float expansion,
    std::vector<int>& out);

// Geometry
glm::vec3 closestPointTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c);

// Collision
void resolveWorldCollision(ServerPlayer& p, const HeadlessWorld& world);
void resolvePlayerCollision(std::unordered_map<uint32_t, ServerPlayer>& players);

// Player simulation
void resetPlayerForSpawn(ServerPlayer& player, bool isInitialSpawn);
void completeAuthoritativeSpawn(SOCKET sock, ServerPlayer& player, bool isInitialSpawn);
void retrySpawnSync(SOCKET sock, ServerPlayer& player);
void tickWeaponRuntimes(std::unordered_map<uint32_t, ServerPlayer>& players, uint32_t currentTick);
void handleSpawnAck(SOCKET sock, const char* buffer, int bytes,
                     std::unordered_map<uint32_t, ServerPlayer>& players,
                     uint32_t tick);
void handleReloadRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                          std::unordered_map<uint32_t, ServerPlayer>& players,
                          uint32_t tick, uint64_t& totalPacketsOut);
void simulatePlayer(ServerPlayer& p, const HeadlessWorld& world);
void pushPositionHistory(ServerPlayer& p, uint32_t tick);
bool getPositionAtTick(const ServerPlayer& p, uint32_t targetTick, glm::vec3& outPos);
SnapshotEntity makePlayerEntity(const ServerPlayer& player);

// NPC simulation
void simulateNpc(ServerNpc& npc, const std::unordered_map<uint32_t, ServerPlayer>& players);
SnapshotEntity makeNpcEntity(const ServerNpc& npc);

// Raycast
bool serverRayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                       const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                       float& outDist);
bool serverRaycastWorld(const glm::vec3& origin, const glm::vec3& direction,
                         float maxDist, const HeadlessWorld& world,
                         glm::vec3& outHitPos, glm::vec3& outNormal);
const char* transportKindName(TransportKind kind);
TransportConnectionId makeUdpConnectionId(const sockaddr_in& endpoint);
TransportConnectionId allocateIceConnectionId();
sockaddr_in legacyEndpointForTransportConnection(TransportConnectionId id);
bool playerOwnsConnectionSource(const ServerPlayer& player,
                                const sockaddr_in* endpoint,
                                const TransportConnectionId* connectionId);

// Packet handlers
void handleHello(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                 std::unordered_map<uint32_t, ServerPlayer>& players,
                 uint32_t& nextPlayerId, uint32_t tick, uint64_t& totalPacketsOut,
                 const HeadlessWorld* world = nullptr,
                 const TransportConnectionId* connectionId = nullptr,
                 std::unique_ptr<IGameTransport>* claimedTransport = nullptr);
void handleInputPacket(const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       const HeadlessWorld& world,
                       uint32_t& nextEntityId,
                       std::unordered_map<uint32_t, ServerNpc>& npcs,
                       const sockaddr_in* from = nullptr,
                       uint32_t serverTick = 0,
                       const TransportConnectionId* connectionId = nullptr);
void handleDisconnect(std::unordered_map<uint32_t, ServerPlayer>& players,
                      const char* buffer);
void handleSpawnNpcRequest(const char* buffer, int bytes,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           uint32_t& nextEntityId,
                           std::unordered_map<uint32_t, ServerNpc>& npcs);
void handleTeleportRequest(const char* buffer, int bytes,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           const HeadlessWorld& world);
void handleExplodeRequest(const char* buffer, int bytes,
                          std::unordered_map<uint32_t, ServerPlayer>& players);
// ── Disagreement retransmission (defined before use in shot/send helpers) ─
constexpr int DISAGREEMENT_RETRANSMIT_MAX = 8;
constexpr int DISAGREEMENT_RETRANSMIT_TICKS = 4;

struct PendingDisagreement
{
    DisagreementPacket packet;
    uint8_t retransmitsLeft = 0;
    bool active = false;
};

struct DisagreementRetransmitState
{
    PendingDisagreement events[DISAGREEMENT_RETRANSMIT_MAX];
    uint32_t nextEventId = 1;
};

void handleShotRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       const HeadlessWorld& world,
                       uint32_t tick, uint64_t& totalPacketsOut,
                       DisagreementRetransmitState* retransmitState = nullptr);
void handleAttackRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                         uint32_t& nextProjectileId,
                         const HeadlessWorld& world,
                         uint32_t tick, uint64_t& totalPacketsOut);
void handleProjectileFireRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                                 std::unordered_map<uint32_t, ServerPlayer>& players,
                                 std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                                 uint32_t& nextProjectileId,
                                 const HeadlessWorld& world,
                                 uint32_t tick, uint64_t& totalPacketsOut);
void tickServerProjectiles(SOCKET sock,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                           const HeadlessWorld& world,
                           float dt, uint32_t tick, uint64_t& totalPacketsOut);
void handleMeleeHitRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           uint32_t tick, uint64_t& totalPacketsOut);
void tickServerSwordCombat(SOCKET sock,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           const HeadlessWorld& world,
                           float dt, uint32_t tick, uint64_t& totalPacketsOut);
bool serverSendToPlayer(SOCKET sock, const ServerPlayer& player,
                         const void* data, size_t size);
void handlePelletBlastRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                               std::unordered_map<uint32_t, ServerPlayer>& players,
                               const HeadlessWorld& world,
                               uint32_t tick, uint64_t& totalPacketsOut,
                                                               DisagreementRetransmitState* retransmitState = nullptr);
void handleGodballState(SOCKET sock,
                        std::unordered_map<uint32_t, ServerPlayer>& players,
                        char* buffer, int bytes);
void handleChatMessage(SOCKET sock, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       uint32_t tick, uint64_t& totalPacketsOut);
void handlePing(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                uint32_t tick, const ServerPlayer* authenticatedPlayer = nullptr);
void handleNpcDamageRequest(SOCKET sock, const char* buffer, int bytes,
                            const sockaddr_in& from,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            std::unordered_map<uint32_t, ServerNpc>& npcs,
                            uint32_t tick, uint64_t& totalPacketsOut);
void handleServerCommand(const char* buffer, int bytes,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs);

// ── Migration: join/reconnect packet handlers ────────────────────────
void handleJoinRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       uint32_t& nextPlayerId, uint32_t tick, uint64_t& totalPacketsOut,
                       const HeadlessWorld* world = nullptr,
                       const TransportConnectionId* connectionId = nullptr,
                       std::unique_ptr<IGameTransport>* claimedTransport = nullptr);
void handleReconnectRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            uint32_t tick, uint64_t& totalPacketsOut,
                            const TransportConnectionId* connectionId = nullptr,
                            std::unique_ptr<IGameTransport>* claimedTransport = nullptr);

ServerPacketProcessResult processServerPacket(
    SOCKET sock,
    const TransportReceiveEvent& event,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    std::unordered_map<uint32_t, ServerProjectile>& projectiles,
    uint32_t& nextPlayerId,
    uint32_t& nextEntityId,
    uint32_t& nextProjectileId,
    const HeadlessWorld& world,
    uint32_t tick,
    uint64_t& totalPacketsIn,
    uint64_t& totalPacketsOut,
    ServerPacketStats* stats = nullptr,
    DisagreementRetransmitState* retransmitState = nullptr,
    ServerPlayer* authenticatedPlayer = nullptr,
    std::unique_ptr<IGameTransport>* claimedTransport = nullptr);

// Generate a secure reconnect token
std::string generateReconnectToken();

// Coordinator state for join token validation
void setServerCoordinatorState(const std::string& code, const std::string& joinToken);
const std::string& getServerCoordinatorCode();
const std::string& getServerCoordinatorJoinToken();

// Server map identity
void setServerMapId(const std::string& mapId);
const std::string& getServerMapId();

// Authoritative transform helper
void beginAuthoritativeTransform(ServerPlayer& player,
    const glm::vec3& position, const glm::vec3& velocity, float yaw,
    const char* reason);

// Post-tick helpers
void handleClientTimeout(std::unordered_map<uint32_t, ServerPlayer>& players);
void checkVoidDeath(std::unordered_map<uint32_t, ServerPlayer>& players,
                    std::unordered_map<uint32_t, ServerNpc>& npcs);
void buildAndSendSnapshot(SOCKET sock,
                          const std::unordered_map<uint32_t, ServerPlayer>& players,
                          const std::unordered_map<uint32_t, ServerNpc>& npcs,
                          uint32_t tick, uint64_t& totalPacketsOut);

void logSnapshotEntity(const SnapshotEntity& entity);

// Send a disagreement event to all connected players.
// If retransmitState is non-null, queues the packet for best-effort retransmission.
void sendDisagreementToAll(SOCKET sock,
                           const std::unordered_map<uint32_t, ServerPlayer>& players,
                           DisagreementReason reason,
                           uint32_t eventId,
                           uint32_t relatedSerial,
                           uint32_t sourcePlayerId,
                           uint32_t targetPlayerId,
                           glm::vec3 position,
                           glm::vec3 correction,
                           const char* description,
                           uint32_t tick,
                           uint64_t& totalPacketsOut,
                           DisagreementRetransmitState* retransmitState = nullptr);

void tickDisagreementRetransmit(SOCKET sock,
                                const std::unordered_map<uint32_t, ServerPlayer>& players,
                                DisagreementRetransmitState& state,
                                uint64_t& totalPacketsOut);

ServerDamageResult applyServerDamage(std::unordered_map<uint32_t, ServerPlayer>& players,
                                     ServerPlayer& target,
                                     uint32_t attackerPlayerId,
                                     int damage,
                                     const glm::vec3& knockback,
                                     ServerDamageSource source);
ReliableGameplayEventQueueResult queueServerDamageConfirmedEvent(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t tick,
    uint64_t& totalPacketsOut,
    uint32_t attackerPlayerId,
    const ServerPlayer& target,
    int damage,
    const ServerDamageResult& result,
    const glm::vec3& hit,
    const glm::vec3& normal,
    const glm::vec3& knockback,
    ServerDamageSource source,
    uint8_t weapon,
    uint32_t causeSerial = 0,
    uint32_t projectileId = 0);

// ─── HostedRoomSession — single canonical room for one host action ─────────

struct HostedRoomSession
{
    bool active = false;
    std::string roomCode;
    std::string hostToken;
    std::string joinToken;
    uint64_t serverProcessId = 0;
    std::string serverEndpoint;
    std::string iceHostSessionId;
    std::string coordinatorRoomType; // "normal" or "ice"
    uint64_t createdAtMs = 0;
    uint64_t lastHeartbeatMs = 0;
};

inline HostedRoomSession& hostedRoomSession()
{
    static HostedRoomSession session;
    return session;
}

// ─── Server Launch Settings (shared by UI, process launch, headless) ──────

struct ServerLaunchSettings
{
    std::string serverName = "MiMITA Server";
    std::string mapName = "funworldv3";
    std::string gameMode = "sandbox";
    uint32_t maxPlayers = 999;
    bool passwordProtected = false;
    std::string password;
    bool startupNpcsEnabled = true;
    uint32_t startupNpcCount = 3;
    uint16_t port = DEFAULT_PORT;
    std::string serverCode;
    bool externalProcessLaunched = false;
    bool iceEnabled = false;
    std::string turnPassword;

    // Resolved state (set during startup, not from UI)
    std::string resolvedMapPath;
};

// ─── Listen Server (host runs server in same process) ──────────────────────

struct ListenServerState
{
    bool active = false;
    SOCKET sock = INVALID_SOCKET;
    std::unordered_map<uint32_t, ServerPlayer> players;
    std::unordered_map<uint32_t, ServerNpc> npcs;
    std::unordered_map<uint32_t, ServerProjectile> projectiles;
    HeadlessWorld world;
    uint32_t nextPlayerId = 1;
    uint32_t nextEntityId = 1000;
    uint32_t nextProjectileId = 1;
    uint32_t tick = 0;
    uint64_t lastLog = 0;
    uint64_t totalPacketsIn = 0;
    uint64_t totalPacketsOut = 0;
    uint64_t startTimeMs = 0;
    uint16_t port = DEFAULT_PORT;
    std::string serverCode;
    std::string joinToken;
    std::string serverName = "MiMITA Server";
    // ── Background thread for 60 Hz independent server timing ──────
    std::atomic<bool> serverRunning{false};
    std::thread serverThread;

    // ── Legacy timing (kept for compatibility) ─────────────────────
    float accumulator = 0.0f;
    uint64_t lastHeartbeatMs = 0;
    std::string publicIp;
    std::string hostSessionId;
    DisagreementRetransmitState disagreementRetransmit;

    // ── ICE server support ─────────────────────────────────────────────
    bool iceEnabled = false;
    std::unique_ptr<class IceAgent> iceListenerAgent;
    std::string iceSessionId;
    uint64_t lastIceCoordinatorPollMs = 0;
    // Pending ICE transports: agents created for new clients but not yet
    // associated with a player (waiting for first Hello packet).
    std::vector<PendingServerTransport> pendingIceTransports;
};

bool startListenServer(ListenServerState& state, uint16_t port,
    const std::string& publicIp = "", const std::string& hostSessionId = "",
    const ServerLaunchSettings* settings = nullptr);
void stopListenServer(ListenServerState& state);
void tickListenServer(ListenServerState& state, float dt);
// ── Non-blocking ICE peer handshake state ────────────────────────────
struct PendingIcePeer {
    std::string requestId;
    std::string clientSessionId;
    std::string clientIceDescription;
    std::unique_ptr<class IceAgent> agent;
    enum class State { Idle, Gathering, WaitingAnswer, Connecting, Connected, Failed };
    State state = State::Idle;
    uint64_t startedAtMs = 0;
    uint64_t lastEventMs = 0;
    int failCount = 0;
};

extern std::vector<std::unique_ptr<PendingIcePeer>> gPendingIcePeers;

bool initServerIceListener(ListenServerState& state);
void tickIceCoordinator(ListenServerState& state);
void tickIcePeers(const std::string& serverCode, const std::string& iceSessionId,
                  std::vector<PendingServerTransport>& pendingIceTransports);
bool waitForAgentState(class IceAgent& agent, IceAgentState target, int timeoutMs);
void tickServerIceTransports(SOCKET sock,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             std::unordered_map<uint32_t, ServerNpc>& npcs,
                             std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                             uint32_t& nextEntityId,
                             uint32_t& nextProjectileId,
                             uint32_t& nextPlayerId,
                             std::vector<PendingServerTransport>& pendingIceTransports,
                             const HeadlessWorld& world,
                             uint32_t tick,
                             uint64_t& totalPacketsIn,
                             uint64_t& totalPacketsOut,
                             ServerPacketStats* stats = nullptr,
                             DisagreementRetransmitState* retransmitState = nullptr);
bool serverSendToPlayer(SOCKET sock, const ServerPlayer& player, const void* data, size_t size);
uint32_t serverReliableEventSessionId();
uint32_t nextReliableGameplayEventId();
uint32_t reliableGameplayEventSessionForPlayer(ServerPlayer& player);
ReliableGameplayEventQueueResult queueReliableGameplayEventToAll(SOCKET sock,
                                                                 std::unordered_map<uint32_t, ServerPlayer>& players,
                                                                 const void* data,
                                                                 size_t size,
                                                                 uint32_t eventId,
                                                                 uint32_t eventSessionId,
                                                                 uint64_t& totalPacketsOut);
void handleReliableEventAck(const char* buffer, int bytes,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            const sockaddr_in* from = nullptr,
                            const ServerPlayer* authenticatedPlayer = nullptr);
void tickReliableGameplayEvents(SOCKET sock,
                                std::unordered_map<uint32_t, ServerPlayer>& players,
                                uint64_t& totalPacketsOut);
void setReliableGameplayEventTestNowMs(uint64_t nowMsOverride);
std::string generateServerCode();

} // namespace MimitaNet

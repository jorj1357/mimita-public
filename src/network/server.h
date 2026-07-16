#pragma once

#include "network/net_common.h"
#include "network/packets.h"
#include "physics/physics-types.h"
#include "physics/config.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <limits>
#include <string>
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

constexpr float SERVER_TICK_RATE = 60.0f;
constexpr float SERVER_DT = 1.0f / SERVER_TICK_RATE;
constexpr float PLAYER_RADIUS = 0.65f;
constexpr float PLAYER_HEIGHT = 3.5f;

struct ServerSpawnPoint
{
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
};

struct HeadlessWorld
{
    std::vector<CollisionTriangle> triangles;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    std::vector<ServerSpawnPoint> spawnPoints;
};

struct ServerInput
{
    glm::vec2 wish{0.0f};
    glm::vec3 camForward{1.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    bool jumpHeld = false;
    bool dashPressed = false;
    bool attackPressed = false;
    bool freezeHeld = false;
    uint32_t tick = 0;
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
    uint32_t lastMeleeAttackSerial = 0;
    float projectileFireCooldown = 0.0f;
    uint16_t lastDashSerial = 0;
    uint16_t lastJumpSerial = 0;
    uint16_t lastDownDashSerial = 0;
    uint16_t lastEquipSerial = 0;
    uint16_t inputStateFlags = 0;
    float dashCooldownTimer = 0.0f;
    float sizeScale = 1.0f;
    ServerInput input;
    std::deque<PositionHistoryEntry> posHistory;
    // ── Migration: auth + reconnect ───────────────────────────────────
    std::string joinToken;
    std::string reconnectToken;
    bool joinTokenValidated = false;
    bool spawned = false;
    int kills = 0;
    int deaths = 0;
    uint16_t transformEpoch = 0;
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
    float distanceTraveled = 0.0f;
    float stateAccumulator = 0.0f;
    int bounceCount = 0;
    int maxBounceCount = 0;
    bool exploded = false;
    bool worldTouched = false;
    uint32_t spawnTick = 0;
};

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

// Geometry
glm::vec3 closestPointTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c);

// Collision
void resolveWorldCollision(ServerPlayer& p, const HeadlessWorld& world);
void resolvePlayerCollision(std::unordered_map<uint32_t, ServerPlayer>& players);

// Player simulation
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

// Packet handlers
void handleHello(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                 std::unordered_map<uint32_t, ServerPlayer>& players,
                 uint32_t& nextPlayerId, uint32_t tick, uint64_t& totalPacketsOut,
                 const HeadlessWorld* world = nullptr);
void handleInputPacket(const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       const HeadlessWorld& world,
                       uint32_t& nextEntityId,
                       std::unordered_map<uint32_t, ServerNpc>& npcs);
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
void handleShotRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
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
void handleChatMessage(SOCKET sock, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       uint32_t tick, uint64_t& totalPacketsOut);
void handlePing(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                uint32_t tick);
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
                       const HeadlessWorld* world = nullptr);
void handleReconnectRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            uint32_t tick, uint64_t& totalPacketsOut);

// Generate a secure reconnect token
std::string generateReconnectToken();

// Coordinator state for join token validation
void setServerCoordinatorState(const std::string& code, const std::string& joinToken);
const std::string& getServerCoordinatorCode();
const std::string& getServerCoordinatorJoinToken();

// Server map identity
void setServerMapId(const std::string& mapId);
const std::string& getServerMapId();

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
void sendDisagreementToAll(SOCKET sock,
                           const std::unordered_map<uint32_t, ServerPlayer>& players,
                           DisagreementReason reason,
                           glm::vec3 position,
                           glm::vec3 correction,
                           const char* description,
                           uint32_t tick,
                           uint64_t& totalPacketsOut);

ServerDamageResult applyServerDamage(std::unordered_map<uint32_t, ServerPlayer>& players,
                                     ServerPlayer& target,
                                     uint32_t attackerPlayerId,
                                     int damage,
                                     const glm::vec3& knockback,
                                     ServerDamageSource source);

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
    float accumulator = 0.0f;
    uint64_t lastHeartbeatMs = 0;
    std::string publicIp;
    std::string hostSessionId;
};

bool startListenServer(ListenServerState& state, uint16_t port,
    const std::string& publicIp = "", const std::string& hostSessionId = "",
    const ServerLaunchSettings* settings = nullptr);
void stopListenServer(ListenServerState& state);
void tickListenServer(ListenServerState& state, float dt);
std::string generateServerCode();

} // namespace MimitaNet

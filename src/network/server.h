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

struct HeadlessWorld
{
    std::vector<CollisionTriangle> triangles;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
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
    uint16_t lastDashSerial = 0;
    float sizeScale = 1.0f;
    ServerInput input;
    std::deque<PositionHistoryEntry> posHistory;
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
                 uint32_t& nextPlayerId, uint32_t tick, uint64_t& totalPacketsOut);
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

// Post-tick helpers
void handleClientTimeout(std::unordered_map<uint32_t, ServerPlayer>& players);
void checkVoidDeath(std::unordered_map<uint32_t, ServerPlayer>& players,
                    std::unordered_map<uint32_t, ServerNpc>& npcs);
void buildAndSendSnapshot(SOCKET sock,
                          const std::unordered_map<uint32_t, ServerPlayer>& players,
                          const std::unordered_map<uint32_t, ServerNpc>& npcs,
                          uint32_t tick, uint64_t& totalPacketsOut);

void logSnapshotEntity(const SnapshotEntity& entity);

} // namespace MimitaNet

#include "network/server.h"
#include "network/multiplayer-context.h"
#include "network/coordinator-client.h"
#include "void-death/void-death.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <limits>
#include <random>

namespace MimitaNet {

// ── Global server code for coordinator validation ────────────────────
// Set by the server when it registers with the coordinator.
static std::string gServerCoordinatorCode;
static std::string gServerCoordinatorJoinToken;

// ── Global server map ID ─────────────────────────────────────────────
static std::string gServerMapId = "funworldv3";

void setServerCoordinatorState(const std::string& code, const std::string& joinToken)
{
    gServerCoordinatorCode = code;
    gServerCoordinatorJoinToken = joinToken;
}

const std::string& getServerCoordinatorCode() { return gServerCoordinatorCode; }
const std::string& getServerCoordinatorJoinToken() { return gServerCoordinatorJoinToken; }

void setServerMapId(const std::string& mapId) { gServerMapId = mapId; }
const std::string& getServerMapId() { return gServerMapId; }

bool sameAddress(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

bool serverRayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                       const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                       float& outDist)
{
    glm::vec3 e1 = b - a;
    glm::vec3 e2 = c - a;
    glm::vec3 p = glm::cross(direction, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < 0.000001f) return false;
    float inv = 1.0f / det;
    glm::vec3 t = origin - a;
    float u = glm::dot(t, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(t, e1);
    float v = glm::dot(direction, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    outDist = glm::dot(e2, q) * inv;
    return outDist > 0.0f;
}

bool serverRaycastWorld(const glm::vec3& origin, const glm::vec3& direction,
                         float maxDist, const HeadlessWorld& world,
                         glm::vec3& outHitPos, glm::vec3& outNormal)
{
    float closest = maxDist;
    bool hit = false;
    for (const CollisionTriangle& tri : world.triangles)
    {
        float d;
        if (serverRayTriangle(origin, direction, tri.a, tri.b, tri.c, d) && d < closest)
        {
            closest = d;
            outNormal = tri.normal;
            hit = true;
        }
    }
    if (hit)
        outHitPos = origin + direction * closest;
    return hit;
}

void logSnapshotEntity(const SnapshotEntity& entity)
{
    printf("  entityId=%u type=%s ownerClientId=%u position=(%.2f,%.2f,%.2f) "
           "rotation=%.2f health=%d\n",
           entity.networkEntityId,
           entity.entityType == ENTITY_PLAYER ? "Player" : "NPC",
           entity.ownerClientId,
           entity.px, entity.py, entity.pz,
           entity.yaw, entity.health);
}

void handleHello(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                 std::unordered_map<uint32_t, ServerPlayer>& players,
                 uint32_t& nextPlayerId, uint32_t tick, uint64_t& totalPacketsOut,
                 const HeadlessWorld* world)
{
    if (bytes < (int)sizeof(HelloPacket))
        return;
    uint32_t existingId = 0;
    for (const auto& kv : players)
        if (sameAddress(kv.second.addr, from))
            existingId = kv.first;

    uint32_t id = existingId ? existingId : nextPlayerId++;
    ServerPlayer& p = players[id];
    p.id = id;
    p.addr = from;
    p.lastHeardMs = nowMs();
    p.lastShotSerial = 0;
    p.name = uniquePlayerName(
        players, reinterpret_cast<const HelloPacket*>(buffer)->name, id);

    if (!existingId)
    {
        // Use map spawnpoints if available
        if (world && !world->spawnPoints.empty())
        {
            size_t idx = (id - 1) % world->spawnPoints.size();
            p.pos = world->spawnPoints[idx].position;
            p.yaw = world->spawnPoints[idx].yaw;
            printf("%s [SERVER PLAYER SPAWN] reason=initial_join id=%u name=\"%s\" "
                   "spawnpoint=%zu position=(%.2f,%.2f,%.2f) yaw=%.1f\n",
                   serverTimestamp(), id, p.name.c_str(), idx,
                   p.pos.x, p.pos.y, p.pos.z, glm::degrees(p.yaw));
        }
        else
        {
            p.pos = {1.0f + (float)(id - 1) * 1.5f, 5.0f, 30.0f};
            printf("%s [SERVER JOIN] id=%u name=\"%s\" addr=%s spawn=(%.1f,%.1f,%.1f) "
                   "(no spawnpoints in map)\n",
                   serverTimestamp(), id, p.name.c_str(), addressToString(from).c_str(),
                   p.pos.x, p.pos.y, p.pos.z);
        }
        p.spawned = true;
    }

    p.reconnectToken = generateReconnectToken();

    ++p.transformEpoch;
    WelcomePacket welcome{};
    welcome.header.type = PACKET_WELCOME;
    welcome.header.tick = tick;
    welcome.header.playerId = id;
    welcome.header.transformEpoch = p.transformEpoch;
    welcome.assignedPlayerId = id;
    welcome.tickRate = SERVER_TICK_RATE;
    copyName(welcome.approvedName, p.name);
    std::memset(welcome.reconnectToken, 0, sizeof(welcome.reconnectToken));
    std::strncpy(welcome.reconnectToken, p.reconnectToken.c_str(), sizeof(welcome.reconnectToken) - 1);
    std::memset(welcome.mapId, 0, sizeof(welcome.mapId));
    std::strncpy(welcome.mapId, gServerMapId.c_str(), sizeof(welcome.mapId) - 1);
    sendto(sock, (const char*)&welcome, sizeof(welcome), 0, (sockaddr*)&from, sizeof(from));
    ++totalPacketsOut;
}

void handleInputPacket(const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       const HeadlessWorld& world,
                       uint32_t& nextEntityId,
                       std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (bytes < (int)sizeof(InputPacket))
        return;
    const InputPacket* in = reinterpret_cast<const InputPacket*>(buffer);
    auto it = players.find(in->header.playerId);
    if (it == players.end())
        return;
    ServerPlayer& p = it->second;
    p.lastHeardMs = nowMs();

    // ── Transform epoch check ─────────────────────────────────────
    // Discard packets from before the player's most recent spawn/respawn.
    if (in->header.transformEpoch != 0 && in->header.transformEpoch < p.transformEpoch)
    {
        static uint64_t lastEpochLogMs = 0;
        uint64_t now = nowMs();
        if (now - lastEpochLogMs >= 1000)
        {
            printf("%s [SERVER INPUT DROP] reason=OLD_TRANSFORM_EPOCH playerId=%u "
                   "packetEpoch=%u currentEpoch=%u\n",
                   serverTimestamp(), p.id, in->header.transformEpoch, p.transformEpoch);
            lastEpochLogMs = now;
        }
        return;
    }

    if (p.dead)
    {
        p.input.attackPressed = false;
        return;
    }
    p.input.wish = {in->wishX, in->wishY};
    p.input.camForward = {in->camForwardX, in->camForwardY, in->camForwardZ};
    p.input.yaw = in->yaw;
    p.input.jumpHeld = in->jumpHeld != 0;
    p.input.dashPressed = in->dashPressed != 0;
    const bool attackPressed = in->attackPressed != 0;
    if (attackPressed && !p.input.attackPressed)
        p.attackQueued = true;
    p.input.attackPressed = attackPressed;
    p.input.freezeHeld = in->freezeHeld != 0;
    p.input.tick = in->header.tick;
    p.equippedSlot = in->equippedSlot;
    p.weaponState = in->weaponState;
    p.pingMs = std::clamp(in->clientPingMs, 0, 9999);
    p.sizeScale = std::max(in->sizeScale, 0.001f);

    const glm::vec3 reportedPosition{
        in->clientPx, in->clientPy, in->clientPz};
    const glm::vec3 reportedVelocity{
        in->clientVx, in->clientVy, in->clientVz};
    constexpr float MAX_CLIENT_STATE_DELTA = 30.0f;
    constexpr float MAX_CLIENT_REPORTED_SPEED = 180.0f;
    const bool finiteState =
        std::isfinite(reportedPosition.x) &&
        std::isfinite(reportedPosition.y) &&
        std::isfinite(reportedPosition.z) &&
        std::isfinite(reportedVelocity.x) &&
        std::isfinite(reportedVelocity.y) &&
        std::isfinite(reportedVelocity.z);
    const float stateDelta = finiteState
        ? glm::length(reportedPosition - p.pos)
        : std::numeric_limits<float>::infinity();
    const float reportedSpeed = finiteState
        ? glm::length(reportedVelocity)
        : std::numeric_limits<float>::infinity();
    if (finiteState &&
        stateDelta <= MAX_CLIENT_STATE_DELTA &&
        reportedSpeed <= MAX_CLIENT_REPORTED_SPEED)
    {
        p.pos = reportedPosition;
        p.vel = reportedVelocity;
        p.clientStateUpdated = true;
    }
    else
    {
        static uint64_t lastRejectedStateLogMs = 0;
        const uint64_t rejectNowMs = nowMs();
        if (rejectNowMs - lastRejectedStateLogMs >= 500)
        {
            printf("%s [SERVER MOVEMENT REJECT] playerId=%u "
                   "reason=position_delta serverPos=(%.2f,%.2f,%.2f) "
                   "clientPos=(%.2f,%.2f,%.2f) delta=%.2f speed=%.2f "
                   "serverMap=%s finite=%d\n",
                   serverTimestamp(), p.id,
                   p.pos.x, p.pos.y, p.pos.z,
                   reportedPosition.x, reportedPosition.y, reportedPosition.z,
                   stateDelta, reportedSpeed,
                   gServerMapId.c_str(), (int)finiteState);
            lastRejectedStateLogMs = rejectNowMs;
        }
    }
    if (in->spawnNpcPressed)
    {
        ServerNpc npc;
        npc.entityId = nextEntityId++;
        npc.name = "NPC " + std::to_string(npc.entityId);
        npc.pos = p.pos + glm::vec3(2.0f, 0.0f, 0.0f);
        npcs[npc.entityId] = npc;
        printf("%s [SERVER ENTITY SPAWN] entityId=%u type=NPC ownerClientId=0 position=(%.2f,%.2f,%.2f)\n",
               serverTimestamp(), npc.entityId, npc.pos.x, npc.pos.y, npc.pos.z);
    }
}

void handleDisconnect(std::unordered_map<uint32_t, ServerPlayer>& players,
                      const char* buffer)
{
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
    auto it = players.find(header->playerId);
    if (it != players.end())
    {
        printf("%s [SERVER LEAVE] id=%u name=\"%s\"\n",
               serverTimestamp(), it->second.id, it->second.name.c_str());
        players.erase(it);
    }
}

void handleSpawnNpcRequest(const char* buffer, int bytes,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           uint32_t& nextEntityId,
                           std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (bytes < (int)sizeof(SpawnNpcRequestPacket))
        return;
    const SpawnNpcRequestPacket* request =
        reinterpret_cast<const SpawnNpcRequestPacket*>(buffer);
    if (players.find(request->header.playerId) == players.end())
        return;

    ServerNpc npc;
    npc.entityId = nextEntityId++;
    npc.name = "NPC " + std::to_string(npc.entityId);
    npc.pos = {request->px, request->py, request->pz};
    npc.difficulty = request->difficulty;
    npcs[npc.entityId] = npc;
    printf("%s [SERVER ENTITY SPAWN] entityId=%u type=NPC ownerClientId=0 position=(%.2f,%.2f,%.2f) difficulty=%.1f\n",
           serverTimestamp(), npc.entityId, npc.pos.x, npc.pos.y, npc.pos.z, npc.difficulty);
}

void handleTeleportRequest(const char* buffer, int bytes,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           const HeadlessWorld& world)
{
    if (bytes < (int)sizeof(TeleportRequestPacket))
        return;
    const TeleportRequestPacket* request =
        reinterpret_cast<const TeleportRequestPacket*>(buffer);
    auto it = players.find(request->header.playerId);
    if (it == players.end() || it->second.dead)
        return;

    const glm::vec3 requestedPosition{
        request->px, request->py, request->pz};
    if (!std::isfinite(requestedPosition.x) ||
        !std::isfinite(requestedPosition.y) ||
        !std::isfinite(requestedPosition.z))
    {
        printf("%s [SERVER TELEPORT] playerId=%u rejected=non-finite\n",
               serverTimestamp(), request->header.playerId);
        return;
    }

    ServerPlayer& p = it->second;
    p.pos = glm::clamp(
        requestedPosition,
        world.boundsMin - glm::vec3(2.0f),
        world.boundsMax + glm::vec3(2.0f));
    p.vel = glm::vec3(0.0f);
    p.onGround = false;
    ++p.transformEpoch;
    printf("%s [SERVER TELEPORT] playerId=%u position=(%.2f,%.2f,%.2f) epoch=%u\n",
           serverTimestamp(), p.id, p.pos.x, p.pos.y, p.pos.z, (unsigned)p.transformEpoch);
}

void handleExplodeRequest(const char* buffer, int bytes,
                          std::unordered_map<uint32_t, ServerPlayer>& players)
{
    (void)bytes;
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(buffer);
    auto it = players.find(header->playerId);
    if (it == players.end() || it->second.dead)
        return;

    ServerPlayer& p = it->second;
    p.health = 0;
    p.dead = true;
    p.respawnSeconds = 2.0f;
    p.vel = glm::vec3(0.0f);
    printf("%s [SERVER DEATH] playerId=%u cause=explode respawn=2.0s\n",
           serverTimestamp(), p.id);
}

void handleJoinRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       uint32_t& nextPlayerId, uint32_t tick, uint64_t& totalPacketsOut,
                       const HeadlessWorld* world)
{
    if (bytes < (int)sizeof(JoinRequestPacket))
        return;
    const JoinRequestPacket* join = reinterpret_cast<const JoinRequestPacket*>(buffer);

    // Check if server is full
    if (players.size() >= MAX_PLAYERS)
    {
        JoinRejectPacket reject{};
        reject.header.type = PACKET_JOIN_REJECT;
        reject.header.tick = tick;
        reject.reason = 1; // full
        sendto(sock, (const char*)&reject, sizeof(reject), 0, (sockaddr*)&from, sizeof(from));
        ++totalPacketsOut;
        printf("%s [SERVER JOIN REJECT] reason=server-full\n", serverTimestamp());
        return;
    }

    // Validate join token — coordinator validation
    const std::string joinTokenStr = join->joinToken;
    if (joinTokenStr.empty())
    {
        JoinRejectPacket reject{};
        reject.header.type = PACKET_JOIN_REJECT;
        reject.header.tick = tick;
        reject.reason = 2; // bad token
        sendto(sock, (const char*)&reject, sizeof(reject), 0, (sockaddr*)&from, sizeof(from));
        ++totalPacketsOut;
        printf("%s [SERVER JOIN REJECT] reason=empty-token\n", serverTimestamp());
        return;
    }

    // If registered with coordinator, validate the join token
    if (!gServerCoordinatorCode.empty())
    {
        printf("[ROOM TOKEN VALIDATE] api=coordinatorValidateJoin code=%s tokenPrefix=%s\n",
               gServerCoordinatorCode.c_str(), joinTokenStr.substr(0, 12).c_str());
        if (!coordinatorValidateJoin(gServerCoordinatorCode, joinTokenStr))
        {
            printf("[ROOM TOKEN VALIDATE] api=coordinatorValidateJoin code=%s tokenPrefix=%s valid=0\n",
                   gServerCoordinatorCode.c_str(), joinTokenStr.substr(0, 12).c_str());
            // Fallback: check against stored join token from registration
            if (joinTokenStr == gServerCoordinatorJoinToken)
            {
                printf("[ROOM TOKEN VALIDATE] fallback local token match code=%s\n",
                       gServerCoordinatorCode.c_str());
            }
            else
            {
                JoinRejectPacket reject{};
                reject.header.type = PACKET_JOIN_REJECT;
                reject.header.tick = tick;
                reject.reason = 2; // bad token
                sendto(sock, (const char*)&reject, sizeof(reject), 0, (sockaddr*)&from, sizeof(from));
                ++totalPacketsOut;
                printf("%s [SERVER JOIN REJECT] reason=coordinator-rejected-token\n", serverTimestamp());
                return;
            }
        }
        printf("[ROOM TOKEN VALIDATE] api=coordinatorValidateJoin code=%s tokenPrefix=%s valid=1\n",
               gServerCoordinatorCode.c_str(), joinTokenStr.substr(0, 12).c_str());
        printf("%s [SERVER JOIN] coordinator validated token for %s\n",
               serverTimestamp(), join->name);
    }
    else
    {
        printf("%s [SERVER JOIN] no coordinator — accepting %s without validation\n",
               serverTimestamp(), join->name);
    }

    // Check for existing reconnection
    uint32_t existingId = 0;
    for (auto& kv : players)
    {
        if (kv.second.reconnectToken == join->joinToken && !kv.second.dead)
        {
            existingId = kv.first;
            break;
        }
    }

    uint32_t id = existingId ? existingId : nextPlayerId++;
    ServerPlayer& p = players[id];
    p.id = id;
    p.addr = from;
    p.lastHeardMs = nowMs();
    p.lastShotSerial = 0;
    p.joinToken = join->joinToken;
    p.joinTokenValidated = true;
    p.reconnectToken = generateReconnectToken();
    p.name = uniquePlayerName(players, join->name, id);

    if (!existingId)
    {
        // Use map spawnpoints if available
        if (world && !world->spawnPoints.empty())
        {
            size_t idx = (id - 1) % world->spawnPoints.size();
            p.pos = world->spawnPoints[idx].position;
            p.yaw = world->spawnPoints[idx].yaw;
            printf("%s [SERVER PLAYER SPAWN] reason=join_request id=%u name=\"%s\" "
                   "spawnpoint=%zu position=(%.2f,%.2f,%.2f) yaw=%.1f token=%s\n",
                   serverTimestamp(), id, p.name.c_str(), idx,
                   p.pos.x, p.pos.y, p.pos.z, glm::degrees(p.yaw),
                   p.reconnectToken.c_str());
        }
        else
        {
            p.pos = {1.0f + (float)(id - 1) * 1.5f, 5.0f, 30.0f};
            printf("%s [SERVER JOIN] id=%u name=\"%s\" addr=%s spawn=(%.1f,%.1f,%.1f) "
                   "(no spawnpoints) token=%s\n",
                   serverTimestamp(), id, p.name.c_str(), addressToString(from).c_str(),
                   p.pos.x, p.pos.y, p.pos.z, p.reconnectToken.c_str());
        }
        // Player registered but not yet spawned — waits for ClientMapReady.
        // Server will simulate and include in snapshots only after spawned=true.
        p.spawned = false;
    }
    else
    {
        printf("%s [SERVER REJOIN] id=%u name=\"%s\" addr=%s\n",
               serverTimestamp(), id, p.name.c_str(), addressToString(from).c_str());
        p.spawned = true;
    }

    ++p.transformEpoch;
    JoinAcceptPacket accept{};
    accept.header.type = PACKET_JOIN_ACCEPT;
    accept.header.tick = tick;
    accept.header.playerId = id;
    accept.header.transformEpoch = p.transformEpoch;
    accept.assignedPlayerId = id;
    accept.tickRate = SERVER_TICK_RATE;
    copyName(accept.approvedName, p.name);
    std::memset(accept.reconnectToken, 0, sizeof(accept.reconnectToken));
    std::strncpy(accept.reconnectToken, p.reconnectToken.c_str(), sizeof(accept.reconnectToken) - 1);
    std::memset(accept.mapId, 0, sizeof(accept.mapId));
    std::strncpy(accept.mapId, gServerMapId.c_str(), sizeof(accept.mapId) - 1);
    sendto(sock, (const char*)&accept, sizeof(accept), 0, (sockaddr*)&from, sizeof(from));
    ++totalPacketsOut;
}

void handleReconnectRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(ReconnectRequestPacket))
        return;
    const ReconnectRequestPacket* req = reinterpret_cast<const ReconnectRequestPacket*>(buffer);

    // Find player with matching reconnect token
    uint32_t foundId = 0;
    for (auto& kv : players)
    {
        if (kv.second.reconnectToken == req->reconnectToken)
        {
            foundId = kv.first;
            break;
        }
    }

    if (foundId == 0)
    {
        printf("%s [SERVER RECONNECT] rejected token=%s not-found\n",
               serverTimestamp(), req->reconnectToken);
        return;
    }

    ServerPlayer& p = players[foundId];
    p.addr = from;
    p.lastHeardMs = nowMs();
    p.reconnectToken = generateReconnectToken(); // rotate token

    ReconnectAcceptPacket accept{};
    accept.header.type = PACKET_RECONNECT_ACCEPT;
    accept.header.tick = tick;
    accept.header.playerId = foundId;
    accept.assignedPlayerId = foundId;
    accept.tickRate = SERVER_TICK_RATE;
    copyName(accept.approvedName, p.name);
    std::memset(accept.reconnectToken, 0, sizeof(accept.reconnectToken));
    std::strncpy(accept.reconnectToken, p.reconnectToken.c_str(), sizeof(accept.reconnectToken) - 1);
    accept.restoredHealth = p.health;
    accept.restoredKills = p.kills;
    accept.restoredDeaths = p.deaths;
    accept.restorePx = p.pos.x;
    accept.restorePy = p.pos.y;
    accept.restorePz = p.pos.z;
    sendto(sock, (const char*)&accept, sizeof(accept), 0, (sockaddr*)&from, sizeof(from));
    ++totalPacketsOut;

    printf("%s [SERVER RECONNECT] accepted id=%u name=\"%s\" health=%d\n",
           serverTimestamp(), foundId, p.name.c_str(), p.health);
}

std::string generateReconnectToken()
{
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static std::mt19937 rng((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, 61);
    std::string token;
    for (int i = 0; i < 24; ++i)
        token += chars[dist(rng)];
    return token;
}

void buildAndSendSnapshot(SOCKET sock,
                          const std::unordered_map<uint32_t, ServerPlayer>& players,
                          const std::unordered_map<uint32_t, ServerNpc>& npcs,
                          uint32_t tick, uint64_t& totalPacketsOut)
{
    SnapshotPacket snapshot{};
    snapshot.header.type = PACKET_SNAPSHOT;
    snapshot.header.tick = tick;
    uint32_t index = 0;
    for (const auto& kv : players)
    {
        if (index >= MAX_SNAPSHOT_ENTITIES)
            break;
        if (!kv.second.spawned)
        {
            printf("%s [SERVER SNAPSHOT SKIP] playerId=%u reason=not-spawned\n",
                   serverTimestamp(), kv.first);
            continue;
        }
        snapshot.entities[index++] = makePlayerEntity(kv.second);
        ++snapshot.playerCount;
    }
    for (const auto& kv : npcs)
    {
        if (index >= MAX_SNAPSHOT_ENTITIES)
            break;
        snapshot.entities[index++] = makeNpcEntity(kv.second);
        ++snapshot.npcCount;
    }
    snapshot.entityCount = index;

    if (tick % 60 == 0)
    {
        printf("%s [SERVER SNAPSHOT BUILD] tick=%u playersIncluded=%u npcsIncluded=%u entitiesIncluded=%u\n",
               serverTimestamp(), tick, snapshot.playerCount, snapshot.npcCount, snapshot.entityCount);
        for (uint32_t i = 0; i < snapshot.entityCount; ++i)
            logSnapshotEntity(snapshot.entities[i]);
    }

    for (const auto& kv : players)
    {
        int bytesSent = sendto(
            sock, (const char*)&snapshot, sizeof(snapshot), 0,
            (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
        if (bytesSent == SOCKET_ERROR)
            printf("%s [NET TX ERROR] sendto failed id=%u error=%d\n",
                   serverTimestamp(), kv.first, WSAGetLastError());
        ++totalPacketsOut;
        if (tick % 60 == 0)
            printf("%s [SERVER SNAPSHOT SEND] toClientId=%u bytes=%d entityCount=%u playerCount=%u npcCount=%u\n",
                   serverTimestamp(), kv.first, bytesSent, snapshot.entityCount,
                   snapshot.playerCount, snapshot.npcCount);
    }
}

// ── Send disagreement event to all connected players ─────────────────
void sendDisagreementToAll(SOCKET sock,
                           const std::unordered_map<uint32_t, ServerPlayer>& players,
                           DisagreementReason reason,
                           glm::vec3 position,
                           glm::vec3 correction,
                           const char* description,
                           uint32_t tick,
                           uint64_t& totalPacketsOut)
{
    DisagreementPacket packet{};
    packet.header.type = PACKET_DISAGREEMENT;
    packet.header.tick = tick;
    packet.reason = (uint8_t)reason;
    packet.posX = position.x;
    packet.posY = position.y;
    packet.posZ = position.z;
    packet.correctionX = correction.x;
    packet.correctionY = correction.y;
    packet.correctionZ = correction.z;
    std::memset(packet.description, 0, sizeof(packet.description));
    if (description)
        std::strncpy(packet.description, description, sizeof(packet.description) - 1);

    printf("%s [SERVER DISAGREEMENT SEND] reason=%u pos=(%.1f,%.1f,%.1f) "
           "correction=(%.1f,%.1f,%.1f) desc=\"%s\" players=%zu\n",
           serverTimestamp(), (unsigned)reason,
           position.x, position.y, position.z,
           correction.x, correction.y, correction.z,
           description ? description : "",
           players.size());

    for (const auto& kv : players)
    {
        int bytesSent = sendto(
            sock, (const char*)&packet, sizeof(packet), 0,
            (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
        if (bytesSent == SOCKET_ERROR)
            printf("%s [NET TX ERROR] sendDisagreementToAll failed id=%u error=%d\n",
                   serverTimestamp(), kv.first, WSAGetLastError());
        ++totalPacketsOut;
    }
}

} // namespace MimitaNet

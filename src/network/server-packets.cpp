#include "network/server.h"
#include "network/multiplayer-context.h"
#include "void-death/void-death.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

namespace MimitaNet {

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
                 uint32_t& nextPlayerId, uint32_t tick, uint64_t& totalPacketsOut)
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
        p.pos = {1.0f + (float)(id - 1) * 1.5f, 5.0f, 30.0f};
        printf("%s [SERVER JOIN] id=%u name=\"%s\" addr=%s\n",
               serverTimestamp(), id, p.name.c_str(), addressToString(from).c_str());
    }

    WelcomePacket welcome{};
    welcome.header.type = PACKET_WELCOME;
    welcome.header.tick = tick;
    welcome.header.playerId = id;
    welcome.assignedPlayerId = id;
    welcome.tickRate = SERVER_TICK_RATE;
    copyName(welcome.approvedName, p.name);
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
                   "distance=%.2f speed=%.2f finite=%d\n",
                   serverTimestamp(), p.id, stateDelta,
                   reportedSpeed, (int)finiteState);
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
    printf("%s [SERVER TELEPORT] playerId=%u position=(%.2f,%.2f,%.2f)\n",
           serverTimestamp(), p.id, p.pos.x, p.pos.y, p.pos.z);
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
        const int bytesSent = sendto(
            sock, (const char*)&snapshot, sizeof(snapshot), 0,
            (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
        ++totalPacketsOut;
        if (tick % 60 == 0)
            printf("%s [SERVER SNAPSHOT SEND] toClientId=%u bytes=%d entityCount=%u playerCount=%u npcCount=%u\n",
                   serverTimestamp(), kv.first, bytesSent, snapshot.entityCount,
                   snapshot.playerCount, snapshot.npcCount);
    }
}

} // namespace MimitaNet

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

void handleShotRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       const HeadlessWorld& world,
                       uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(ShotRequestPacket))
        return;
    const ShotRequestPacket* shot =
        reinterpret_cast<const ShotRequestPacket*>(buffer);
    auto shooterIt = players.find(shot->header.playerId);
    const bool ownsShooter =
        shooterIt != players.end() &&
        sameAddress(shooterIt->second.addr, from);
    if (!ownsShooter)
    {
        printf("%s [NET SHOT OWNERSHIP] claimedShooter=%u "
               "accepted=0 reason=sender-address-mismatch\n",
               serverTimestamp(), shot->header.playerId);
        return;
    }

    ServerPlayer& shooter = shooterIt->second;
    if (shooter.dead ||
        (shooter.lastShotSerial != 0 &&
         (int32_t)(shot->shotSerial - shooter.lastShotSerial) <= 0))
    {
        printf("%s [NET SHOT FILTER] shooter=%u serial=%u "
               "accepted=0 reason=%s lastSerial=%u\n",
               serverTimestamp(), shooter.id, shot->shotSerial,
               shooter.dead ? "dead" : "duplicate-or-stale",
               shooter.lastShotSerial);
        return;
    }

    const bool validWeapon =
        shot->weapon == NETWORK_WEAPON_REVOLVER ||
        shot->weapon == NETWORK_WEAPON_GODBALL ||
        shot->weapon == NETWORK_WEAPON_SHOTGUN ||
        shot->weapon == NETWORK_WEAPON_SWORDSWORD;
    const bool validImpact =
        shot->impactType <= SHOT_IMPACT_ENTITY;
    const glm::vec3 origin{
        shot->originX, shot->originY, shot->originZ};
    const glm::vec3 position{
        shot->hitX, shot->hitY, shot->hitZ};
    const glm::vec3 direction{
        shot->dirX, shot->dirY, shot->dirZ};
    const glm::vec3 normal{
        shot->normalX, shot->normalY, shot->normalZ};
    const bool finite =
        std::isfinite(origin.x) && std::isfinite(origin.y) &&
        std::isfinite(origin.z) && std::isfinite(position.x) &&
        std::isfinite(position.y) && std::isfinite(position.z) &&
        std::isfinite(direction.x) && std::isfinite(direction.y) &&
        std::isfinite(direction.z) &&
        std::isfinite(normal.x) && std::isfinite(normal.y) &&
        std::isfinite(normal.z) && std::isfinite(shot->power);
    const float shotDistance = finite
        ? glm::length(position - origin)
        : std::numeric_limits<float>::infinity();
    const float originDistance = finite
        ? glm::length(origin - shooter.pos)
        : std::numeric_limits<float>::infinity();
    const float directionLength = finite
        ? glm::length(direction)
        : 0.0f;
    const bool validGeometry =
        finite && shotDistance <= 150.0f &&
        originDistance <= 8.0f &&
        directionLength >= 0.5f && directionLength <= 1.5f;
    if (!validWeapon || !validImpact || !validGeometry ||
        shot->shotSerial == 0)
    {
        printf("%s [NET SHOT FILTER] shooter=%u serial=%u "
               "accepted=0 weapon=%u impact=%u finite=%d "
               "distance=%.2f originDistance=%.2f dirLength=%.2f\n",
               serverTimestamp(), shooter.id, shot->shotSerial,
               shot->weapon, shot->impactType, (int)finite,
               shotDistance, originDistance, directionLength);
        return;
    }
    shooter.lastShotSerial = shot->shotSerial;

    constexpr uint16_t ALLOWED_EFFECT_FLAGS =
        SHOT_EFFECT_MUZZLE |
        SHOT_EFFECT_TRACER |
        SHOT_EFFECT_SHOOT_SOUND |
        SHOT_EFFECT_WORLD_IMPACT |
        SHOT_EFFECT_DEBRIS |
        SHOT_EFFECT_ENTITY_IMPACT |
        SHOT_EFFECT_BLOOD |
        SHOT_EFFECT_HIT_SOUND |
        SHOT_EFFECT_WEAPON_TRIGGER;

    ShotEventPacket event{};
    event.header.type = PACKET_SHOT_EVENT;
    event.header.tick = tick;
    event.header.playerId = shooter.id;
    event.shotSerial = shot->shotSerial;
    event.clientTimeMs = shot->clientTimeMs;
    event.shooterPlayerId = shooter.id;
    event.targetPlayerId = shot->targetPlayerId;
    event.power = std::clamp(shot->power, 0.0f, 200.0f);
    event.effectFlags = shot->effectFlags & ALLOWED_EFFECT_FLAGS;
    event.weapon = shot->weapon;
    event.impactType = shot->impactType;
    if (event.weapon == NETWORK_WEAPON_GODBALL)
    {
        event.effectFlags &= ~(
            SHOT_EFFECT_MUZZLE |
            SHOT_EFFECT_TRACER |
            SHOT_EFFECT_SHOOT_SOUND |
            SHOT_EFFECT_WEAPON_TRIGGER);
    }
    if (event.impactType == SHOT_IMPACT_NONE)
    {
        event.effectFlags &= ~(
            SHOT_EFFECT_WORLD_IMPACT |
            SHOT_EFFECT_DEBRIS |
            SHOT_EFFECT_ENTITY_IMPACT |
            SHOT_EFFECT_BLOOD |
            SHOT_EFFECT_HIT_SOUND);
    }
    else if (event.impactType == SHOT_IMPACT_WORLD)
    {
        event.effectFlags &= ~(
            SHOT_EFFECT_ENTITY_IMPACT |
            SHOT_EFFECT_BLOOD);
    }
    else
    {
        event.effectFlags &= ~(
            SHOT_EFFECT_WORLD_IMPACT |
            SHOT_EFFECT_DEBRIS);
    }
    event.originX = origin.x;
    event.originY = origin.y;
    event.originZ = origin.z;
    event.hitX = position.x;
    event.hitY = position.y;
    event.hitZ = position.z;
    const glm::vec3 normalizedDirection =
        glm::normalize(direction);
    event.dirX = normalizedDirection.x;
    event.dirY = normalizedDirection.y;
    event.dirZ = normalizedDirection.z;
    const glm::vec3 normalizedNormal =
        glm::length(normal) > 0.001f
        ? glm::normalize(normal)
        : -normalizedDirection;
    event.normalX = normalizedNormal.x;
    event.normalY = normalizedNormal.y;
    event.normalZ = normalizedNormal.z;
    event.knockX = shot->knockX;
    event.knockY = shot->knockY;
    event.knockZ = shot->knockZ;
    event.lastServerTick = shot->lastServerTick;

    auto targetIt = players.find(shot->targetPlayerId);
    bool damageConfirmed = false;

    if (MimitaNet::gNetDamageDebug)
    {
        printf("[NET DAMAGE] shooter=%u target=%u impactType=%u "
               "damage=%d shooterDead=%d targetDead=%d "
               "shooterHealth=%d targetHealth=%d\n",
               shooter.id, shot->targetPlayerId, shot->impactType,
               shot->damage, (int)shooter.dead,
               (int)(targetIt != players.end() && targetIt->second.dead),
               shooter.health,
               targetIt != players.end() ? targetIt->second.health : -1);
    }

    if (shot->impactType == SHOT_IMPACT_ENTITY &&
        targetIt != players.end() &&
        shooterIt != targetIt &&
        !targetIt->second.dead &&
        shot->damage > 0 && shot->damage <= 200)
    {
        ServerPlayer& target = targetIt->second;

        glm::vec3 rewoundPos;
        bool hasRewound = getPositionAtTick(
            target, shot->lastServerTick, rewoundPos);

        glm::vec3 checkPos = hasRewound
            ? rewoundPos
            : target.pos;

        const float rewindDistance = glm::length(
            position - (checkPos + glm::vec3(0.0f, 0.0f, 0.8f)));

        if (gNetHitDebug)
        {
            printf("[NET HIT] shooter=%u target=%u "
                   "origin=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f) "
                   "claimedHit=(%.2f,%.2f,%.2f) "
                   "rewindTick=%u rewindDist=%.2f "
                   "targetRewoundPos=(%.2f,%.2f,%.2f) "
                   "targetCurrentPos=(%.2f,%.2f,%.2f)\n",
                   shooter.id, shot->targetPlayerId,
                   origin.x, origin.y, origin.z,
                   direction.x, direction.y, direction.z,
                   position.x, position.y, position.z,
                   shot->lastServerTick, rewindDistance,
                   checkPos.x, checkPos.y, checkPos.z,
                   target.pos.x, target.pos.y, target.pos.z);
        }

        if (rewindDistance <= 2.5f)
        {
            glm::vec3 shotDir = glm::normalize(direction);
                        glm::vec3 worldHit, worldNormal;
            bool hitWorld = serverRaycastWorld(
                origin, shotDir, shotDistance, world, worldHit, worldNormal);

            bool occluded = hitWorld && glm::length(worldHit - origin) < rewindDistance;

            if (gNetHitDebug)
            {
                printf("[NET HIT OCCLUSION] hitWorld=%d occluded=%d "
                       "worldHitDist=%.2f claimedDist=%.2f\n",
                       (int)hitWorld, (int)occluded,
                       hitWorld ? glm::length(worldHit - origin) : 0.0f,
                       rewindDistance);
            }

            if (!occluded)
            {
                damageConfirmed = true;
                event.damage = shot->damage;
                target.health = std::max(0, target.health - shot->damage);
                event.targetHealth = target.health;
                event.damageConfirmed = 1;
                if (target.health == 0)
                {
                    target.dead = true;
                    target.respawnSeconds = 2.0f;
                    target.vel = glm::vec3(0.0f);
                    event.killed = 1;
                }

                printf("%s [NET SHOT REWIND] shooter=%u target=%u "
                       "rewoundTick=%u rewindDist=%.2f occluded=%d "
                       "hasHistory=%d\n",
                       serverTimestamp(), shooter.id, target.id,
                       shot->lastServerTick, rewindDistance,
                       (int)occluded, (int)hasRewound);
            }
            else
            {
                printf("%s [NET SHOT OCCLUDED] shooter=%u target=%u "
                       "worldHit=%.2f < hitDist=%.2f\n",
                       serverTimestamp(), shooter.id, target.id,
                       glm::length(worldHit - origin), rewindDistance);
            }
        }
        else
        {
            printf("%s [NET SHOT REWIND MISS] shooter=%u target=%u "
                   "rewoundTick=%u rewindDist=%.2f (<=2.5f required) "
                   "currentDist=%.2f hasHistory=%d\n",
                   serverTimestamp(), shooter.id, target.id,
                   shot->lastServerTick, rewindDistance,
                   glm::length(position - (target.pos + glm::vec3(0,0,0.8f))),
                   (int)hasRewound);
        }
    }

    if (!damageConfirmed && event.impactType == SHOT_IMPACT_ENTITY)
    {
        if (gNetDamageDebug)
        {
            printf("[NET DAMAGE REJECT] shooter=%u target=%u "
                   "reason=", shooter.id, shot->targetPlayerId);
            if (targetIt == players.end())
                printf("target-not-found");
            else if (targetIt->second.dead)
                printf("target-dead");
            else
                printf("rewind-dist=%.2f-or-occluded",
                       targetIt != players.end() ?
                       glm::length(glm::vec3(event.hitX, event.hitY, event.hitZ) -
                       (targetIt->second.pos + glm::vec3(0,0,0.8f))) : 0.0f);
            printf("\n");
        }
        event.targetPlayerId = 0;
        event.impactType = SHOT_IMPACT_NONE;
        event.effectFlags &= ~(
            SHOT_EFFECT_ENTITY_IMPACT |
            SHOT_EFFECT_BLOOD |
            SHOT_EFFECT_HIT_SOUND);
    }

    printf("%s [NET SHOT RELAY] shooter=%u serial=%u target=%u "
           "weapon=%u impact=%u flags=0x%03x damageConfirmed=%d\n",
           serverTimestamp(), shooter.id, event.shotSerial,
           event.targetPlayerId, event.weapon, event.impactType,
           event.effectFlags, (int)event.damageConfirmed);

    for (const auto& playerEntry : players)
    {
        sendto(sock, (const char*)&event, sizeof(event), 0,
               (sockaddr*)&playerEntry.second.addr,
               sizeof(playerEntry.second.addr));
        ++totalPacketsOut;
    }
}

void handleChatMessage(SOCKET sock, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(ChatPacket))
        return;
    ChatPacket* chat = const_cast<ChatPacket*>(reinterpret_cast<const ChatPacket*>(buffer));
    auto it = players.find(chat->header.playerId);
    if (it == players.end())
        return;

    chat->header.tick = tick;
    printf("%s [CHAT] %s: %s\n", serverTimestamp(),
           it->second.name.c_str(), chat->text);

    for (const auto& playerEntry : players)
    {
        if (playerEntry.first == chat->header.playerId)
            continue;
        sendto(sock, (const char*)chat, sizeof(ChatPacket), 0,
               (sockaddr*)&playerEntry.second.addr,
               sizeof(playerEntry.second.addr));
        ++totalPacketsOut;
    }
}

void handlePing(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                uint32_t tick)
{
    if (bytes < (int)sizeof(PingPacket))
        return;
    PingPacket pong =
        *reinterpret_cast<const PingPacket*>(buffer);
    pong.header.tick = tick;
    sendto(sock, (const char*)&pong, sizeof(pong), 0,
           (sockaddr*)&from, sizeof(from));
}

void handleNpcDamageRequest(SOCKET sock, const char* buffer, int bytes,
                            const sockaddr_in& from,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            std::unordered_map<uint32_t, ServerNpc>& npcs,
                            uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(NpcDamageRequestPacket))
        return;
    const NpcDamageRequestPacket* req =
        reinterpret_cast<const NpcDamageRequestPacket*>(buffer);
    auto shooterIt = players.find(req->header.playerId);
    if (shooterIt == players.end() ||
        !sameAddress(shooterIt->second.addr, from))
        return;

    auto npcIt = npcs.find(req->npcEntityId);
    if (npcIt == npcs.end())
    {
        printf("%s [NET NPC DAMAGE] shooter=%u npcId=%u accepted=0 reason=npc-not-found\n",
               serverTimestamp(), req->header.playerId, req->npcEntityId);
        return;
    }

    ServerNpc& target = npcIt->second;
    const int clamped = std::clamp((int)req->damage, 1, 200);
    target.health -= clamped;
    const bool killed = target.health <= 0;
    if (killed)
    {
        target.health = 0;
        printf("%s [NET NPC KILL] shooter=%u npcId=%u name=\"%s\"\n",
               serverTimestamp(), req->header.playerId,
               target.entityId, target.name.c_str());
    }

    NpcDamageEventPacket event{};
    event.header.type = PACKET_NPC_DAMAGE_EVENT;
    event.header.tick = tick;
    event.header.playerId = req->header.playerId;
    event.npcEntityId = req->npcEntityId;
    event.shooterPlayerId = req->header.playerId;
    event.damage = clamped;
    event.npcHealth = target.health;
    event.killed = killed ? 1 : 0;
    event.originX = req->originX; event.originY = req->originY; event.originZ = req->originZ;
    event.hitX = req->hitX; event.hitY = req->hitY; event.hitZ = req->hitZ;
    event.dirX = req->dirX; event.dirY = req->dirY; event.dirZ = req->dirZ;
    event.normalX = req->normalX; event.normalY = req->normalY; event.normalZ = req->normalZ;
    event.effectFlags = req->effectFlags;
    event.weapon = req->weapon;
    event.impactType = req->impactType;

    for (const auto& pe : players)
    {
        sendto(sock, (const char*)&event, sizeof(event), 0,
               (sockaddr*)&pe.second.addr, sizeof(pe.second.addr));
        ++totalPacketsOut;
    }

    if (killed)
        npcs.erase(npcIt);
}

void handleServerCommand(const char* buffer, int bytes,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (bytes < (int)sizeof(ServerCommandPacket))
        return;
    ServerCommandPacket* cmd =
        const_cast<ServerCommandPacket*>(reinterpret_cast<const ServerCommandPacket*>(buffer));
    auto it = players.find(cmd->header.playerId);
    if (it == players.end())
        return;

    cmd->commandText[239] = '\0';
    const std::string commandStr(cmd->commandText);

    printf("%s [SERVER COMMAND] playerId=%u name=\"%s\" cmd=\"%s\"\n",
           serverTimestamp(), it->second.id, it->second.name.c_str(),
           commandStr.c_str());

    if (commandStr == "npc_delete_all")
    {
        printf("%s [SERVER COMMAND] npc_delete_all by playerId=%u count=%zu\n",
               serverTimestamp(), it->second.id, npcs.size());
        npcs.clear();
    }
    else
    {
        printf("%s [SERVER COMMAND] unknown cmd=\"%s\" from playerId=%u\n",
               serverTimestamp(), commandStr.c_str(), it->second.id);
    }
}

void handleClientTimeout(std::unordered_map<uint32_t, ServerPlayer>& players)
{
    for (auto it = players.begin(); it != players.end(); )
    {
        const uint64_t silentMs = nowMs() - it->second.lastHeardMs;
        if (silentMs > CLIENT_TIMEOUT_MS)
        {
            printf("%s [SERVER DISCONNECT] reason=timeout id=%u name=\"%s\" lastHeard=%llums ago ping=%dms\n",
                   serverTimestamp(), it->second.id, it->second.name.c_str(),
                   (unsigned long long)silentMs, it->second.pingMs);
            it = players.erase(it);
        }
        else
            ++it;
    }
}

void checkVoidDeath(std::unordered_map<uint32_t, ServerPlayer>& players,
                    std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    const VoidDeathConfig& vdc = getVoidDeathConfig();
    if (!vdc.enabled)
        return;

    for (auto& kv : players)
    {
        if (!kv.second.dead && kv.second.pos.z < vdc.killZ)
        {
            kv.second.health = 0;
            kv.second.dead = true;
            kv.second.respawnSeconds = 2.0f;
            kv.second.vel = glm::vec3(0.0f);
            printf("%s [SERVER VOID DEATH] playerId=%u name=%s z=%.1f killZ=%.1f\n",
                   serverTimestamp(), kv.second.id, kv.second.name.c_str(),
                   kv.second.pos.z, vdc.killZ);
        }
    }
    for (auto& kv : npcs)
    {
        if (kv.second.pos.z < vdc.killZ)
        {
            printf("%s [SERVER VOID DEATH] npcId=%u z=%.1f killZ=%.1f\n",
                   serverTimestamp(), kv.second.entityId,
                   kv.second.pos.z, vdc.killZ);
            kv.second.health = 0;
            kv.second.pos = {1.0f + (float)(kv.second.entityId - 1) * 1.5f, 5.0f, 30.0f};
            kv.second.vel = glm::vec3(0.0f);
        }
    }
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

#include "network/server.h"
#include "network/multiplayer-context.h"
#include "network/network-weapons.h"
#include "void-death/void-death.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

namespace MimitaNet {

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
        std::strcmp(networkWeaponTypeName(shot->weapon), "unknown") != 0 &&
        shot->weapon != NETWORK_WEAPON_NONE &&
        !networkWeaponTypeIsProjectile(shot->weapon);
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
               "accepted=0 weapon=%u weaponName=%s impact=%u finite=%d "
               "distance=%.2f originDistance=%.2f dirLength=%.2f\n",
               serverTimestamp(), shooter.id, shot->shotSerial,
               shot->weapon, networkWeaponTypeName(shot->weapon),
               shot->impactType, (int)finite,
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

    if (shot->weapon == NETWORK_WEAPON_SHOTGUN)
    {
        printf("%s [SHOTGUN SERVER REQUEST] shooter=%u serial=%u target=%u "
               "claimedDamage=%d accepted=pending reason=validation-start\n",
               serverTimestamp(), shooter.id, shot->shotSerial,
               shot->targetPlayerId, shot->damage);
    }

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
                ServerDamageResult damage = applyServerDamage(
                    players, target, shooter.id, shot->damage,
                    glm::vec3(shot->knockX, shot->knockY, shot->knockZ),
                    ServerDamageSource::Hitscan);
                event.targetHealth = damage.healthAfter;
                event.damageConfirmed = 1;
                if (damage.killed)
                    event.killed = 1;

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

    if (shot->weapon == NETWORK_WEAPON_SHOTGUN)
    {
        printf("%s [SHOTGUN SERVER REQUEST] shooter=%u serial=%u target=%u "
               "claimedDamage=%d accepted=%d reason=%s\n",
               serverTimestamp(), shooter.id, shot->shotSerial,
               shot->targetPlayerId, shot->damage, (int)damageConfirmed,
               damageConfirmed ? "accepted" : "validation-failed");
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

} // namespace MimitaNet

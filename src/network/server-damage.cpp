// 07 21 2026, 17 10
/* purpose
* Owns authoritative server damage application and confirmed damage event packet creation.
* Bridges server-owned knockback into shared movement external impulse state.
* Keeps kill, death, health, and damage-confirmed replication decisions server-side.
* Does NOT trust client health, ammo, score, damage, projectile hits, or knockback outcomes.
* Does NOT simulate movement frames, poll sockets, or render damage presentation.
* Does NOT let death leave active movement impulse state behind.
*/

#include "network/server.h"

#include <algorithm>
#include <cstdio>

namespace MimitaNet {

static const char* damageSourceName(ServerDamageSource source)
{
    switch (source)
    {
    case ServerDamageSource::Hitscan: return "hitscan";
    case ServerDamageSource::Melee: return "melee";
    case ServerDamageSource::PhysicalContact: return "physical_contact";
    case ServerDamageSource::RocketExplosion: return "rocket_explosion";
    case ServerDamageSource::GrenadeExplosion: return "grenade_explosion";
    default: return "unknown";
    }
}

static uint8_t damageConfirmedSource(ServerDamageSource source)
{
    switch (source)
    {
    case ServerDamageSource::Hitscan: return DAMAGE_CONFIRMED_HITSCAN;
    case ServerDamageSource::Melee: return DAMAGE_CONFIRMED_MELEE;
    case ServerDamageSource::PhysicalContact: return DAMAGE_CONFIRMED_PHYSICAL_CONTACT;
    case ServerDamageSource::RocketExplosion: return DAMAGE_CONFIRMED_ROCKET_EXPLOSION;
    case ServerDamageSource::GrenadeExplosion: return DAMAGE_CONFIRMED_GRENADE_EXPLOSION;
    default: return 0;
    }
}

ServerDamageResult applyServerDamage(std::unordered_map<uint32_t, ServerPlayer>& players,
                                     ServerPlayer& target,
                                     uint32_t attackerPlayerId,
                                     int damage,
                                     const glm::vec3& knockback,
                                     ServerDamageSource source)
{
    ServerDamageResult result;
    result.healthBefore = target.health;
    result.healthAfter = target.health;

    if (target.dead || damage <= 0)
    {
        printf("%s [SERVER DAMAGE] target=%u attacker=%u source=%s accepted=0 "
               "reason=%s damage=%d health=%d\n",
               serverTimestamp(), target.id, attackerPlayerId,
               damageSourceName(source),
               target.dead ? "target-dead" : "non-positive-damage",
               damage, target.health);
        return result;
    }

    const int clampedDamage = std::clamp(damage, 1, 500);
    target.health = std::max(0, target.health - clampedDamage);
    target.vel += knockback;
    recordServerMovementExternalImpulse(target, knockback);
    result.applied = true;
    result.healthAfter = target.health;

    if (target.health == 0)
    {
        target.dead = true;
        target.respawnSeconds = 2.0f;
        target.vel = glm::vec3(0.0f);
        target.movement.movementEnabled = false;
        target.movement.baseVelocity = glm::vec3(0.0f);
        target.movement.externalImpulse = glm::vec3(0.0f);
        target.attackQueued = false;
        target.deaths += 1;
        if (attackerPlayerId != target.id)
        {
            auto attacker = players.find(attackerPlayerId);
            if (attacker != players.end())
            {
                attacker->second.kills += 1;
                // Heal the attacker to full health
                attacker->second.health = 100;
            }
        }
        result.killed = true;
    }

    printf("%s [SERVER DAMAGE] target=%u attacker=%u source=%s damage=%d "
           "healthBefore=%d healthAfter=%d killed=%d knockback=(%.2f,%.2f,%.2f)\n",
           serverTimestamp(), target.id, attackerPlayerId, damageSourceName(source),
           clampedDamage, result.healthBefore, result.healthAfter,
           (int)result.killed, knockback.x, knockback.y, knockback.z);
    return result;
}

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
    uint32_t causeSerial,
    uint32_t projectileId)
{
    if (!result.applied)
        return ReliableGameplayEventQueueResult::Queued;

    DamageConfirmedEventPacket event{};
    event.header.type = PACKET_DAMAGE_CONFIRMED_EVENT;
    event.header.tick = tick;
    event.header.playerId = attackerPlayerId;
    event.eventId = nextReliableGameplayEventId();
    event.eventSessionId = serverReliableEventSessionId();
    event.attackerPlayerId = attackerPlayerId;
    event.targetPlayerId = target.id;
    event.causeSerial = causeSerial;
    event.projectileId = projectileId;
    auto attackerIt = players.find(attackerPlayerId);
    event.attackerSpawnGeneration = attackerIt != players.end()
        ? attackerIt->second.spawnGeneration : 0;
    event.targetSpawnGeneration = target.spawnGeneration;
    event.damage = damage;
    event.healthBefore = result.healthBefore;
    event.healthAfter = result.healthAfter;
    event.source = damageConfirmedSource(source);
    event.weapon = weapon;
    event.killed = result.killed ? 1 : 0;
    event.hitX = hit.x;
    event.hitY = hit.y;
    event.hitZ = hit.z;
    event.normalX = normal.x;
    event.normalY = normal.y;
    event.normalZ = normal.z;
    event.knockX = knockback.x;
    event.knockY = knockback.y;
    event.knockZ = knockback.z;
    return queueReliableGameplayEventToAll(
        sock, players, &event, sizeof(event), event.eventId,
        event.eventSessionId, totalPacketsOut);
}

} // namespace MimitaNet

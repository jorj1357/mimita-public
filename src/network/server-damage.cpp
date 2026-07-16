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
    case ServerDamageSource::RocketExplosion: return "rocket_explosion";
    case ServerDamageSource::GrenadeExplosion: return "grenade_explosion";
    default: return "unknown";
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
    result.applied = true;
    result.healthAfter = target.health;

    if (target.health == 0)
    {
        target.dead = true;
        target.respawnSeconds = 2.0f;
        target.vel = glm::vec3(0.0f);
        target.attackQueued = false;
        target.deaths += 1;
        if (attackerPlayerId != target.id)
        {
            auto attacker = players.find(attackerPlayerId);
            if (attacker != players.end())
                attacker->second.kills += 1;
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

} // namespace MimitaNet

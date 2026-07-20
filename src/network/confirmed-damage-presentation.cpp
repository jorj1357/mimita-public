// 07 19 2026, 13 05
/* purpose
* Implements generic attacker-only presentation for confirmed network damage.
* Reuses existing hitmarker, hitmarker audio, and HitEffects damage-number paths.
* Rejects duplicate, stale-session, stale-spawn, non-attacker, and self-damage events.
* Does NOT apply health, kill credit, death, knockback, or projectile authority.
* Does NOT contain weapon-specific name comparisons.
* Does NOT create new damage or reliability rules.
*/

#include "network/confirmed-damage-presentation.h"

#include "audio/hitmarker-audio.h"
#include "config/weapon-hitfx-config.h"
#include "network/multiplayer-context.h"
#include "network/network-weapons.h"
#include "ui/hitmarker.h"

#include <cstdio>
#include <string>

#include <glm/glm.hpp>

namespace MimitaNet {
namespace {

uint64_t presentationEventKey(uint32_t eventSessionId, uint32_t eventId)
{
    return ((uint64_t)eventSessionId << 32) | (uint64_t)eventId;
}

void rememberPresentedDamage(MultiplayerContext& ctx, uint64_t key)
{
    ctx.presentedDamageEventIds.insert(key);
    ctx.presentedDamageEventOrder.push_back(key);
    while (ctx.presentedDamageEventOrder.size() > 512)
    {
        ctx.presentedDamageEventIds.erase(ctx.presentedDamageEventOrder.front());
        ctx.presentedDamageEventOrder.pop_front();
    }
}

bool hasMatchingVictimSpawn(const MultiplayerContext& ctx,
                            const DamageConfirmedEventPacket& event)
{
    if (event.targetSpawnGeneration == 0)
        return true;
    if (event.targetPlayerId == ctx.localPlayerId)
        return event.targetSpawnGeneration == ctx.lastKnownSpawnGeneration;

    auto it = ctx.remotePlayers.find(event.targetPlayerId);
    return it == ctx.remotePlayers.end() ||
        it->second.spawnGeneration == event.targetSpawnGeneration;
}

std::string playerNameFor(const MultiplayerContext& ctx, uint32_t playerId)
{
    auto it = ctx.playerRegistry.find(playerId);
    if (it != ctx.playerRegistry.end())
        return it->second.name;
    return "player_" + std::to_string(playerId);
}

} // namespace

bool presentConfirmedDamage(MultiplayerContext& ctx,
                            const DamageConfirmedEventPacket& event,
                            const ConfirmedDamagePresentationSink* sink)
{
    if (event.eventId == 0 || event.eventSessionId == 0 || event.damage <= 0)
        return false;
    if (ctx.reliableEventSessionId != 0 && event.eventSessionId != ctx.reliableEventSessionId)
        return false;

    const uint64_t key = presentationEventKey(event.eventSessionId, event.eventId);
    if (ctx.presentedDamageEventIds.count(key))
        return false;

    if (event.attackerPlayerId != ctx.localPlayerId)
        return false;
    if (event.attackerSpawnGeneration != 0 &&
        event.attackerSpawnGeneration != ctx.lastKnownSpawnGeneration)
        return false;
    if (!hasMatchingVictimSpawn(ctx, event))
        return false;

    const char* weaponId = networkWeaponTypeName(event.weapon);
    const auto& presentation = WeaponHitFxConfig::instance().presentationFor(weaponId);
    if (!presentation.enabled)
        return false;
    if (event.attackerPlayerId == event.targetPlayerId && !presentation.selfDamageFeedback)
        return false;

    rememberPresentedDamage(ctx, key);

    HitEvent hit;
    hit.position = {event.hitX, event.hitY, event.hitZ};
    hit.normal = {event.normalX, event.normalY, event.normalZ};
    hit.direction = glm::length(glm::vec3(event.knockX, event.knockY, event.knockZ)) > 0.001f
        ? glm::normalize(glm::vec3(event.knockX, event.knockY, event.knockZ))
        : -hit.normal;
    hit.hitEntity = true;
    hit.damage = event.damage;
    hit.attacker = playerNameFor(ctx, event.attackerPlayerId);
    hit.victim = playerNameFor(ctx, event.targetPlayerId);
    hit.weaponSource = weaponId;

    if (presentation.hitmarker)
    {
        if (sink && sink->showHitmarker)
            sink->showHitmarker(event.damage, sink->user);
        else
            hitmarkerVisualOnly(event.damage);
    }
    if (presentation.hitSound)
    {
        if (sink && sink->playHitSound)
            sink->playHitSound(event.damage, sink->user);
        else
            playHitmarkerSound(event.damage);
    }
    if (presentation.damageNumber)
    {
        if (sink && sink->showDamageNumber)
            sink->showDamageNumber(hit, sink->user);
        else
            HitEffects::spawnHitEffects(hit.position, hit.direction, hit.normal,
                                        hit.damage, hit.attacker, hit.victim, true);
    }

    printf("[NET DAMAGE PRESENT] eventId=%u attacker=%u target=%u damage=%d weapon=%u hitmarker=%d sound=%d damageNumber=%d\n",
           event.eventId, event.attackerPlayerId, event.targetPlayerId,
           event.damage, (unsigned)event.weapon, (int)presentation.hitmarker,
           (int)presentation.hitSound, (int)presentation.damageNumber);
    return true;
}

} // namespace MimitaNet

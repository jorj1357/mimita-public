// 09 01 2026, 00 00
/* purpose
* Owns authoritative server damage application and confirmed damage event packet creation.
* Bridges server-owned knockback into shared movement external impulse state.
* Keeps kill, death, health, and damage-confirmed replication decisions server-side.
* Filters friendly fire in team-based modes (TDM) at the authoritative damage path.
* Does NOT trust client health, ammo, score, damage, projectile hits, or knockback outcomes.
* Does NOT simulate movement frames, poll sockets, or render damage presentation.
* Does NOT let death leave active movement impulse state behind.
*/

#include "network/server.h"
#include "network/server-gamemode.h"
#include "persistence/persistence-emit.h"
#include "combat/weapon-registry.h"
#include "network/network-weapons.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

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

ServerActorRef findServerActor(uint32_t actorId,
                               ServerActorKind actorKind,
                               std::unordered_map<uint32_t, ServerPlayer>& players,
                               std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    ServerActorRef ref;
    ref.id = actorId;
    ref.kind = actorKind;
    if (actorKind == ServerActorKind::Npc) {
        auto it = npcs.find(actorId);
        if (it != npcs.end()) ref.npc = &it->second;
    } else {
        auto it = players.find(actorId);
        if (it != players.end()) ref.player = &it->second;
    }
    return ref;
}

static ServerDamageResult applyPlayerDamageLegacy(
    std::unordered_map<uint32_t, ServerPlayer>& players,
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
        DBG(Network, "SERVER DAMAGE target=%u attacker=%u source=%s accepted=0 "
            "reason=%s damage=%d health=%d",
            target.id, attackerPlayerId, damageSourceName(source),
            target.dead ? "target-dead" : "non-positive-damage",
            damage, target.health);
        return result;
    }

    // Disconnected/reconnecting players take no damage: someone who crashed or
    // froze should not die while offline. Their body is frozen and invulnerable
    // for the reconnect grace window.
    // TODO(anti-cheat): a client could drop packets to dodge damage. Honest-play
    // assumption for now; revisit with server-side movement/DPS accounting.
    if (target.connectionStale)
    {
        DBG(Network, "SERVER DAMAGE target=%u attacker=%u source=%s accepted=0 "
            "reason=target-reconnecting damage=%d health=%d",
            target.id, attackerPlayerId, damageSourceName(source), damage, target.health);
        return result;
    }

    // Team-based friendly fire filtering: teammates cannot damage each other.
    // Self-damage (attacker == target) is always allowed for rocket jumping.
    if (attackerPlayerId != target.id && target.matchTeam >= 0)
    {
        auto attackerIt = players.find(attackerPlayerId);
        if (attackerIt != players.end() && attackerIt->second.matchTeam >= 0)
        {
            if (attackerIt->second.matchTeam == target.matchTeam)
            {
                DBG(Network, "SERVER DAMAGE target=%u attacker=%u source=%s accepted=0 "
                    "reason=friendly-fire teams=%d damage=%d health=%d",
                    target.id, attackerPlayerId, damageSourceName(source),
                    target.matchTeam, damage, target.health);
                return result;
            }
        }
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
        target.respawnSeconds = 0.01f;  // instant respawn (next server tick)
        target.vel = glm::vec3(0.0f);
        target.movement.movementEnabled = false;
        target.movement.baseVelocity = glm::vec3(0.0f);
        target.movement.externalImpulse = glm::vec3(0.0f);
        target.attackQueued = false;
        target.deaths += 1;
        // Clear NPC damage tracking on death so it doesn't carry over to next life
        target.lastNpcDamageSourceId = 0;
        target.lastNpcDamageTick = 0;
        if (attackerPlayerId != target.id)
        {
            auto attacker = players.find(attackerPlayerId);
            if (attacker != players.end())
            {
                attacker->second.kills += 1;
                // Heal the attacker to full health
                attacker->second.health = serverMaxHp();
            }
        }
        result.killed = true;
        emitPvPKillPersistenceEvent(players, attackerPlayerId, target.id,
            damageSourceName(source), 0, target.pos, target.pos);
    }

    DBG(Network, "SERVER DAMAGE target=%u attacker=%u source=%s damage=%d "
        "healthBefore=%d healthAfter=%d killed=%d knockback=(%.2f,%.2f,%.2f)",
        target.id, attackerPlayerId, damageSourceName(source), clampedDamage,
        result.healthBefore, result.healthAfter, (int)result.killed,
        knockback.x, knockback.y, knockback.z);
    return result;
}

ServerDamageResult applyActorDamage(
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    const ServerActorDamageRequest& request)
{
    ServerDamageResult result;
    result.eventId = request.eventId;
    result.correlationId = request.correlationId;

    const ServerActorRef victim = findServerActor(
        request.victim.id, request.victim.kind, players, npcs);
    if (!victim.player || victim.kind != ServerActorKind::Player) {
        result.rejectionReason = "phase1-player-victim-required";
        Debug::log(Debug::Category::Networking,
            "[ACTOR_DAMAGE_REJECTED] victim=%u kind=%u reason=%s event=%u\n",
            request.victim.id, (unsigned)request.victim.kind,
            result.rejectionReason.c_str(), request.eventId);
        return result;
    }

    if (request.attacker.kind != ServerActorKind::Player) {
        result.rejectionReason = "phase1-player-attacker-required";
        Debug::log(Debug::Category::Networking,
            "[ACTOR_DAMAGE_REJECTED] attacker=%u kind=%u reason=%s event=%u\n",
            request.attacker.id, (unsigned)request.attacker.kind,
            result.rejectionReason.c_str(), request.eventId);
        return result;
    }

    const ServerActorRef attacker = findServerActor(
        request.attacker.id, request.attacker.kind, players, npcs);
    if (!attacker.player) {
        result.rejectionReason = "attacker-not-found";
        Debug::log(Debug::Category::Networking,
            "[ACTOR_DAMAGE_REJECTED] attacker=%u kind=%u reason=%s event=%u\n",
            request.attacker.id, (unsigned)request.attacker.kind,
            result.rejectionReason.c_str(), request.eventId);
        return result;
    }

    Debug::log(Debug::Category::Networking,
        "[ACTOR_DAMAGE_BEGIN] attacker=%u victim=%u damage=%d event=%u tick=%u\n",
        request.attacker.id, request.victim.id, request.damage,
        request.eventId, request.serverTick);
    result = applyPlayerDamageLegacy(players, *victim.player,
                                     attacker.id, request.damage,
                                     request.knockback, request.source);
    result.eventId = request.eventId;
    result.correlationId = request.correlationId;
    Debug::log(Debug::Category::Networking,
        "[ACTOR_DAMAGE_APPLIED] attacker=%u victim=%u applied=%d killed=%d health=%d->%d event=%u\n",
        request.attacker.id, request.victim.id, (int)result.applied,
        (int)result.killed, result.healthBefore, result.healthAfter,
        request.eventId);
    return result;
}

ServerDamageResult applyServerDamage(std::unordered_map<uint32_t, ServerPlayer>& players,
                                     ServerPlayer& target,
                                     uint32_t attackerPlayerId,
                                     int damage,
                                     const glm::vec3& knockback,
                                     ServerDamageSource source)
{
    // Compatibility wrapper for existing callers. Player-to-player damage
    // now crosses the shared actor boundary; NPC callers retain their legacy
    // behavior until their migration phase supplies the NPC registry.
    if (attackerPlayerId != 0) {
        std::unordered_map<uint32_t, ServerNpc> noNpcs;
        ServerActorDamageRequest request;
        request.attacker = {attackerPlayerId, ServerActorKind::Player, nullptr, nullptr};
        request.victim = {target.id, ServerActorKind::Player, nullptr, nullptr};
        request.damage = damage;
        request.knockback = knockback;
        request.source = source;
        return applyActorDamage(players, noNpcs, request);
    }
    return applyPlayerDamageLegacy(players, target, attackerPlayerId,
                                   damage, knockback, source);
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
    uint32_t projectileId,
    uint32_t attackerNpcId,
    const std::string& weaponDefId)
{
    if (!result.applied)
        return ReliableGameplayEventQueueResult::Queued;

    DamageConfirmedEventPacket event{};
    event.header.type = PACKET_DAMAGE_CONFIRMED_EVENT;
    event.header.tick = tick;
    event.header.playerId = attackerPlayerId;
    event.eventId = nextReliableGameplayEventId();
    event.eventSessionId = serverReliableEventSessionId();
    event.attackerPlayerId = attackerNpcId != 0 ? attackerNpcId : attackerPlayerId;
    event.attackerEntityType = attackerNpcId != 0 ? ENTITY_NPC : ENTITY_PLAYER;
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
    event.weaponDefNetworkId = weaponDefId.empty() ? 0 : weaponDefNetworkIdFor(weaponDefId);
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
    if (result.killed)
    {
        // NPC damage attribution: if the victim was recently damaged by an NPC,
        // attribute the kill to the NPC even if the final blow came from
        // self-damage (e.g. rocket splash). 120 ticks = 2 seconds window.
        uint32_t effectiveAttackerNpcId = attackerNpcId;
        uint32_t effectiveAttackerPlayerId = attackerPlayerId;
        if (effectiveAttackerNpcId == 0 && effectiveAttackerPlayerId != 0 &&
            target.lastNpcDamageSourceId != 0 &&
            (tick - target.lastNpcDamageTick) <= 120)
        {
            effectiveAttackerNpcId = target.lastNpcDamageSourceId;
            effectiveAttackerPlayerId = 0;
            DBG(Network,
                "NPC_KILL_REATTRIBUTION victim=%u originalAttacker=%u npcAttacker=%u "
                "npcDamageTick=%u currentTick=%u window=%u",
                target.id, attackerPlayerId, effectiveAttackerNpcId,
                target.lastNpcDamageTick, tick, tick - target.lastNpcDamageTick);
        }

        const char* weaponId = networkWeaponTypeName(weapon);
        std::string weaponDisplayName = weaponId;
        if (const WeaponDefinition* definition = WeaponRegistry::instance().get(weaponId))
            weaponDisplayName = definition->displayName.empty()
                ? definition->id : definition->displayName;
        if (effectiveAttackerNpcId != 0)
        {
            serverGamemodeOnNpcDeath(effectiveAttackerNpcId, target.id, weaponId,
                                     weaponDisplayName, event.eventId);
            event.attackerPlayerId = effectiveAttackerNpcId;
            event.attackerEntityType = ENTITY_NPC;
            DBG(Network,
                "KILL_EVENT_ENQUEUE type=NPC_KILLS_PLAYER killerNpcId=%u victimPlayerId=%u "
                "weaponId=\"%s\" weaponDisplay=\"%s\" eventId=%u tick=%u",
                effectiveAttackerNpcId, target.id,
                weaponId, weaponDisplayName.c_str(),
                event.eventId, tick);
        }
        else if (effectiveAttackerPlayerId != 0)
        {
            serverGamemodeOnPlayerDeath(effectiveAttackerPlayerId, target.id, weaponId,
                                        weaponDisplayName, event.eventId);
            DBG(Network,
                "KILL_EVENT_ENQUEUE type=PLAYER_KILLS_PLAYER killerPlayerId=%u victimPlayerId=%u "
                "weaponId=\"%s\" weaponDisplay=\"%s\" eventId=%u tick=%u",
                effectiveAttackerPlayerId, target.id,
                weaponId, weaponDisplayName.c_str(),
                event.eventId, tick);
        }
    }
    return queueReliableGameplayEventToAll(
        sock, players, &event, sizeof(event), event.eventId,
        event.eventSessionId, totalPacketsOut);
}

} // namespace MimitaNet

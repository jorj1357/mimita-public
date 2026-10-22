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
    case ServerDamageSource::Environment: return "environment";
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
    case ServerDamageSource::Environment: return DAMAGE_CONFIRMED_ENVIRONMENT;
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
    int attackerTeamOverride,
    bool attackerIsNpc,
    int damage,
    const glm::vec3& knockback,
    ServerDamageSource source)
{
    ServerDamageResult result;
    result.healthBefore = target.health;
    result.healthAfter = target.health;

    int attackerTeam = attackerTeamOverride;
    if (attackerTeam < 0)
    {
        const ServerGamemodeState& mode = serverGamemodeState();
        auto attackerIt = players.find(attackerPlayerId);
        attackerTeam = attackerIt != players.end()
            ? attackerIt->second.matchTeam : -1;
        if (attackerTeam < 0)
        {
            auto rosterIt = mode.matchTeams.find(attackerPlayerId);
            if (rosterIt != mode.matchTeams.end()) attackerTeam = rosterIt->second;
        }
    }
    const ServerGamemodeState& mode = serverGamemodeState();
    int targetTeam = target.matchTeam;
    if (targetTeam < 0)
    {
        auto rosterIt = mode.matchTeams.find(target.id);
        if (rosterIt != mode.matchTeams.end()) targetTeam = rosterIt->second;
    }

    if (target.dead || target.downed || damage <= 0)
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

    if (target.teleportInvulnerabilityTicks > 0)
    {
        target.health = target.maxHealth > 0 ? target.maxHealth : serverMaxHp();
        DBG(Network, "SERVER DAMAGE target=%u attacker=%u source=%s accepted=0 "
            "reason=teleport-invulnerability ticks=%u health=%d",
            target.id, attackerPlayerId, damageSourceName(source),
            (unsigned)target.teleportInvulnerabilityTicks, target.health);
        return result;
    }

    // Melee 60-tick spawn protection: block damage/knockback immediately after
    // a respawn, then end it on schedule (serverMeleeActorProtected reads the
    // authoritative server tick).
    if (serverMeleeActorProtected(target.id))
    {
        target.health = target.maxHealth > 0 ? target.maxHealth : serverMaxHp();
        DBG(Network, "SERVER DAMAGE target=%u attacker=%u source=%s accepted=0 "
            "reason=melee-spawn-protection health=%d",
            target.id, attackerPlayerId, damageSourceName(source), target.health);
        return result;
    }

    // Team-based friendly fire filtering: teammates cannot damage each other.
    // Use the gamemode roster as the authoritative fallback because a player
    // mirror can briefly have matchTeam == -1 while the roster already knows
    // the assignment. Self-damage remains allowed for rocket jumping.
    if (!serverFriendlyFireEnabled() && (attackerIsNpc || attackerPlayerId != target.id))
    {
        if (attackerTeam >= 0 && targetTeam >= 0 && attackerTeam == targetTeam)
        {
            DBG(Network, "SERVER DAMAGE target=%u attacker=%u source=%s accepted=0 "
                "reason=friendly-fire teams=%d damage=%d health=%d",
                target.id, attackerPlayerId, damageSourceName(source),
                targetTeam, damage, target.health);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "server.damage.rejected", std::to_string(target.id),
                "same-team damage blocked", 0,
                {{"attacker_id", attackerPlayerId},
                 {"attacker_type", attackerIsNpc ? "npc" : (attackerPlayerId == 0 ? "world" : "player")},
                 {"target_actor_id", target.id},
                 {"target_actor_type", "player"},
                 {"attacker_team", attackerTeam},
                 {"target_team", targetTeam},
                 {"source", damageSourceName(source)},
                 {"damage_requested", damage},
                 {"friendly_fire", false}} ,
                __FILE__, __LINE__, __FUNCTION__);
            return result;
        }
    }

    const int clampedDamage = std::clamp(damage, 1, 500);
    target.health = std::max(0, target.health - clampedDamage);
    target.vel += knockback;
    recordServerMovementExternalImpulse(target, knockback);
    result.applied = true;
    result.healthAfter = target.health;
    {
        ServerGamemodeState& mode = serverGamemodeState();
        if (mode.zombieTower && !attackerIsNpc && attackerPlayerId != target.id) {
            auto& stats = mode.ztStats[attackerPlayerId];
            stats.damageDealt += (uint64_t)clampedDamage;
            if (stats.name.empty()) {
                auto attackerIt = players.find(attackerPlayerId);
                if (attackerIt != players.end()) stats.name = attackerIt->second.name;
            }
        }
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "server.damage.applied", std::to_string(target.id),
        "authoritative server damage changed health", 0,
        { {"attacker_id", attackerPlayerId},
          {"attacker_type", attackerIsNpc ? "npc" : (attackerPlayerId == 0 ? "world" : "player")},
          {"attacker_team", attackerTeam}, {"target_actor_id", target.id},
          {"target_actor_type", "player"}, {"target_name", target.name},
          {"target_team", targetTeam},
          {"damage_requested", damage}, {"damage_applied", clampedDamage},
          {"health_before", result.healthBefore}, {"health_after", result.healthAfter},
          {"dead_before", false}, {"dead_after", target.health <= 0},
          {"killed", target.health <= 0}, {"source", damageSourceName(source)},
          {"spawn_generation", target.spawnGeneration},
          {"position", {target.pos.x, target.pos.y, target.pos.z}},
          {"knockback", {knockback.x, knockback.y, knockback.z}} },
        __FILE__, __LINE__, __FUNCTION__);

    if (target.health == 0)
    {
        ServerGamemodeState& mode = serverGamemodeState();
        int targetTeam = target.matchTeam;
        if (targetTeam < 0) {
            auto it = mode.matchTeams.find(target.id);
            if (it != mode.matchTeams.end()) targetTeam = it->second;
        }
        if (mode.zombieTower && targetTeam == 0 && !target.downed)
        {
            target.downed = true;
            target.downedHealth = 100.0f;
            target.reviveProgress = 0.0f;
            target.health = 0;
            target.respawnSeconds = -1.0f;
            target.vel = glm::vec3(0.0f);
            target.movement.movementEnabled = false;
            target.movement.baseVelocity = glm::vec3(0.0f);
            target.movement.externalImpulse = glm::vec3(0.0f);
            target.attackQueued = false;
            result.killed = false;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "zombie_tower.actor-downed", std::to_string(target.id),
                "human-team actor entered downed state", 0,
                {{"attacker_id", attackerPlayerId}, {"target_actor_id", target.id},
                 {"target_name", target.name}, {"target_team", targetTeam},
                 {"health_before", result.healthBefore}, {"source", damageSourceName(source)}},
                __FILE__, __LINE__, __FUNCTION__);
            return result;
        }
        target.dead = true;
        serverConsumeNpcWaveLife(target.id);
        // Honor the active gamemode's respawn rule. One-life modes set a
        // negative timer so the respawn pump never revives the actor and the
        // match state advances it to Spectating.
        const bool respawnAllowed = serverPlayerRespawnsEnabled(target.id);
        target.respawnSeconds = respawnAllowed ? serverMatchRespawnSeconds() : -1.0f;
        target.vel = glm::vec3(0.0f);
        target.movement.movementEnabled = false;
        target.movement.baseVelocity = glm::vec3(0.0f);
        target.movement.externalImpulse = glm::vec3(0.0f);
        target.attackQueued = false;
        target.deaths += 1;
        // Clear NPC damage tracking on death so it doesn't carry over to next life
        target.lastNpcDamageSourceId = 0;
        target.lastNpcDamageTick = 0;
        // Kill credit, heal, persistence, and the killfeed are owned by
        // serverGamemodeRecordKill so there is one authoritative kill owner.
        result.killed = true;

        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "server.death.transition", std::to_string(target.id),
            "authoritative server actor transitioned alive to dead", 0,
            { {"attacker_id", attackerPlayerId}, {"target_actor_id", target.id},
              {"target_name", target.name}, {"target_team", target.matchTeam},
              {"health_before", result.healthBefore}, {"health_after", result.healthAfter},
              {"damage_applied", clampedDamage}, {"source", damageSourceName(source)},
              {"spawn_generation", target.spawnGeneration},
              {"respawn_allowed", respawnAllowed},
              {"npc_waves", serverGamemodeState().npcWaves},
              {"wave", serverGamemodeState().waveNumber},
              {"wave_lives_remaining", serverGamemodeState().waveLivesRemaining},
              {"position", {target.pos.x, target.pos.y, target.pos.z}},
              {"respawn_seconds", target.respawnSeconds} },
            __FILE__, __LINE__, __FUNCTION__);
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
    if ((victim.kind == ServerActorKind::Player && !victim.player) ||
        (victim.kind == ServerActorKind::Npc && !victim.npc)) {
        result.rejectionReason = "victim-not-found";
        Debug::log(Debug::Category::Networking,
            "[ACTOR_DAMAGE_REJECTED] victim=%u kind=%u reason=%s event=%u\n",
            request.victim.id, (unsigned)request.victim.kind,
            result.rejectionReason.c_str(), request.eventId);
        return result;
    }

    const ServerActorRef attacker = findServerActor(
        request.attacker.id, request.attacker.kind, players, npcs);
    if ((request.attacker.kind == ServerActorKind::Player && !attacker.player) ||
        (request.attacker.kind == ServerActorKind::Npc && !attacker.npc)) {
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
    int attackerTeam = attacker.player ? attacker.player->matchTeam
        : attacker.npc->matchTeam;
    if (attackerTeam < 0)
    {
        const auto teamIt = serverGamemodeState().matchTeams.find(attacker.id);
        if (teamIt != serverGamemodeState().matchTeams.end())
            attackerTeam = teamIt->second;
    }

    if (victim.kind == ServerActorKind::Npc)
    {
        ServerNpc& target = *victim.npc;
        result.healthBefore = target.health;
        result.healthAfter = target.health;
        if (target.health <= 0 || request.damage <= 0)
        {
            result.rejectionReason = target.health <= 0 ? "target-dead" : "non-positive-damage";
            return result;
        }

        int targetTeam = target.matchTeam;
        if (targetTeam < 0)
        {
            const auto teamIt = serverGamemodeState().matchTeams.find(target.entityId);
            if (teamIt != serverGamemodeState().matchTeams.end())
                targetTeam = teamIt->second;
        }
        if (!serverFriendlyFireEnabled() &&
            (attacker.kind != ServerActorKind::Npc || attacker.id != target.entityId) &&
            attackerTeam >= 0 && targetTeam >= 0 && attackerTeam == targetTeam)
        {
            result.rejectionReason = "friendly-fire";
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "server.damage.rejected", std::to_string(target.entityId),
                "same-team actor damage blocked", request.serverTick,
                {{"attacker_id", attacker.id},
                 {"attacker_type", attacker.kind == ServerActorKind::Npc ? "npc" : "player"},
                 {"attacker_team", attackerTeam}, {"target_actor_id", target.entityId},
                 {"target_actor_type", "npc"}, {"target_team", targetTeam},
                 {"source", damageSourceName(request.source)},
                 {"damage_requested", request.damage}, {"friendly_fire", false}},
                __FILE__, __LINE__, __FUNCTION__);
            return result;
        }

        const int appliedDamage = std::clamp(request.damage, 1, 500);
        target.health = std::max(0, target.health - appliedDamage);
        target.knockbackImpulse += request.knockback;
        result.applied = true;
        result.healthAfter = target.health;
        result.killed = target.health == 0;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "server.damage.applied", std::to_string(target.entityId),
            "authoritative actor damage changed NPC health", request.serverTick,
            {{"attacker_id", attacker.id},
             {"attacker_type", attacker.kind == ServerActorKind::Npc ? "npc" : "player"},
             {"attacker_team", attackerTeam}, {"target_actor_id", target.entityId},
             {"target_actor_type", "npc"}, {"target_team", targetTeam},
             {"damage_requested", request.damage}, {"damage_applied", appliedDamage},
             {"health_before", result.healthBefore}, {"health_after", result.healthAfter},
             {"killed", result.killed}, {"source", damageSourceName(request.source)},
             {"knockback", {request.knockback.x, request.knockback.y, request.knockback.z}}},
            __FILE__, __LINE__, __FUNCTION__);
        result.eventId = request.eventId;
        result.correlationId = request.correlationId;
        return result;
    }

    result = applyPlayerDamageLegacy(players, *victim.player,
                                     attacker.id, attackerTeam,
                                     attacker.kind == ServerActorKind::Npc,
                                     request.damage,
                                     request.knockback, request.source);
    if (result.applied && !result.killed && attacker.kind == ServerActorKind::Npc) {
        victim.player->lastNpcDamageSourceId = attacker.id;
        victim.player->lastNpcDamageTick = request.serverTick;
    }
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
    return applyPlayerDamageLegacy(players, target, attackerPlayerId, -1, false,
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
    const std::string& weaponDefId,
    float contactForce)
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
    event.contactForce = contactForce;
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
        // attribute the kill to the NPC even if the final blow was ownerless or
        // self-inflicted (e.g. rocket splash). 120 ticks = 2 seconds window.
        uint32_t effectiveAttackerNpcId = attackerNpcId;
        uint32_t effectiveAttackerPlayerId = attackerPlayerId;
        const bool hasRealPlayerAttacker =
            effectiveAttackerPlayerId != 0 && effectiveAttackerPlayerId != target.id;
        if (effectiveAttackerNpcId == 0 && !hasRealPlayerAttacker &&
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
            serverGamemodeRecordKill(sock, players, nullptr,
                effectiveAttackerNpcId, ENTITY_NPC, target.id, ENTITY_PLAYER,
                weaponId, weaponDisplayName, event.eventId,
                hit, target.pos, tick, totalPacketsOut);
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
            glm::vec3 killerPos = target.pos;
            auto killerIt = players.find(effectiveAttackerPlayerId);
            if (killerIt != players.end()) killerPos = killerIt->second.pos;
            serverGamemodeRecordKill(sock, players, nullptr,
                effectiveAttackerPlayerId, ENTITY_PLAYER, target.id, ENTITY_PLAYER,
                weaponId, weaponDisplayName, event.eventId,
                killerPos, target.pos, tick, totalPacketsOut);
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

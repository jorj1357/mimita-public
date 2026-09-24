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
#include "network/server-context.h"
#include "network/server-gamemode.h"
#include "network/server-damage-policy.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-damage-application.h"
#include "hot-reload/hot-kill-attribution.h"
#include "ecs/components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "live-code/live-behavior.h"
#include "network/actor-health.h"
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

// Resolve the active generation's damage-application policy through the one
// generic doorway. Never cached across a generation swap. Null when no hot
// provider is registered.
static GameDamageApplicationFn hotDamageApplicationPolicy()
{
    void* callable = MimitaRuntime::GenericRuntime::instance().capability(
        GAME_CAP_DAMAGE_APPLICATION);
    return callable ? reinterpret_cast<GameDamageApplicationFn>(callable) : nullptr;
}

static GameKillAttributionFn hotKillAttributionPolicy()
{
    void* callable = MimitaRuntime::GenericRuntime::instance().capability(
        GAME_CAP_KILL_ATTRIBUTION);
    return callable ? reinterpret_cast<GameKillAttributionFn>(callable) : nullptr;
}

static ServerDamageResult applyPlayerDamageLegacy(
    std::unordered_map<uint32_t, ServerPlayer>& players,
    ServerPlayer& target,
    uint32_t attackerPlayerId,
    int damage,
    const glm::vec3& knockback,
    ServerDamageSource source,
    uint32_t tick = 0)
{
    ServerDamageResult result;
    result.healthBefore = target.health;
    result.healthAfter = target.health;

    // The accept/reject, friendly-fire, damage-limit, and death/respawn rules
    // are hot (net.damage-application); the cold path applies the decision.
    GameDamageApplicationV1 request{};
    request.structSize = sizeof(GameDamageApplicationV1);
    request.targetDead = target.dead ? 1u : 0u;
    request.targetConnectionStale = target.connectionStale ? 1u : 0u;
    request.attackerIsTarget = attackerPlayerId == target.id ? 1u : 0u;
    request.targetTeam = target.matchTeam;
    request.damage = damage;
    request.damageLimit = serverAuthoritativeDamageLimit();
    request.respawnsEnabled = serverMatchRespawnsEnabled() ? 1u : 0u;
    request.respawnSeconds = serverMatchRespawnSeconds();
    request.targetHealth = target.health;
    auto attackerIt = players.find(attackerPlayerId);
    request.attackerFound = attackerIt != players.end() ? 1u : 0u;
    request.attackerTeam = attackerIt != players.end() ? attackerIt->second.matchTeam : -1;
    // v2 identity/outcome facts: explicit suicide (attacker == victim) and the
    // scoring eligibility decision live in the hot policy.
    request.version = 2u;
    request.outVictimEntity = target.id;
    request.outAttackerEntity = attackerPlayerId;
    request.outDeathReason = static_cast<std::uint32_t>(source);

    GameDamageApplicationFn policy = hotDamageApplicationPolicy();
    if (policy)
        policy(nullptr, &request);
    else
        HotDamageApplicationImpl::evaluate(request);

    if (!request.result || !request.accept)
    {
        const char* reason =
            request.rejectReason == GAME_DAMAGE_REJECT_TARGET_DEAD ? "target-dead" :
            request.rejectReason == GAME_DAMAGE_REJECT_NON_POSITIVE ? "non-positive-damage" :
            request.rejectReason == GAME_DAMAGE_REJECT_TARGET_RECONNECTING ? "target-reconnecting" :
            request.rejectReason == GAME_DAMAGE_REJECT_FRIENDLY_FIRE ? "friendly-fire" :
            "rejected";
        DBG(Network, "SERVER DAMAGE target=%u attacker=%u source=%s accepted=0 "
            "reason=%s damage=%d health=%d",
            target.id, attackerPlayerId, damageSourceName(source), reason,
            damage, target.health);
        return result;
    }

    target.health = request.healthAfter;
    target.vel += knockback;
    recordServerMovementExternalImpulse(target, knockback);
    result.applied = true;
    result.healthAfter = target.health;
    result.suicide = request.outSuicide != 0u;
    result.scoreEligible = request.outScoreEligible != 0u;

    if (request.killed)
    {
        target.dead = true;
        // Honor the active gamemode's respawn rule. One-life modes set a
        // negative timer so the respawn pump never revives the actor and the
        // match state advances it to Spectating.
        target.respawnSeconds = request.outRespawnSeconds;
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
    }

    DBG(Network, "SERVER DAMAGE target=%u attacker=%u source=%s damage=%d "
        "healthBefore=%d healthAfter=%d killed=%d knockback=(%.2f,%.2f,%.2f)",
        target.id, attackerPlayerId, damageSourceName(source), request.clampedDamage,
        result.healthBefore, result.healthAfter, (int)result.killed,
        knockback.x, knockback.y, knockback.z);

    // Generic actor.damage fact through the existing event system. The payload is
    // the resolved hot outcome; an unregistered event type is a safe no-op.
    {
        GameDamageApplicationV1 fact = request;
        fact.tick = tick;
        LiveBehavior::dispatchGameplayEvent64(
            gameHash("actor.damage"), &fact, sizeof(fact), tick,
            fact.outAttackerEntity, fact.outVictimEntity);
    }
    if (request.killed)
    {
        ActorLifecycleStateV1 life{};
        life.entityId = target.id;
        life.actorKind = 1u;  // player
        life.dead = 1u;
        life.respawnRequested = request.outRespawnSeconds >= 0.0f ? 1u : 0u;
        life.reason = static_cast<std::uint32_t>(source);
        LiveBehavior::dispatchGameplayEvent64(
            gameHash("actor.respawn_requested"), &life, sizeof(life), tick,
            0, target.id);
    }
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
                                     request.knockback, request.source,
                                     request.serverTick);
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

// Generic authoritative damage from a package: the caller supplies victim and
// source entity ids and a generic damage source; the kernel maps the entities
// to actors and applies damage through the shared actor damage boundary. Raw
// containers stay private.
bool serverApplyEntityDamage(GameDamageApplyV1& request)
{
    ServerContextV1* context = activeServerContext();
    if (!context || !context->players || !context->npcs)
        return false;
    auto& players =
        *static_cast<std::unordered_map<uint32_t, ServerPlayer>*>(context->players);
    auto& npcs =
        *static_cast<std::unordered_map<uint32_t, ServerNpc>*>(context->npcs);

    const EntityId victimEntity = static_cast<EntityId>(request.victimEntity);
    if (victimEntity == kInvalidEntityId)
        return false;
    const EntityDomain victimDomain = entityDomain(victimEntity);
    const bool victimIsNpc = victimDomain == EntityDomain::Npc;

    // Generic damageable entity (world object, destructible, runtime monster):
    // authoritative health is the generic ActorHealthState dynamic component
    // when present (replicated), otherwise the typed HealthComponent.
    if (victimDomain != EntityDomain::Player && !victimIsNpc) {
        if (actorHealthHas(victimEntity)) {
            std::int32_t after = 0;
            bool dead = false;
            if (!actorHealthApplyDamage(victimEntity, request.amount, &after, &dead))
                return false;
            if (auto* health = EntityRegistry::instance().tryGet<HealthComponent>(victimEntity)) {
                health->current = after;
                health->dead = dead;
            }
            request.applied = 1;
            request.killed = dead ? 1u : 0u;
            request.healthAfter = after;
            return true;
        }
        if (auto* health =
                EntityRegistry::instance().tryGet<HealthComponent>(victimEntity)) {
            const int amount = request.amount > 0 ? request.amount : 1;
            health->current = std::max(0, health->current - amount);
            health->dead = health->current <= 0;
            request.applied = 1;
            request.killed = health->dead ? 1u : 0u;
            request.healthAfter = health->current;
            return true;
        }
        return false;
    }

    ServerActorDamageRequest damageRequest;
    damageRequest.victim = findServerActor(
        entityLegacyId(victimEntity),
        victimIsNpc ? ServerActorKind::Npc : ServerActorKind::Player, players, npcs);
    if (!damageRequest.victim.player && !damageRequest.victim.npc)
        return false;

    damageRequest.damage = request.amount;
    damageRequest.knockback =
        glm::vec3(request.knockback[0], request.knockback[1], request.knockback[2]);
    // Map the generic damage source onto the shared server damage vocabulary.
    switch (request.sourceKind) {
    case GAME_DAMAGE_SOURCE_HITSCAN: damageRequest.source = ServerDamageSource::Hitscan; break;
    case GAME_DAMAGE_SOURCE_MELEE: damageRequest.source = ServerDamageSource::Melee; break;
    case GAME_DAMAGE_SOURCE_CONTACT: damageRequest.source = ServerDamageSource::PhysicalContact; break;
    default: damageRequest.source = ServerDamageSource::RocketExplosion; break;
    }
    if (context->tick)
        damageRequest.serverTick = *context->tick;

    const EntityId sourceEntity = static_cast<EntityId>(request.sourceEntity);
    bool havePlayerAttacker = false;
    uint32_t attackerPlayerId = 0;
    if (sourceEntity != kInvalidEntityId) {
        const bool sourceIsNpc = entityDomain(sourceEntity) == EntityDomain::Npc;
        damageRequest.attacker = findServerActor(
            entityLegacyId(sourceEntity),
            sourceIsNpc ? ServerActorKind::Npc : ServerActorKind::Player, players, npcs);
        havePlayerAttacker = damageRequest.attacker.player != nullptr;
        if (havePlayerAttacker)
            attackerPlayerId = damageRequest.attacker.id;
    }

    ServerDamageResult result;
    if (!victimIsNpc && havePlayerAttacker) {
        // Full shared actor damage boundary (player attacker + player victim).
        result = applyActorDamage(players, npcs, damageRequest);
    } else if (!victimIsNpc) {
        // No player attacker: apply to the player victim directly (self/world
        // damage), which is also the phase-1 generic damage path.
        result = applyPlayerDamageLegacy(players, *damageRequest.victim.player,
                                         attackerPlayerId, damageRequest.damage,
                                         damageRequest.knockback, damageRequest.source,
                                         damageRequest.serverTick);
    } else {
        // NPC victim: generic ActorHealthState on the NPC entity is the
        // authoritative store. The ServerNpc mirror and the typed
        // HealthComponent are projections (bridges), not owners. No separate
        // damageNpc capability or NPC-specific packet is introduced.
        auto nit = npcs.find(entityLegacyId(victimEntity));
        if (nit == npcs.end()) {
            request.applied = 0;
            request.killed = 0;
            request.healthAfter = 0;
            return false;
        }
        if (!actorHealthHas(victimEntity))
            actorHealthInit(victimEntity,
                            nit->second.health > 0 ? nit->second.health : 100);
        std::int32_t before = 0;
        bool wasDead = false;
        actorHealthRead(victimEntity, &before, nullptr, &wasDead);
        std::int32_t after = 0;
        bool dead = false;
        if (!actorHealthApplyDamage(victimEntity, request.amount, &after, &dead)) {
            request.applied = 0;
            return false;
        }
        // Projections (bridges).
        nit->second.health = after;
        if (auto* health = EntityRegistry::instance().tryGet<HealthComponent>(victimEntity)) {
            health->current = after;
            health->dead = dead;
        }
        request.applied = 1;
        request.killed = dead ? 1u : 0u;
        request.healthAfter = after;

        // Generic death fact: one runtime event, no NPC-specific callback.
        if (dead && !wasDead) {
            GameActorKilledV1 killedEvent{};
            killedEvent.victimEntity = request.victimEntity;
            killedEvent.victimId = entityLegacyId(victimEntity);
            killedEvent.victimIsNpc = 1;
            killedEvent.killerId = attackerPlayerId;
            killedEvent.tick = context->tick ? *context->tick : 0;
            LiveBehavior::dispatchActorKilled(killedEvent, killedEvent.tick);
            if (context->sock && context->totalPacketsOut && context->tick) {
                serverGamemodeRecordKill(
                    static_cast<SOCKET>(context->sock), players, &npcs,
                    attackerPlayerId, ENTITY_PLAYER, nit->second.entityId, ENTITY_NPC,
                    "", "", 0, glm::vec3(0.0f), nit->second.pos, *context->tick,
                    *context->totalPacketsOut);
            }
        }
        return true;
    }
    request.applied = result.applied ? 1u : 0u;
    request.killed = result.killed ? 1u : 0u;
    request.healthAfter = result.healthAfter;
    return result.applied;
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
        // attribute the kill to the NPC even if the final blow was ownerless or
        // self-inflicted (e.g. rocket splash). 120 ticks = 2 seconds window.
        // The reattribution window and decision are hot (net.kill-attribution).
        GameKillAttributionV1 attribution{};
        attribution.structSize = sizeof(GameKillAttributionV1);
        attribution.attackerNpcId = attackerNpcId;
        attribution.attackerPlayerId = attackerPlayerId;
        attribution.victimId = target.id;
        attribution.lastNpcDamageSourceId = target.lastNpcDamageSourceId;
        attribution.lastNpcDamageTick = target.lastNpcDamageTick;
        attribution.tick = tick;
        attribution.windowTicks = 120;
        GameKillAttributionFn attributionPolicy = hotKillAttributionPolicy();
        if (attributionPolicy)
            attributionPolicy(nullptr, &attribution);
        else
            HotKillAttributionImpl::evaluate(attribution);
        uint32_t effectiveAttackerNpcId = attribution.outAttackerNpcId;
        uint32_t effectiveAttackerPlayerId = attribution.outAttackerPlayerId;
        if (attribution.reattributed)
        {
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
        if (result.suicide)
        {
            // Explicit suicide (attacker == victim): the death is a fact, but it
            // has no scoring path. Dispatch the generic actor.killed fact with
            // killer == victim and skip serverGamemodeRecordKill entirely.
            GameActorKilledV1 suicideEvent{};
            suicideEvent.killerEntity = target.id;
            suicideEvent.victimEntity = target.id;
            suicideEvent.killerId = target.id;
            suicideEvent.victimId = target.id;
            suicideEvent.killerIsNpc = 0;
            suicideEvent.victimIsNpc = 0;
            suicideEvent.weaponNetworkId = weapon;
            suicideEvent.tick = tick;
            LiveBehavior::dispatchActorKilled(suicideEvent, tick);
            DBG(Network,
                "KILL_EVENT_ENQUEUE type=SUICIDE victimPlayerId=%u weaponId=\"%s\" "
                "weaponDisplay=\"%s\" eventId=%u tick=%u",
                target.id, weaponId, weaponDisplayName.c_str(), event.eventId, tick);
        }
        else if (effectiveAttackerNpcId != 0)
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

        debug::Event deathEvent{
            .category = "NETWORK",
            .name = "server.player_died",
            .level = debug::Level::Info,
            .message = "authoritative player death confirmed",
            .reason = damageSourceName(source),
            .correlationId = std::to_string(event.eventId),
            .fields = {
                {"victim_player_id", target.id}, {"victim_entity_id", target.id},
                {"victim_name", target.name}, {"killer_player_id", effectiveAttackerPlayerId},
                {"killer_npc_id", effectiveAttackerNpcId},
                {"killer_entity_type", effectiveAttackerNpcId != 0 ? "npc" : "player_or_world"},
                {"weapon", weaponDisplayName}, {"damage", damage},
                {"health_before", result.healthBefore}, {"health_after", result.healthAfter},
                {"server_tick", tick}, {"spawn_generation", target.spawnGeneration},
                {"deaths_total", target.deaths}, {"respawn_seconds", target.respawnSeconds},
                {"respawns_enabled", target.respawnSeconds >= 0.0f},
                {"connection_stale", target.connectionStale}
            },
            .serverTick = tick,
            .aggregationKey = "server.player.death"};
        MIMITA_EVENT(deathEvent);
    }
    return queueReliableGameplayEventToAll(
        sock, players, &event, sizeof(event), event.eventId,
        event.eventSessionId, totalPacketsOut);
}

} // namespace MimitaNet

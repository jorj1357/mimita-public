// 09 22 2026
/* purpose
* ONE shared hot source of truth for weapon damage consequences: turns a
* completed trace or a list of victims into authoritative damage + replication
* events (DamageConfirmed / NPC damage broadcast, kills) with the exact packet
* contents. Hot behaviors call these directly; cold fallbacks (a cold trace,
* the damage.resolve primitive) call them too, so cold and hot run the same
* logic — a consequence bug is a hot (game DLL) fix.
* Does NOT own the trace, weapon values, or simulation.
*/
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "hot-reload/game-api.h"
#include "hot-reload/hot-damage-event.h"
#include "hot-reload/hot-event-broadcast.h"
#include "hot-reload/hot-projectile-event.h"
#include "network/packets.h"
#include "combat/weapon-execution.h"
#include "combat/weapon-types.h"

namespace HotConsequences {

// ── Generic per-victim and packet primitives ─────────────────────────

inline std::uint32_t ownerPlayerId(std::uint64_t ownerEntity)
{
    return hotOwnerPlayerId(ownerEntity);
}

// Apply one authoritative damage fact (policy already resolved) and emit its
// replication event. No second policy pass.
inline void deliverVictim(GameplayContextV1* ctx, std::uint64_t attacker,
                          std::uint64_t victim, std::uint32_t spawnGeneration,
                          std::uint32_t sourceKind, std::int32_t damage,
                          const float knockback[3], const float hit[3],
                          const float normal[3], std::uint32_t causeSerial,
                          std::uint32_t projectileId, std::uint32_t weaponNetworkId,
                          std::uint32_t weaponDefNetworkId)
{
    GameDamageEventV1 event{};
    event.attackerEntity = attacker;
    event.victimEntity = victim;
    event.victimSpawnGeneration = spawnGeneration;
    event.sourceKind = sourceKind;
    event.damage = damage;
    event.causeSerial = causeSerial;
    event.projectileId = projectileId;
    event.weaponNetworkId = weaponNetworkId;
    event.weaponDefNetworkId = weaponDefNetworkId;
    event.knockback[0] = knockback[0];
    event.knockback[1] = knockback[1];
    event.knockback[2] = knockback[2];
    event.hitPosition[0] = hit[0];
    event.hitPosition[1] = hit[1];
    event.hitPosition[2] = hit[2];
    event.hitNormal[0] = normal[0];
    event.hitNormal[1] = normal[1];
    event.hitNormal[2] = normal[2];
    hotApplyDamageEvent(ctx, event);
}

// Resolve policy then apply one victim and emit its replication event.
inline void applyVictim(GameplayContextV1* ctx, std::uint64_t attacker,
                        std::uint64_t victim, std::uint32_t spawnGeneration,
                        std::uint32_t sourceKind, std::int32_t damage,
                        const float knockback[3], const float hit[3],
                        const float normal[3], std::uint32_t causeSerial,
                        std::uint32_t projectileId, std::uint32_t weaponNetworkId,
                        std::uint32_t weaponDefNetworkId)
{
    GameDamagePolicyV1 policy{};
    policy.sourceKind = sourceKind;
    policy.attackerEntity = attacker;
    policy.victimEntity = victim;
    policy.weaponNetworkId = weaponNetworkId;
    policy.victimIsNpc = 0;
    policy.tick = ctx ? (std::uint32_t)ctx->tick : 0;
    policy.baseDamage = damage;
    policy.knockback[0] = knockback[0];
    policy.knockback[1] = knockback[1];
    policy.knockback[2] = knockback[2];
    hotResolveDamagePolicy(ctx, policy);
    const float kb[3] = {policy.knockback[0], policy.knockback[1], policy.knockback[2]};
    deliverVictim(ctx, attacker, victim, spawnGeneration, sourceKind,
                  policy.outDamage, kb, hit, normal, causeSerial, projectileId,
                  weaponNetworkId, weaponDefNetworkId);
}

// ── Hitscan outcome (packet contents owned here) ─────────────────────

struct HitscanVictim {
    std::uint64_t entity = 0;
    std::uint32_t spawnGeneration = 0;
    std::uint32_t pelletHits = 0;
    std::int32_t damage = 0;
    bool headshot = false;
    float knockback[3] = {0.0f, 0.0f, 0.0f};
    float hitPosition[3] = {0.0f, 0.0f, 0.0f};
    float hitNormal[3] = {0.0f, 0.0f, 1.0f};
};

// Applies damage + events + shot visuals for a completed hitscan trace and
// returns the hit verdict (HIT_VERDICT_*).
inline std::uint32_t resolveHitscan(
    GameplayContextV1* ctx,
    std::uint64_t attacker,
    const WeaponDefinition& def,
    std::uint32_t weaponNetworkId,
    std::uint32_t weaponDefNetworkId,
    std::uint32_t pelletCount,
    std::uint32_t requestId,
    std::uint32_t clientSimulationTick,
    std::uint32_t claimedTargetId,
    const float origin[3],
    const float direction[3],
    const float worldHit[3],
    const float worldNormal[3],
    float maxRange,
    float worldBlockDistance,
    const std::vector<HitscanVictim>& victims)
{
    for (const HitscanVictim& v : victims)
    {
        const std::uint32_t sourceKind = GAME_DAMAGE_SOURCE_HITSCAN;
        applyVictim(ctx, attacker, v.entity, v.spawnGeneration, sourceKind, v.damage,
                    v.knockback, v.hitPosition, v.hitNormal, requestId, 0,
                    weaponNetworkId, weaponDefNetworkId);
    }

    const bool localPresent =
        ctx && ctx->permanentStorage &&
        ctx->permanentStorageSize >= sizeof(GameSharedStateV1) &&
        reinterpret_cast<const GameSharedStateV1*>(ctx->permanentStorage)->magic ==
            GAME_SHARED_MAGIC &&
        reinterpret_cast<const GameSharedStateV1*>(ctx->permanentStorage)->localPlayerEntity != 0;

    if (pelletCount <= 1)
    {
        float hitPos[3] = {origin[0] + direction[0] * maxRange,
                           origin[1] + direction[1] * maxRange,
                           origin[2] + direction[2] * maxRange};
        float hitNml[3] = {-direction[0], -direction[1], -direction[2]};
        std::uint32_t hitTarget = 0;
        if (!victims.empty())
        {
            hitPos[0] = victims[0].hitPosition[0];
            hitPos[1] = victims[0].hitPosition[1];
            hitPos[2] = victims[0].hitPosition[2];
            hitNml[0] = victims[0].hitNormal[0];
            hitNml[1] = victims[0].hitNormal[1];
            hitNml[2] = victims[0].hitNormal[2];
            hitTarget = (std::uint32_t)victims[0].entity;
        }
        else
        {
            const float dx = worldHit[0] - origin[0];
            const float dy = worldHit[1] - origin[1];
            const float dz = worldHit[2] - origin[2];
            if (dx * dx + dy * dy + dz * dz < maxRange * maxRange)
            {
                hitPos[0] = worldHit[0];
                hitPos[1] = worldHit[1];
                hitPos[2] = worldHit[2];
                hitNml[0] = worldNormal[0];
                hitNml[1] = worldNormal[1];
                hitNml[2] = worldNormal[2];
            }
        }

        std::uint16_t effectFlags = MimitaNet::SHOT_EFFECT_MUZZLE | MimitaNet::SHOT_EFFECT_TRACER |
            MimitaNet::SHOT_EFFECT_SHOOT_SOUND | MimitaNet::SHOT_EFFECT_WEAPON_TRIGGER;
        std::uint8_t impactType = MimitaNet::SHOT_IMPACT_NONE;
        if (hitTarget != 0)
        {
            impactType = MimitaNet::SHOT_IMPACT_ENTITY;
            effectFlags |= MimitaNet::SHOT_EFFECT_ENTITY_IMPACT | MimitaNet::SHOT_EFFECT_BLOOD | MimitaNet::SHOT_EFFECT_HIT_SOUND;
        }
        else
        {
            const float dx = hitPos[0] - origin[0];
            const float dy = hitPos[1] - origin[1];
            const float dz = hitPos[2] - origin[2];
            if (dx * dx + dy * dy + dz * dz < (maxRange - 0.1f) * (maxRange - 0.1f))
            {
                impactType = MimitaNet::SHOT_IMPACT_WORLD;
                effectFlags |= MimitaNet::SHOT_EFFECT_WORLD_IMPACT | MimitaNet::SHOT_EFFECT_DEBRIS | MimitaNet::SHOT_EFFECT_HIT_SOUND;
            }
        }

        if (localPresent)
        {
            MimitaNet::ShotEventPacket shotEvent{};
            shotEvent.header.type = MimitaNet::PACKET_SHOT_EVENT;
            shotEvent.header.tick = ctx ? (std::uint32_t)ctx->tick : 0;
            shotEvent.header.playerId = ownerPlayerId(attacker);
            shotEvent.eventId = 0;
            shotEvent.shotSerial = requestId;
            shotEvent.clientTimeMs = clientSimulationTick;
            shotEvent.shooterPlayerId = ownerPlayerId(attacker);
            shotEvent.targetPlayerId = hitTarget;
            shotEvent.weapon = (std::uint8_t)weaponNetworkId;
            shotEvent.impactType = impactType;
            shotEvent.effectFlags = effectFlags;
            shotEvent.originX = origin[0];
            shotEvent.originY = origin[1];
            shotEvent.originZ = origin[2];
            shotEvent.hitX = hitPos[0];
            shotEvent.hitY = hitPos[1];
            shotEvent.hitZ = hitPos[2];
            shotEvent.dirX = direction[0];
            shotEvent.dirY = direction[1];
            shotEvent.dirZ = direction[2];
            shotEvent.normalX = hitNml[0];
            shotEvent.normalY = hitNml[1];
            shotEvent.normalZ = hitNml[2];
            shotEvent.beamEndX = origin[0] + direction[0] * maxRange;
            shotEvent.beamEndY = origin[1] + direction[1] * maxRange;
            shotEvent.beamEndZ = origin[2] + direction[2] * maxRange;
            if (hitTarget != 0 && !victims.empty())
            {
                shotEvent.damage = victims[0].damage;
                shotEvent.damageConfirmed = 1;
            }
            hotBroadcastPacket(ctx, &shotEvent, (std::uint32_t)sizeof(shotEvent), 0);
        }
    }
    else
    {
        MimitaNet::PelletBlastEventPacket blastEvent{};
        blastEvent.header.type = MimitaNet::PACKET_PELLET_BLAST_EVENT;
        blastEvent.header.tick = ctx ? (std::uint32_t)ctx->tick : 0;
        blastEvent.shooterPlayerId = ownerPlayerId(attacker);
        blastEvent.shotSerial = requestId;
        blastEvent.clientTimeMs = clientSimulationTick;
        blastEvent.originX = origin[0];
        blastEvent.originY = origin[1];
        blastEvent.originZ = origin[2];
        blastEvent.baseDirX = direction[0];
        blastEvent.baseDirY = direction[1];
        blastEvent.baseDirZ = direction[2];
        blastEvent.weapon = (std::uint8_t)weaponNetworkId;
        blastEvent.pelletCount = (std::uint8_t)std::min(pelletCount, (std::uint32_t)MimitaNet::MAX_NETWORK_PELLETS);
        blastEvent.maxRange = maxRange;
        blastEvent.beamEndX = origin[0] + direction[0] * maxRange;
        blastEvent.beamEndY = origin[1] + direction[1] * maxRange;
        blastEvent.beamEndZ = origin[2] + direction[2] * maxRange;

        // Victim aggregates (presentation only; damage already applied).
        blastEvent.targetCount = 0;
        for (const HitscanVictim& v : victims)
        {
            if (blastEvent.targetCount >= MimitaNet::MAX_PELLET_BLAST_TARGETS)
                break;
            MimitaNet::PelletBlastTargetResult& t =
                blastEvent.targets[blastEvent.targetCount++];
            t.targetPlayerId = (std::uint32_t)v.entity;
            t.totalDamage = (int16_t)v.damage;
            t.pelletsHit = (std::uint8_t)v.pelletHits;
            t.targetSpawnGeneration = v.spawnGeneration;
        }
        if (localPresent)
            hotBroadcastPacket(ctx, &blastEvent, (std::uint32_t)sizeof(blastEvent), 0);
    }

    std::uint32_t hitVerdict = MimitaNet::HIT_VERDICT_MISS;
    if (!victims.empty())
    {
        bool hitClaimed = false;
        for (const HitscanVictim& v : victims)
        {
            if (claimedTargetId != 0 && (std::uint32_t)v.entity == claimedTargetId)
            {
                hitClaimed = true;
                break;
            }
        }
        hitVerdict = hitClaimed ? MimitaNet::HIT_VERDICT_HIT_CLAIMED_TARGET
                                : MimitaNet::HIT_VERDICT_HIT_OTHER_TARGET;
    }
    (void)def;
    (void)weaponDefNetworkId;
    (void)worldBlockDistance;
    return hitVerdict;
}

// Convenience overload from WeaponExecution trace types.
inline std::uint32_t resolveHitscanTrace(
    GameplayContextV1* ctx,
    std::uint64_t attacker,
    const WeaponDefinition& def,
    std::uint32_t weaponNetworkId,
    std::uint32_t weaponDefNetworkId,
    const WeaponExecution::HitscanTraceResult& trace,
    std::uint32_t requestId,
    std::uint32_t clientSimulationTick,
    std::uint32_t claimedTargetId,
    const float origin[3],
    const float direction[3],
    const float worldHit[3],
    const float worldNormal[3],
    float maxRange,
    float worldBlockDistance)
{
    std::vector<HitscanVictim> victims;
    victims.reserve(trace.aggregates.size());
    for (const auto& agg : trace.aggregates)
    {
        HitscanVictim v;
        v.entity = agg.targetPlayerId;
        v.spawnGeneration = agg.targetSpawnGeneration;
        v.pelletHits = (std::uint32_t)agg.pelletHits;
        v.damage = agg.damage;
        v.headshot = agg.headshot;
        v.knockback[0] = agg.knockback.x;
        v.knockback[1] = agg.knockback.y;
        v.knockback[2] = agg.knockback.z;
        v.hitPosition[0] = agg.hitPosition.x;
        v.hitPosition[1] = agg.hitPosition.y;
        v.hitPosition[2] = agg.hitPosition.z;
        v.hitNormal[0] = agg.hitNormal.x;
        v.hitNormal[1] = agg.hitNormal.y;
        v.hitNormal[2] = agg.hitNormal.z;
        victims.push_back(v);
    }
    return resolveHitscan(ctx, attacker, def, weaponNetworkId, weaponDefNetworkId,
                          (std::uint32_t)trace.pelletCount, requestId,
                          clientSimulationTick, claimedTargetId, origin, direction,
                          worldHit, worldNormal, maxRange, worldBlockDistance,
                          victims);
}

} // namespace HotConsequences

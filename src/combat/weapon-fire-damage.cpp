// 07 31 2026, 14 50
/* purpose
* Applies local hitscan damage, knockback, hit effects, and world-impact visuals.
* Handles single-ray and multi-pellet hit processing shared by local prediction.
* Does NOT own server weapon authority, packet send/receive, or damage validation.
* Does NOT run the fixed-step simulation tick or render viewmodels.
*/
#include "weapon-fire.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "audio/audio.h"
#include "combat/weapon-audio.h"
#include "combat/weapon-execution.h"
#include "config/networking-config.h"
#include "config/player-settings.h"
#include "config/weapon-hitfx-config.h"
#include "debug/debug-log.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/death-ghost.h"
#include "entities/player.h"
#include "network/multiplayer-context.h"
#include "npc/npc.h"
#include "ui/hitmarker.h"

extern MimitaNet::MultiplayerContext* gpMpContext;

namespace WeaponFire {

static void fireSound(const WeaponDefinition& def, const glm::vec3& muzzlePos)
{
    WeaponAudio::playShootSound(def, muzzlePos);
}

// Config-driven victim knockback. baseImpulse is the damage-scaled impulse
// magnitude; direction comes from the shot. Vertical fraction and enemy
// multiplier come from the weapon config so knockback tuning is hot-reloadable.
static glm::vec3 victimKnockbackImpulse(const WeaponDefinition& def,
                                        const glm::vec3& shotDirection,
                                        float baseImpulse)
{
    glm::vec3 kb = shotDirection * baseImpulse * def.enemyImpulseMultiplier;
    kb.z += baseImpulse * def.victimKnockbackVerticalFraction;
    return kb;
}

// Predicted remote kill: show the death instantly on the local replica without
// DeathSystem::kill's side effects (killfeed, heal, replay, duel tracking) —
// those stay server-confirmed so they never double-fire. The server's
// confirmed damage event remains authoritative and reconciles this (clear the
// flag on agreement, or revive + disagreement on rollback).
static void predictRemoteKill(Player& victim,
                              const glm::vec3& direction,
                              const std::string& actorType,
                              uint32_t ownerId)
{
    if (victim.dead || victim.netPredictedDead)
        return;
    victim.netPredictedDead = true;

    // Spawn the fall-over death visual as a SEPARATE clone; the remote body
    // itself is never pinned or frozen, so it can never stick at a spot.
    if (!victim.networkDeathPresented)
    {
        victim.networkDeathPresented = true;
        DeathGhostSystem::instance().spawnFromPlayer(
            victim, direction, actorType, ownerId);
    }

    victim.vel = glm::vec3(0.0f);
    victim.externalImpulse = glm::vec3(0.0f);
    victim.inputWishMove = glm::vec2(0.0f);
    victim.currentHp = 0;
    victim.dead = true;
    victim.respawnTimer = 0.0f;

    if (actorType == "npc")
    {
        AudioManager::instance().play(
            {"npc_death", AudioCategory::NPC, true, victim.pos, 1.0f, 0.9f, 45.0f, 0});
    }

    Debug::log(Debug::Category::Networking,
               "[NET PREDICTED KILL] victim=%s type=%s pos=(%.2f,%.2f,%.2f)",
               victim.username.c_str(), actorType.c_str(),
               victim.pos.x, victim.pos.y, victim.pos.z);
}

// Shared predicted hit presentation for remote targets: instant hitmarker,
// blood/impact FX, and hit sound. Mirrors the server damage model so the
// predicted damage number matches what the server will confirm.
static void presentRemoteHit(const WeaponDefinition& def,
                             const glm::vec3& hitEnd,
                             const glm::vec3& hitNormal,
                             const glm::vec3& shotDirection,
                             float nearest,
                             const std::string& hitPart,
                             const std::string& shooterName,
                             const std::string& victimName,
                             int damage)
{
    hitmarker(damage);
    if (GetPlayerSettings().debugCombat)
        Debug::log(Debug::Category::Weapons,
            "[HITMARKER] attacker=%s victim=%s show=1 reason=local_predicted_remote",
            shooterName.c_str(), victimName.c_str());
    {
        HitEvent ev;
        ev.position = hitEnd;
        ev.normal = hitNormal;
        ev.direction = shotDirection;
        ev.hitEntity = true;
        ev.damage = damage;
        ev.hitDistance = nearest;
        ev.attacker = shooterName;
        ev.victim = victimName;
        ev.weaponSource = def.id;
        HitEffects::onHit(ev);
    }
    {
        float dist = glm::length(hitEnd - audioListenerPosition());
        float angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, hitNormal)), 0.15f, 1.0f);
        float headMul = (hitPart == "head") ? 2.0f : 1.0f;
        float severity = std::clamp(angleFactor * ((float)damage / 100.0f) * headMul, 0.0f, 1.0f);
        float vol, pit;
        const auto& sndCfg = WeaponHitFxConfig::instance().soundFor(def.id);
        computeImpactAudio(sndCfg.baseVolume, dist, severity, vol, pit);
        playWorldSound(def.soundHit, hitEnd, vol, pit, 60.0f);
        Debug::log(Debug::Category::Audio, "[HIT AUDIO] event=%s dist=%.1f damage=%d severity=%.2f pitch=%.2f volume=%.2f\n",
                   def.soundHit.c_str(), dist, damage, severity, pit, vol);
    }
}

// Shared predicted damage model (headshot/limb multipliers + range falloff)
// so client prediction matches the server trace damage.
static int predictedRemoteDamage(const WeaponDefinition& def,
                                 const std::string& hitPart,
                                 float nearest)
{
    return WeaponExecution::computeHitscanDamage(def, hitPart, nearest, 1.0f);
}

void processNpcHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    Npc& victim,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& shotDirection,
    float nearest,
    Player& shooter,
    NpcSystem& npcs,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir)
{
    DamageContext ctx;
    ctx.baseDamage = def.damage;
    ctx.distance = nearest;
    ctx.angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, hitNormal)), 0.15f, 1.0f);
    ctx.bodyPart = hitPart;
    ctx.hitPosition = hitEnd;
    ctx.hitNormal = hitNormal;
    ctx.shotDirection = shotDirection;
    ctx.shooterId = 0;
    ctx.shooterName = shooter.username;

    (void)muzzleDir;
    (void)muzzlePos;

    int totalDamage = applyDamageToEntity(ctx, victim, def, shooter, npcs, muzzlePos, shotDirection);

    result.hitEntity = true;
    result.bodyPart = hitPart;
    result.damage = (float)totalDamage;
    result.targetId = victim.id;
    {
        float df = WeaponExecution::hitscanFalloffFactor(def, nearest);
        const float kbScale = def.victimKnockbackPerDamage / 0.15f;
        const float kn =
            (def.victimKnockback +
             (float)totalDamage * (0.08f + ctx.angleFactor * 0.12f) * kbScale) * df;
        result.knockbackImpulse = victimKnockbackImpulse(def, shotDirection, kn);
    }
    hitmarker(totalDamage);
    if (GetPlayerSettings().debugCombat)
        Debug::log(Debug::Category::Weapons,
            "[HITMARKER] attacker=%s victim=npc_%u show=1 reason=local_player_hit_npc",
            shooter.username.c_str(), victim.id);

    printf("[SOUND] weapon=%s event=hit_entity body=%s damage=%.0f\n",
           def.id.c_str(), hitPart.c_str(), result.damage);
    {
        float dist = glm::length(hitEnd - audioListenerPosition());
        float headMul = (hitPart == "head") ? 2.0f : 1.0f;
        float severity = std::clamp(ctx.angleFactor * ((float)totalDamage / 100.0f) * headMul, 0.0f, 1.0f);
        float vol, pit;
        computeImpactAudio(1.2f, dist, severity, vol, pit);
        playWorldSound(def.soundHit, hitEnd, vol, pit, 60.0f);
        Debug::log(Debug::Category::Audio, "[HIT AUDIO] event=%s dist=%.1f damage=%d severity=%.2f pitch=%.2f volume=%.2f\n",
                   def.soundHit.c_str(), dist, totalDamage, severity, pit, vol);
    }
}

void processRemotePlayerHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& shotDirection,
    float nearest,
    Player& shooter,
    uint32_t remoteTargetId,
    Player* remoteVictim)
{
    const int totalDamage = predictedRemoteDamage(def, hitPart, nearest);

    result.hitEntity = true;
    result.targetIsRemotePlayer = true;
    result.bodyPart = hitPart;
    result.damage = (float)totalDamage;
    result.targetId = remoteTargetId;
    {
        const float df = WeaponExecution::hitscanFalloffFactor(def, nearest);
        const float kn =
            (def.victimKnockback + (float)totalDamage * def.victimKnockbackPerDamage) * df;
        result.knockbackImpulse = victimKnockbackImpulse(def, shotDirection, kn);
    }

    presentRemoteHit(def, hitEnd, hitNormal, shotDirection, nearest, hitPart,
                     shooter.username, remoteVictim->username, totalDamage);

    // Predict the victim's knockback instantly so the shooter sees the push
    // immediately (the server's authoritative knockback corrects it if needed).
    if (remoteVictim)
        remoteVictim->externalImpulse += result.knockbackImpulse;

    const int hpBeforePrediction = remoteVictim
        ? remoteVictim->currentHp : totalDamage;
    const auto& prediction = NetworkingConfig::instance().data().prediction;
    if (gpMpContext && gpMpContext->active && remoteVictim &&
        prediction.predictDamage)
    {
        MimitaNet::mpApplyPredictedDamage(
            *gpMpContext, remoteTargetId, totalDamage, false);
    }

    // Predicted kill: if this hit would drop the victim to 0 hp, show the
    // death immediately (gated by predict_deaths). The server's
    // DamageConfirmedEvent reconciles it.
    if (prediction.predictDeaths &&
        remoteVictim && totalDamage >= hpBeforePrediction)
    {
        remoteVictim->killedByWeapon = def.displayName;
        remoteVictim->lastDamagedBy = shooter.username;
        predictRemoteKill(*remoteVictim, shotDirection, "player", remoteTargetId);
        if (gpMpContext && gpMpContext->active)
            MimitaNet::mpApplyPredictedKillHeal(*gpMpContext, remoteTargetId, false);
    }
}

void processRemoteNpcHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const std::string& hitPart,
    const glm::vec3& hitEnd,
    float nearest,
    Player& shooter,
    uint32_t remoteNpcTargetId,
    Player* remoteNpc)
{
    if (remoteNpc)
    {
        const int totalDamage = predictedRemoteDamage(def, hitPart, nearest);
        result.hitEntity = true;
        result.bodyPart = hitPart;
        result.end = hitEnd;
        result.targetId = remoteNpcTargetId;
        result.targetIsRemoteNpc = true;

        // Record the prediction timestamp so the server-confirm path can
        // suppress the duplicate hitmarker/killfeed for the local shooter.
        if (gpMpContext && gpMpContext->active)
        {
            const uint64_t nowMsVal = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            gpMpContext->predictedNpcHitMs[remoteNpcTargetId] = nowMsVal;
        }

        const glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f);
        presentRemoteHit(def, hitEnd, normal, -normal, nearest, hitPart,
                         shooter.username, remoteNpc->username, totalDamage);

        // Predict the NPC knockback instantly (corrected by the server confirm).
        {
            const glm::vec3 hitDir = result.end - result.start;
            const float hitLen = glm::length(hitDir);
            if (hitLen > 0.001f)
            {
                const float kn =
                    def.victimKnockback +
                    (float)totalDamage * 0.08f * (def.victimKnockbackPerDamage / 0.15f);
                remoteNpc->externalImpulse +=
                    victimKnockbackImpulse(def, hitDir / hitLen, kn);
            }
        }

        const int hpBeforePrediction = remoteNpc->currentHp;
        const auto& prediction = NetworkingConfig::instance().data().prediction;
        if (gpMpContext && gpMpContext->active && prediction.predictDamage)
            MimitaNet::mpApplyPredictedDamage(
                *gpMpContext, remoteNpcTargetId, totalDamage, true);

        // Predicted kill: the server NpcDamageEvent remains authoritative and
        // reconciles this (revive + disagreement if the server disagrees).
        // Gated by predict_deaths.
        if (prediction.predictDeaths && totalDamage >= hpBeforePrediction)
        {
            remoteNpc->killedByWeapon = def.displayName;
            remoteNpc->lastDamagedBy = shooter.username;
            predictRemoteKill(*remoteNpc, result.end - result.start, "npc", remoteNpcTargetId);
            if (gpMpContext && gpMpContext->active)
                MimitaNet::mpApplyPredictedKillHeal(*gpMpContext, remoteNpcTargetId, true);
        }
        if (GetPlayerSettings().debugCombat)
            Debug::log(Debug::Category::Weapons,
                "[REMOTE NPC PREDICT] attacker=%s npcId=%u part=%s dist=%.1f damage=%d hp=%d\n",
                shooter.username.c_str(), remoteNpcTargetId, hitPart.c_str(), nearest,
                totalDamage, remoteNpc->currentHp);
    }
    else
    {
        // No replica available: still stop the beam at the traced endpoint.
        result.hitEntity = true;
        result.bodyPart = hitPart;
        result.end = hitEnd;
        result.targetId = remoteNpcTargetId;
    }
}

void processRemoteBlastHitFeedback(
    const WeaponDefinition& def,
    const glm::vec3& hitEnd,
    const glm::vec3& blastDir,
    const std::string& shooterName,
    uint32_t targetId,
    bool isNpc,
    Player& target,
    int damage)
{
    if (target.dead || target.currentHp <= 0)
        return;

    const glm::vec3 normal(0.0f, 0.0f, 1.0f);
    const glm::vec3 dir = glm::length(blastDir) > 0.001f
        ? glm::normalize(blastDir) : glm::vec3(0.0f, 0.0f, -1.0f);
    const int hpBefore = target.currentHp;

    presentRemoteHit(def, hitEnd, normal, -normal, 1.0f, "torso",
                     shooterName, target.username, damage);

    const auto& prediction = NetworkingConfig::instance().data().prediction;
    if (gpMpContext && gpMpContext->active && prediction.predictDamage)
        MimitaNet::mpApplyPredictedDamage(*gpMpContext, targetId, damage, isNpc);

    if (prediction.predictDeaths && damage >= hpBefore)
    {
        target.killedByWeapon = def.displayName;
        target.lastDamagedBy = shooterName;
        predictRemoteKill(target, dir, isNpc ? "npc" : "player", targetId);
        if (gpMpContext && gpMpContext->active)
            MimitaNet::mpApplyPredictedKillHeal(*gpMpContext, targetId, isNpc);
    }
}

void processPlayerHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& shotDirection,
    float nearest,
    Player& shooter,
    Player* targetPlayer,
    float damageMultiplier)
{
    const float minAngle = WeaponExecution::paramOr(def, "minAngleFactor", 0.15f);
    const float angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, hitNormal)), minAngle, 1.0f);
    int totalDamage = WeaponExecution::computeHitscanDamage(def, hitPart, nearest, angleFactor);
    totalDamage = std::max(1, (int)std::round((float)totalDamage * damageMultiplier));

    float df = WeaponExecution::hitscanFalloffFactor(def, nearest);
    const float kn =
        (def.victimKnockback + (float)totalDamage * def.victimKnockbackPerDamage) * df;
    const glm::vec3 knockback = victimKnockbackImpulse(def, shotDirection, kn);

    const_cast<Player*>(targetPlayer)->takeDamage(totalDamage, knockback, glm::length(knockback));
    const_cast<Player*>(targetPlayer)->killedByWeapon = def.displayName;
    const_cast<Player*>(targetPlayer)->lastDamagedBy = shooter.username;

    result.hitEntity = true;
    result.bodyPart = hitPart;
    result.damage = (float)totalDamage;

    Debug::log(Debug::Category::NpcCombat,
        "[NPC HITS PLAYER] npc=%s weapon=%s damage=%d dist=%.1f",
        shooter.username.c_str(), def.id.c_str(), totalDamage, nearest);
    {
        HitEvent ev;
        ev.position = hitEnd;
        ev.normal = hitNormal;
        ev.direction = shotDirection;
        ev.hitEntity = true;
        ev.damage = totalDamage;
        ev.hitDistance = nearest;
        ev.attacker = shooter.username;
        ev.victim = targetPlayer->username;
        ev.weaponSource = def.id;
        HitEffects::onHit(ev);
    }
    {
        float dist = glm::length(hitEnd - audioListenerPosition());
        float angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, hitNormal)), 0.15f, 1.0f);
        float headMul = (hitPart == "head") ? 2.0f : 1.0f;
        float severity = std::clamp(angleFactor * ((float)totalDamage / 100.0f) * headMul, 0.0f, 1.0f);
        float vol, pit;
        const auto& sndCfg = WeaponHitFxConfig::instance().soundFor(def.id);
        computeImpactAudio(sndCfg.baseVolume, dist, severity, vol, pit);
        playWorldSound(def.soundHit, hitEnd, vol, pit, 60.0f);
        Debug::log(Debug::Category::Audio, "[HIT AUDIO] event=%s dist=%.1f damage=%d severity=%.2f pitch=%.2f volume=%.2f\n",
                   def.soundHit.c_str(), dist, totalDamage, severity, pit, vol);
    }
}

void processWorldHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const glm::vec3& hitEnd,
    const glm::vec3& worldNormal,
    const glm::vec3& shotDirection,
    const std::string& shooterName)
{
    result.hitWorld = true;
    {
        float angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, worldNormal)), 0.15f, 1.0f);
        float baseForce = def.damage / 100.0f;
        if (def.projectileSpeed > 0.0f) baseForce *= def.projectileSpeed / 100.0f;
        const auto& forceCfg = WeaponHitFxConfig::instance().forceFor(def.id);
        float hitForce = baseForce * angleFactor * forceCfg.weaponMultiplier;
        hitForce = std::clamp(hitForce, forceCfg.minForce, forceCfg.maxForce);

        HitEvent ev;
        ev.position = hitEnd;
        ev.normal = worldNormal;
        ev.direction = shotDirection;
        ev.hitWorld = true;
        ev.damage = (int)(hitForce * 20.0f);
        ev.attacker = shooterName;
        ev.weaponSource = def.id;
        HitEffects::onHit(ev);
    }
    EffectPartSystem::instance().spawnWorldDebris(hitEnd, worldNormal, std::clamp(def.damage / 100.0f, 0.1f, 5.0f));
    EffectPartSystem::instance().spawnImpactSphereTickCfg(hitEnd);
    float dist = glm::length(hitEnd - audioListenerPosition());
    float directness = std::abs(glm::dot(-shotDirection, worldNormal));
    float severity = std::clamp(directness, 0.0f, 1.0f);
    float vol, pit;
    const auto& sndCfg = WeaponHitFxConfig::instance().soundFor(def.id);
    computeImpactAudio(sndCfg.baseVolume, dist, severity, vol, pit);
    playWorldSound("hitworld", hitEnd, vol, pit, 60.0f);
    Debug::log(Debug::Category::Audio, "[WORLD IMPACT AUDIO] dist=%.1f severity=%.2f pitch=%.2f volume=%.2f\n",
               dist, severity, pit, vol);
}

float computeFalloffDamage(
    const WeaponDefinition& def,
    const std::string& hitPart,
    float nearest,
    int& outDamage)
{
    outDamage = WeaponExecution::computeHitscanDamage(def, hitPart, nearest, 1.0f);
    return WeaponExecution::paramOr(def, "distanceFalloffStart", 110.0f);
}

void processMultiPelletNpcHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    Npc& victim,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& pelletDir,
    float pelletNearest,
    Player& shooter,
    NpcSystem& npcs,
    const glm::vec3& muzzlePos,
    float& accumulatedDamage,
    bool& anyHitEntity,
    uint32_t& lastTargetId,
    glm::vec3& accumulatedKnockback,
    float& nearestPelletDist,
    glm::vec3& lastPelletEnd,
    glm::vec3& lastHitNormal)
{
    DamageContext ctx;
    ctx.baseDamage = def.damage;
    ctx.distance = pelletNearest;
    ctx.angleFactor = std::clamp(std::fabs(glm::dot(-pelletDir, hitNormal)), 0.15f, 1.0f);
    ctx.bodyPart = hitPart;
    ctx.hitPosition = hitEnd;
    ctx.hitNormal = hitNormal;
    ctx.shotDirection = pelletDir;
    ctx.shooterId = 0;
    ctx.shooterName = shooter.username;

    int dmg = applyDamageToEntity(ctx, victim, def, shooter, npcs, muzzlePos, pelletDir);
    accumulatedDamage += (float)dmg;
    anyHitEntity = true;
    lastTargetId = victim.id;
    float df = WeaponExecution::hitscanFalloffFactor(def, pelletNearest);
    {
        const float kbScale = def.victimKnockbackPerDamage / 0.15f;
        accumulatedKnockback += victimKnockbackImpulse(def, pelletDir,
            (def.victimKnockback +
             (float)dmg * (0.08f + ctx.angleFactor * 0.12f) * kbScale) * df);
    }

    if (pelletNearest < nearestPelletDist) {
        nearestPelletDist = pelletNearest;
        lastPelletEnd = hitEnd;
        lastHitNormal = hitNormal;
    }
}

void processMultiPelletRemoteHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& pelletDir,
    float pelletNearest,
    Player& shooter,
    uint32_t pelletRemoteTargetId,
    float& accumulatedDamage,
    bool& anyHitEntity,
    uint32_t& lastTargetId,
    glm::vec3& accumulatedKnockback,
    float& nearestPelletDist,
    glm::vec3& lastPelletEnd,
    glm::vec3& lastHitNormal,
    const std::string& victimName)
{
int totalDmg = WeaponExecution::computeHitscanDamage(def, hitPart, pelletNearest, 1.0f);
    result.targetIsRemotePlayer = true;
    result.hitEntity = true;
    result.targetId = pelletRemoteTargetId;
    accumulatedDamage += (float)totalDmg;
    anyHitEntity = true;
    lastTargetId = pelletRemoteTargetId;

    float df = WeaponExecution::hitscanFalloffFactor(def, pelletNearest);
    accumulatedKnockback += victimKnockbackImpulse(def, pelletDir,
        (def.victimKnockback + (float)totalDmg * def.victimKnockbackPerDamage) * df);

    {
        HitEvent ev;
        ev.position = hitEnd;
        ev.normal = hitNormal;
        ev.direction = pelletDir;
        ev.hitEntity = true;
        ev.damage = totalDmg;
        ev.hitDistance = pelletNearest;
        ev.attacker = shooter.username;
        ev.victim = victimName;
        ev.weaponSource = def.id;
        HitEffects::onHit(ev);
    }

    if (pelletNearest < nearestPelletDist) {
        nearestPelletDist = pelletNearest;
        lastPelletEnd = hitEnd;
        lastHitNormal = hitNormal;
    }
}

void processMultiPelletRemoteNpcHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& pelletDir,
    float pelletNearest,
    Player& shooter,
    uint32_t pelletRemoteNpcTargetId,
    float& accumulatedDamage,
    bool& anyHitEntity,
    uint32_t& lastTargetId,
    glm::vec3& accumulatedKnockback,
    float& nearestPelletDist,
    glm::vec3& lastPelletEnd,
    glm::vec3& lastHitNormal)
{
    int totalDmg = WeaponExecution::computeHitscanDamage(def, hitPart, pelletNearest, 1.0f);
    result.targetIsRemoteNpc = true;
    result.hitEntity = true;
    result.targetId = pelletRemoteNpcTargetId;
    accumulatedDamage += (float)totalDmg;
    anyHitEntity = true;
    lastTargetId = pelletRemoteNpcTargetId;

    float df = WeaponExecution::hitscanFalloffFactor(def, pelletNearest);
    accumulatedKnockback += victimKnockbackImpulse(def, pelletDir,
        (def.victimKnockback + (float)totalDmg * def.victimKnockbackPerDamage) * df);

    // Predicted HP overlay for the remote NPC (instant feedback, corrected by
    // the server confirm). Mirrors the single-shot remote NPC path.
    if (gpMpContext && gpMpContext->active)
    {
        const uint64_t nowMsVal = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        gpMpContext->predictedNpcHitMs[pelletRemoteNpcTargetId] = nowMsVal;
        if (NetworkingConfig::instance().data().prediction.predictDamage)
            MimitaNet::mpApplyPredictedDamage(
                *gpMpContext, pelletRemoteNpcTargetId, totalDmg, true);
    }

    {
        HitEvent ev;
        ev.position = hitEnd;
        ev.normal = hitNormal;
        ev.direction = pelletDir;
        ev.hitEntity = true;
        ev.damage = totalDmg;
        ev.hitDistance = pelletNearest;
        ev.attacker = shooter.username;
        ev.weaponSource = def.id;
        HitEffects::onHit(ev);
    }

    if (pelletNearest < nearestPelletDist) {
        nearestPelletDist = pelletNearest;
        lastPelletEnd = hitEnd;
        lastHitNormal = hitNormal;
    }
}

void processMultiPelletWorldHit(
    const WeaponDefinition& def,
    const glm::vec3& hitEnd,
    const glm::vec3& worldNml,
    const glm::vec3& pelletDir,
    float pelletNearest,
    Player& shooter,
    bool& anyHitWorld,
    float& nearestPelletDist,
    glm::vec3& lastPelletEnd,
    glm::vec3& lastHitNormal)
{
    anyHitWorld = true;
    float debrisForce = std::clamp(def.damage / 100.0f, 0.1f, 5.0f);
    EffectPartSystem::instance().queueWorldHit(
        hitEnd, worldNml, pelletDir, debrisForce,
        shooter.username, def.id);
    EffectPartSystem::instance().spawnImpactSphereTickCfg(hitEnd);

    if (pelletNearest < nearestPelletDist) {
        nearestPelletDist = pelletNearest;
        lastPelletEnd = hitEnd;
        lastHitNormal = worldNml;
    }
}

void finalizeMultiPelletResult(
    RevolverShotResult& outResult,
    const glm::vec3& muzzlePos,
    const glm::vec3& lastPelletEnd,
    const glm::vec3& lastHitNormal,
    float accumulatedDamage,
    bool anyHitEntity,
    bool anyHitWorld,
    uint32_t lastTargetId,
    const glm::vec3& accumulatedKnockback,
    int totalPellets,
    const WeaponDefinition& def,
    Player& shooter)
{
    outResult.fired = true;
    outResult.start = muzzlePos;
    outResult.end = lastPelletEnd;
    outResult.hitNormal = lastHitNormal;
    outResult.damage = accumulatedDamage;
    outResult.hitEntity = anyHitEntity;
    outResult.hitWorld = anyHitWorld && !anyHitEntity;
    outResult.targetId = lastTargetId;
    outResult.knockbackImpulse = accumulatedKnockback;

    printf("[WEAPON] multi-pellet fired: pellets=%d totalDamage=%.0f\n",
           totalPellets, accumulatedDamage);

    if (anyHitEntity) {
        float dist = glm::length(lastPelletEnd - audioListenerPosition());
        float severity = std::clamp(accumulatedDamage / 100.0f, 0.0f, 1.0f);
        float vol, pit;
        computeImpactAudio(1.2f, dist, severity, vol, pit);
        playWorldSound(def.soundHit, lastPelletEnd, vol, pit, 60.0f);
        Debug::log(Debug::Category::Audio, "[HIT AUDIO] event=%s dist=%.1f damage=%.0f severity=%.2f pitch=%.2f volume=%.2f\n",
                   def.soundHit.c_str(), dist, accumulatedDamage, severity, pit, vol);
        hitmarker((int)accumulatedDamage);
        if (GetPlayerSettings().debugCombat)
            Debug::log(Debug::Category::Weapons,
                "[HITMARKER] attacker=%s pellet_hit=1 show=1 reason=shotgun_hit_entity",
                shooter.username.c_str());
    } else if (anyHitWorld) {
        float dist = glm::length(lastPelletEnd - audioListenerPosition());
        glm::vec3 hitDir = glm::length(lastPelletEnd - muzzlePos) > 0.001f
            ? glm::normalize(lastPelletEnd - muzzlePos) : glm::vec3(0.0f, 0.0f, -1.0f);
        float directness = std::abs(glm::dot(-hitDir, lastHitNormal));
        float severity = std::clamp(directness, 0.0f, 1.0f);
        float vol, pit;
        computeImpactAudio(1.2f, dist, severity, vol, pit);
        playWorldSound("hitworld", lastPelletEnd, vol, pit, 60.0f);
        Debug::log(Debug::Category::Audio, "[WORLD IMPACT AUDIO] dist=%.1f severity=%.2f pitch=%.2f volume=%.2f\n",
                   dist, severity, pit, vol);
    }
}

} // namespace WeaponFire

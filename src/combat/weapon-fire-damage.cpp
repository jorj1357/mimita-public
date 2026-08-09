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
#include "config/networking-config.h"
#include "config/ragdoll-death-config.h"
#include "config/player-settings.h"
#include "config/weapon-hitfx-config.h"
#include "debug/debug-log.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
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

// Config-driven limb damage multiplier (0.75 default) so client prediction
// matches the server damage model.
static float limbMultiplier(const WeaponDefinition& def)
{
    auto it = def.customParams.find("limbDamageMultiplier");
    return it != def.customParams.end() ? it->second : 0.75f;
}

// Predicted remote kill: show the death instantly on the local replica without
// DeathSystem::kill's side effects (killfeed, heal, replay, duel tracking) —
// those stay server-confirmed so they never double-fire. The server's
// confirmed damage event remains authoritative and reconciles this (clear the
// flag on agreement, or revive + disagreement on rollback).
static void predictRemoteKill(Player& victim,
                              const glm::vec3& direction,
                              const std::string& actorType)
{
    if (victim.dead || victim.netPredictedDead)
        return;
    victim.netPredictedDead = true;

    victim.vel = glm::vec3(0.0f);
    victim.externalImpulse = glm::vec3(0.0f);
    victim.inputWishMove = glm::vec2(0.0f);
    victim.currentHp = 0;
    victim.dead = true;
    victim.proceduralFrozen = true;
    victim.respawnTimer = 0.0f;

    // Freeze aim/procedural pose (mirrors DeathSystem::kill prologue).
    for (PhysicalBodyPart& part : victim.physicalBody.parts)
    {
        if (part.name == "leftArm" || part.name == "rightArm")
        {
            part.pose = ProceduralPose{};
            part.perfectPose = ProceduralPose{};
            part.translationSpring = SpringState{};
            part.rotationSpring = SpringState{};
        }
    }
    victim.syncLegacyStateToLayers();
    victim.updateModelWorldTransforms();

    const auto& cfg = RagdollDeathConfig::instance();
    victim.deathAnim.active = true;
    victim.deathAnim.tick = 0;
    victim.deathAnim.totalTicks = cfg.totalTicks();
    victim.deathAnim.startAlpha = cfg.startAlpha();
    victim.deathAnim.endAlpha = cfg.endAlpha();
    victim.deathAnim.startRotation = cfg.startRotation();
    victim.deathAnim.endRotation = cfg.endRotation();
    victim.deathAnim.frozenPosition = victim.pos;

    glm::vec3 dir = glm::length(direction) > 0.001f
        ? glm::normalize(direction) : glm::vec3(0.0f, 0.0f, -1.0f);
    const auto& deCfg = HitEffects::config().deathEllipsoid;
    if (deCfg.enabled)
    {
        EffectPartSystem::instance().spawnDeathEllipsoid(
            victim.pos, dir, deCfg.length, deCfg.radius, deCfg.lifetime,
            victim.sizeScale);
    }
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
        ev.attacker = shooterName;
        ev.victim = victimName;
        ev.weaponSource = def.id;
        // predict_damage off: still hitmarker + blood + sound, but no floating
        // damage number (that comes server-confirmed only).
        ev.suppressDamageNumber =
            !NetworkingConfig::instance().data().prediction.predictDamage;
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
    float damage = def.damage;
    if (hitPart == "head")
        damage *= def.headshotMultiplier;
    else if (hitPart.find("leg") != std::string::npos)
        damage *= limbMultiplier(def);

    const float falloffStart = def.customParams.count("distanceFalloffStart")
        ? def.customParams.at("distanceFalloffStart") : 110.0f;
    const float minFraction = def.customParams.count("minDamageFraction")
        ? def.customParams.at("minDamageFraction") : 0.1f;
    damage *= std::clamp(1.0f - nearest / falloffStart, minFraction, 1.0f);
    return std::max(1, (int)std::round(damage));
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
        float df = std::clamp(1.0f - nearest / 110.0f, 0.10f, 1.0f);
        float kn = (float)totalDamage * df * (0.08f + ctx.angleFactor * 0.12f);
        result.knockbackImpulse = shotDirection * kn + glm::vec3(0, 0, kn * 0.12f);
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
        const float falloffStart = def.customParams.count("distanceFalloffStart")
            ? def.customParams.at("distanceFalloffStart") : 110.0f;
        const float minFraction = def.customParams.count("minDamageFraction")
            ? def.customParams.at("minDamageFraction") : 0.1f;
        float df = std::clamp(1.0f - nearest / falloffStart, minFraction, 1.0f);
        float kn = (float)totalDamage * df * 0.15f;
        result.knockbackImpulse = shotDirection * kn;
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
        predictRemoteKill(*remoteVictim, shotDirection, "player");
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
                const float kn = (float)totalDamage * 0.08f;
                remoteNpc->externalImpulse += hitDir / hitLen * kn;
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
            predictRemoteKill(*remoteNpc, result.end - result.start, "npc");
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
        predictRemoteKill(target, dir, isNpc ? "npc" : "player");
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
    float damage = def.damage;
    if (hitPart == "head")
        damage *= def.headshotMultiplier;
    else if (hitPart.find("leg") != std::string::npos)
        damage *= limbMultiplier(def);

    const float falloffStart = def.customParams.count("distanceFalloffStart")
        ? def.customParams.at("distanceFalloffStart") : 110.0f;
    const float minFraction = def.customParams.count("minDamageFraction")
        ? def.customParams.at("minDamageFraction") : 0.1f;
    damage *= std::clamp(1.0f - nearest / falloffStart, minFraction, 1.0f);
    damage *= damageMultiplier;
    int totalDamage = std::max(1, (int)std::round(damage));

    float df = std::clamp(1.0f - nearest / falloffStart, minFraction, 1.0f);
    float kn = (float)totalDamage * df * 0.15f;
    glm::vec3 knockback = shotDirection * kn;

    const_cast<Player*>(targetPlayer)->takeDamage(totalDamage, knockback, 8.0f);
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
    EffectPartSystem::instance().spawnImpactSphereTick(hitEnd, {0.1f, 0.5f, 1.0f});
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
    float damage = def.damage;
    if (hitPart == "head")
        damage *= def.headshotMultiplier;
    else if (hitPart.find("leg") != std::string::npos)
        damage *= limbMultiplier(def);

    const float falloffStart = def.customParams.count("distanceFalloffStart")
        ? def.customParams.at("distanceFalloffStart") : 110.0f;
    const float minFraction = def.customParams.count("minDamageFraction")
        ? def.customParams.at("minDamageFraction") : 0.1f;
    damage *= std::clamp(1.0f - nearest / falloffStart, minFraction, 1.0f);
    outDamage = std::max(1, (int)std::round(damage));
    return falloffStart;
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
    float df = std::clamp(1.0f - pelletNearest / 110.0f, 0.10f, 1.0f);
    accumulatedKnockback += pelletDir * (float)dmg * df * (0.08f + ctx.angleFactor * 0.12f);

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
    float dmg = def.damage;
    if (hitPart == "head") dmg *= def.headshotMultiplier;
    else if (hitPart.find("leg") != std::string::npos)
        dmg *= limbMultiplier(def);

    float falloffStart = 110.0f;
    auto fit = def.customParams.find("distanceFalloffStart");
    if (fit != def.customParams.end()) falloffStart = fit->second;
    float minFrac = 0.1f;
    fit = def.customParams.find("minDamageFraction");
    if (fit != def.customParams.end()) minFrac = fit->second;
    dmg *= std::clamp(1.0f - pelletNearest / falloffStart, minFrac, 1.0f);
    int totalDmg = std::max(1, (int)std::round(dmg));
    result.targetIsRemotePlayer = true;
    result.hitEntity = true;
    result.targetId = pelletRemoteTargetId;
    accumulatedDamage += (float)totalDmg;
    anyHitEntity = true;
    lastTargetId = pelletRemoteTargetId;

    float df = std::clamp(1.0f - pelletNearest / falloffStart, minFrac, 1.0f);
    accumulatedKnockback += pelletDir * (float)totalDmg * df * 0.15f;

    {
        HitEvent ev;
        ev.position = hitEnd;
        ev.normal = hitNormal;
        ev.direction = pelletDir;
        ev.hitEntity = true;
        ev.damage = totalDmg;
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
    float dmg = def.damage;
    if (hitPart == "head") dmg *= def.headshotMultiplier;
    else if (hitPart.find("leg") != std::string::npos)
        dmg *= limbMultiplier(def);

    float falloffStart = 110.0f;
    auto fit = def.customParams.find("distanceFalloffStart");
    if (fit != def.customParams.end()) falloffStart = fit->second;
    float minFrac = 0.1f;
    fit = def.customParams.find("minDamageFraction");
    if (fit != def.customParams.end()) minFrac = fit->second;
    dmg *= std::clamp(1.0f - pelletNearest / falloffStart, minFrac, 1.0f);
    int totalDmg = std::max(1, (int)std::round(dmg));
    result.targetIsRemoteNpc = true;
    result.hitEntity = true;
    result.targetId = pelletRemoteNpcTargetId;
    accumulatedDamage += (float)totalDmg;
    anyHitEntity = true;
    lastTargetId = pelletRemoteNpcTargetId;

    float df = std::clamp(1.0f - pelletNearest / falloffStart, minFrac, 1.0f);
    accumulatedKnockback += pelletDir * (float)totalDmg * df * 0.15f;

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
    EffectPartSystem::instance().spawnImpactSphereTick(hitEnd, {0.1f, 0.5f, 1.0f});

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

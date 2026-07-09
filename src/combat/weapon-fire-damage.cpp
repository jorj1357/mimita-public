#include "weapon-fire.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "audio/audio.h"
#include "combat/weapon-audio.h"
#include "config/player-settings.h"
#include "config/weapon-hitfx-config.h"
#include "debug/debug-log.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "ui/hitmarker.h"

namespace WeaponFire {

static void fireSound(const WeaponDefinition& def, const glm::vec3& muzzlePos)
{
    WeaponAudio::playShootSound(def, muzzlePos);
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
    const Player* remoteVictim)
{
    float damage = def.damage;
    if (hitPart == "head")
        damage *= def.headshotMultiplier;
    else if (hitPart == "leg")
        damage *= 0.5f;

    const float falloffStart = def.customParams.count("distanceFalloffStart")
        ? def.customParams.at("distanceFalloffStart") : 110.0f;
    const float minFraction = def.customParams.count("minDamageFraction")
        ? def.customParams.at("minDamageFraction") : 0.1f;
    damage *= std::clamp(
        1.0f - nearest / falloffStart, minFraction, 1.0f);
    const int totalDamage = std::max(1, (int)std::round(damage));

    result.hitEntity = true;
    result.targetIsRemotePlayer = true;
    result.bodyPart = hitPart;
    result.damage = (float)totalDamage;
    result.targetId = remoteTargetId;
    {
        float df = std::clamp(1.0f - nearest / falloffStart, minFraction, 1.0f);
        float kn = (float)totalDamage * df * 0.15f;
        result.knockbackImpulse = shotDirection * kn;
    }
    hitmarker(totalDamage);
    if (GetPlayerSettings().debugCombat)
        Debug::log(Debug::Category::Weapons,
            "[HITMARKER] attacker=%s victim=%s show=1 reason=local_player_hit_remote",
            shooter.username.c_str(), remoteVictim->username.c_str());
    {
        HitEvent ev;
        ev.position = hitEnd;
        ev.normal = hitNormal;
        ev.direction = shotDirection;
        ev.hitEntity = true;
        ev.damage = totalDamage;
        ev.attacker = shooter.username;
        ev.victim = remoteVictim->username;
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

void processPlayerHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& shotDirection,
    float nearest,
    Player& shooter,
    Player* targetPlayer)
{
    float damage = def.damage;
    if (hitPart == "head")
        damage *= def.headshotMultiplier;
    else if (hitPart == "leg")
        damage *= 0.5f;

    const float falloffStart = def.customParams.count("distanceFalloffStart")
        ? def.customParams.at("distanceFalloffStart") : 110.0f;
    const float minFraction = def.customParams.count("minDamageFraction")
        ? def.customParams.at("minDamageFraction") : 0.1f;
    damage *= std::clamp(1.0f - nearest / falloffStart, minFraction, 1.0f);
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
    else if (hitPart == "leg")
        damage *= 0.5f;

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
    (void)result;
    float dmg = def.damage;
    if (hitPart == "head") dmg *= def.headshotMultiplier;
    else if (hitPart == "leg") dmg *= 0.5f;

    float falloffStart = 110.0f;
    auto fit = def.customParams.find("distanceFalloffStart");
    if (fit != def.customParams.end()) falloffStart = fit->second;
    float minFrac = 0.1f;
    fit = def.customParams.find("minDamageFraction");
    if (fit != def.customParams.end()) minFrac = fit->second;
    dmg *= std::clamp(1.0f - pelletNearest / falloffStart, minFrac, 1.0f);
    int totalDmg = std::max(1, (int)std::round(dmg));
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

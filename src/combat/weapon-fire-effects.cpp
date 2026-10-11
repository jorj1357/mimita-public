#include "combat/weapon-fire.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include "config/player-settings.h"
#include "config/size-scaling-config.h"
#include "config/weapon-hitfx-config.h"
#include "combat/weapon-execution.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "effects/hit-effects.h"
#include "combat/death-system.h"
#include "devtools/terminal.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "camera.h"

namespace WeaponFire {

void applyRecoil(Camera& camera, Player& shooter, const WeaponDefinition& def,
                 const glm::vec3& shotDirection, float& inOutRecoil, float dt) {
    const PlayerSettings& cfg = GetPlayerSettings();
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(shooter.sizeScale, 0.001f);
    // Per-weapon shooter_knockback (hot reloadable) wins over the global player
    // setting, so recoil / self-push can be tuned per weapon (TF2-style).
    float recoilStrength = def.shooterKnockback > 0.0f
        ? def.shooterKnockback * def.selfImpulseMultiplier *
              sc.scale(1.0f, sc.recoilExponent, ss)
        : cfg.weaponRecoilStrength * sc.scale(1.0f, sc.recoilExponent, ss);
    glm::vec3 recoilDir(
        -shotDirection.x,
        -shotDirection.y,
        0.0f
    );
    if (glm::length(recoilDir) > 0.001f)
        recoilDir = glm::normalize(recoilDir);
    shooter.externalImpulse += recoilDir * recoilStrength;
    shooter.externalImpulse.z += recoilStrength * (def.shooterKnockbackVertical > 0.0f
        ? def.shooterKnockbackVertical
        : cfg.weaponRecoilUpKick * 0.01f);
    float shotRecoil = 0.0f;
    float cameraKick = 0.0f;
    float cameraYawKick = 0.0f;
    bool typedRecoil = def.recoilDefinition.enabled;
    if (typedRecoil) {
        const auto& r = def.recoilDefinition;
        const float adsScale = shooter.weaponAimDownSights ? r.cameraAdsScale : 1.0f;
        shotRecoil = r.accumulationPerShot * sc.scale(1.0f, sc.recoilExponent, ss);
        inOutRecoil += shotRecoil;
        if (r.maxAccumulation > 0.0f)
            inOutRecoil = std::min(inOutRecoil, r.maxAccumulation * sc.scale(1.0f, sc.recoilExponent, ss));
        cameraKick = r.pitchPerShot * r.cameraScale * adsScale;
        cameraYawKick = r.yawPerShot * r.cameraScale * adsScale;
        shooter.weaponRecoilArmRotation += glm::vec3(
            -r.armPitchPerShot,
            r.armYawPerShot,
            r.armRollPerShot) * (shooter.weaponAimDownSights ? r.armAdsScale : 1.0f);
        shooter.weaponRecoilArmRecoveryRate = r.armRecoveryRate;
    } else {
        const float recoilAccumulation = def.customParams.count("recoilAccumulation")
            ? std::max(0.0f, def.customParams.at("recoilAccumulation")) : 1.0f;
        const float recoilScale = std::max(0.0f, def.recoil - 1.0f);
        shotRecoil = std::clamp(recoilScale * 0.01f * recoilAccumulation, 0.0f, 4.0f);
        inOutRecoil += shotRecoil * sc.scale(1.0f, sc.recoilExponent, ss);
        cameraKick = std::clamp(shotRecoil * 1.25f + inOutRecoil * 0.08f, 0.0f, 10.0f);
    }

    if (cameraKick > 0.0f || cameraYawKick != 0.0f) {
        const glm::vec3 viewRightRaw = glm::cross(camera.front, glm::vec3(0.0f, 0.0f, 1.0f));
        const float viewRightLength = glm::length(viewRightRaw);
        if (viewRightLength > 0.001f) {
            const glm::vec3 viewRight = viewRightRaw / viewRightLength;
            const float directionLength = glm::length(shotDirection);
            const float lateral = directionLength > 0.001f
                ? glm::dot(shotDirection / directionLength, viewRight) : 0.0f;
            if (typedRecoil)
                camera.addWeaponRecoil(cameraKick, cameraYawKick - lateral * cameraKick * 0.18f);
            else
                camera.addPunch(cameraKick, -lateral * cameraKick * 0.18f);
        }
    }

    if (DebugConfig::DEBUG_RECOIL)
        Debug::log(Debug::Category::General,
            "[RECOIL] weapon=%s input=%.2f typed=%d shot=%.2f accumulated=%.2f camera=(%.2f %.2f) arm=(%.2f %.2f %.2f)\n",
            def.id.c_str(), def.recoil, (int)typedRecoil, shotRecoil, inOutRecoil,
            cameraKick, cameraYawKick,
            shooter.weaponRecoilArmRotation.x,
            shooter.weaponRecoilArmRotation.y,
            shooter.weaponRecoilArmRotation.z);

    if (StructuredLogger::instance().shouldLog(
            StructuredCategory::Weapons, StructuredLevel::Important)) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Weapons, StructuredLevel::Important,
            "weapon.recoil.shot", def.id, "shot-accepted", 0,
            nlohmann::json{
                {"typed", typedRecoil}, {"ads", shooter.weaponAimDownSights},
                {"shot_recoil", shotRecoil}, {"accumulated_recoil", inOutRecoil},
                {"camera_pitch", cameraKick}, {"camera_yaw", cameraYawKick},
                {"arm_pitch", shooter.weaponRecoilArmRotation.x},
                {"arm_yaw", shooter.weaponRecoilArmRotation.y},
                {"arm_roll", shooter.weaponRecoilArmRotation.z}}
        );
    }
}

int applyDamageToEntity(const DamageContext& ctx, Npc& victim,
                         const WeaponDefinition& def, Player& shooter,
                         NpcSystem& npcs, const glm::vec3& muzzlePos,
                         const glm::vec3& shotDirection, bool spawnDamageNumber) {
    float minAngleFrac = 0.15f;
    auto it = def.customParams.find("minAngleFactor");
    if (it != def.customParams.end()) minAngleFrac = it->second;

    float angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, ctx.hitNormal)), minAngleFrac, 1.0f);
    int rounded = WeaponExecution::computeHitscanDamage(def, ctx.bodyPart, ctx.distance, angleFactor);

    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(shooter.sizeScale, 0.001f);
    rounded = std::max(1, (int)std::round((float)rounded * sc.scale(1.0f, sc.damageExponent, ss)));

    // Unified hitForce: drives debris, blood, sound, knockback
    float hitForce = (float)rounded / 100.0f;
    if (def.projectileSpeed > 0.0f)
        hitForce *= def.projectileSpeed / 100.0f;
    const auto& forceCfg = WeaponHitFxConfig::instance().forceFor(def.id);
    if (forceCfg.angleEnabled)
        hitForce *= angleFactor;
    hitForce *= forceCfg.weaponMultiplier;
    hitForce = std::clamp(hitForce, forceCfg.minForce, forceCfg.maxForce);

    float knockback = hitForce * 2.0f;

    Debug::log(Debug::Category::NpcCombat,
        "[HITFX] weapon=%s hitForce=%.3f angleFactor=%.2f damage=%d debris=%.0f\n",
        def.id.c_str(), hitForce, angleFactor, rounded,
        hitForce * 4.0f + 12.0f);

    victim.body.currentHp = std::max(0, victim.body.currentHp - rounded);
    victim.body.externalImpulse += shotDirection * knockback + glm::vec3(0, 0, knockback * 0.12f);
    victim.body.killedByWeapon = def.displayName;
    victim.body.lastDamagedBy = shooter.username;
    victim.hitReactionTimer = 0.15f + std::min((float)rounded / 50.0f, 0.5f);
    // Add aim flinch proportional to damage
    victim.aimTimer = -std::min((float)rounded / 30.0f, 0.4f);

    {
        HitEvent ev;
        ev.position = ctx.hitPosition;
        ev.normal = ctx.hitNormal;
        ev.direction = -shotDirection;
        ev.hitEntity = true;
        ev.damage = rounded;
        ev.hitDistance = ctx.distance;
        ev.attacker = shooter.username;
        ev.victim = victim.body.username;
        ev.weaponSource = "weaponfire";
        ev.weaponId = def.id;
        ev.attackType = "hitscan";
        ev.bodyPart = ctx.bodyPart;
        ev.spawnDamageNumber = spawnDamageNumber;
        HitEffects::onHit(ev);
    }

    if (GetPlayerSettings().debugCombat) {
        char debug[320];
        snprintf(debug, sizeof(debug),
                 "[DAMAGE] part=%s distance=%.2fm angleFactor=%.2f damage=%d knockback=%.2f",
                 ctx.bodyPart.c_str(), ctx.distance, angleFactor, rounded, knockback);
        Terminal::instance().addLog(debug);
    }

    if (victim.body.currentHp <= 0) {
        DeathSystem::instance().kill(
            victim.body,
            "npc_" + std::to_string(victim.id),
            "npc",
            shooter.username,
            shotDirection,
            18.0f);
        std::string line = shooter.username + " killed " + victim.body.username + " with " + def.displayName;
        Terminal::instance().addLog(line);
    }

    return rounded;
}

} // namespace WeaponFire

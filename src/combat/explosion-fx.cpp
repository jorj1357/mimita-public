// 07 31 2026, 18 41
/* purpose
* Implements the shared explosion visual spawner used by rocket and grenade launchers.
* Owns the single source of truth for explosion fireball, smoke, debris, and sound visuals.
* Does NOT apply damage, knockback, camera shake, or server authority.
* Does NOT send packets or render viewmodels.
*/

#include "combat/explosion-fx.h"

#include <cstdlib>

#include "audio/audio.h"
#include "config/weapon-hitfx-config.h"
#include "combat/weapon-registry.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "debug/structured-log.h"
#include "render/dynamic-light-config.h"
#include "render/dynamic-light.h"

bool spawnExplosionFx(const glm::vec3& position, const std::string& weaponId,
                      const std::string& attacker, float sizeScale, bool playSound,
                      const glm::vec3& surfaceNormal, float forceScale,
                      const glm::vec3* markPosition, const glm::vec3* markNormal,
                      bool spawnMark)
{
    forceScale = std::clamp(forceScale, 0.25f, 3.0f);

    // Bloodtest bullets: a bullet hole plus a compact authored 3D shockwave.
    // The rocket/grenade fireball, smoke, flash and explosion sound are skipped.
    if (weaponId == "bloodtest_ak47" || weaponId == "bloodtest_judgement") {
        const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
        const glm::vec3 normal = glm::length(surfaceNormal) > 0.001f
            ? glm::normalize(surfaceNormal) : glm::vec3(0.0f, 0.0f, 1.0f);
        const float bulletHoleScale = def && def->customParams.count("bullet_hole_scale")
            ? std::clamp(def->customParams.at("bullet_hole_scale"), 0.05f, 4.0f)
            : 0.25f;
        if (spawnMark)
            EffectPartSystem::instance().spawnBulletImpact(
                position, normal, bulletHoleScale, 1.0f, 1.0f);
        if (def &&
            (!def->explosionShockwaveModel.empty() ||
             !def->explosionShockwaveTexture.empty())) {
            const float rand01 = (float)std::rand() / (float)RAND_MAX;
            const float rand01b = (float)std::rand() / (float)RAND_MAX;
            const float randomMin = def->explosionShockwaveRandomRotationMinDeg;
            const float randomMax = def->explosionShockwaveRandomRotationMaxDeg;
            const float randomSpan = randomMax - randomMin;
            const float baseScale = std::max(0.01f, def->explosionShockwaveModelBaseScale);
            EffectPart shockwave;
            shockwave.position = position + normal * 0.05f;
            shockwave.normal = normal;
            shockwave.maxLifetime = std::max(0.02f, def->explosionShockwaveLifetime);
            shockwave.rotation.z = def->explosionShockwaveStartRotationDeg +
                (randomSpan > 0.0f ? randomMin + rand01 * randomSpan : 0.0f);
            shockwave.endRotation.z = def->explosionShockwaveEndRotationDeg +
                (randomSpan > 0.0f ? randomMin + rand01b * randomSpan : 0.0f);
            shockwave.scale = def->explosionShockwaveStartScale * forceScale * baseScale;
            shockwave.endScale = def->explosionShockwaveEndScale * forceScale * baseScale;
            shockwave.alpha = def->explosionShockwaveStartAlpha;
            shockwave.endAlpha = def->explosionShockwaveEndAlpha;
            shockwave.useEndAlpha = true;
            shockwave.color = def->explosionShockwaveStartColor;
            shockwave.endColor = def->explosionShockwaveEndColor;
            shockwave.useEndColor = true;
            shockwave.billboardText = false;
            shockwave.replayType = "impact_shockwave";
            shockwave.assetId = weaponId;
            shockwave.modelPath = def->explosionShockwaveModel;
            shockwave.modelNormalize = def->explosionShockwaveNormalize;
            shockwave.texturePath = def->explosionShockwaveTexture;
            EffectPartSystem::instance().spawn(shockwave);
        }
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Weapons, StructuredLevel::Important,
            "impact.surface.spawn", weaponId,
            "spawned bullet hole and surface shockwave",
            0, nlohmann::json{{"weapon", weaponId},
                              {"force_scale", forceScale},
                              {"spawn_mark", spawnMark},
                              {"bullet_hole_scale", bulletHoleScale},
                              {"shockwave_model", def ? def->explosionShockwaveModel : std::string()},
                              {"shockwave_lifetime_seconds", def ? def->explosionShockwaveLifetime : 0.0f},
                              {"shockwave_start_scale", def ? def->explosionShockwaveStartScale : 0.0f},
                              {"shockwave_end_scale", def ? def->explosionShockwaveEndScale : 0.0f},
                              {"shockwave_start_alpha", def ? def->explosionShockwaveStartAlpha : 0.0f},
                              {"shockwave_end_alpha", def ? def->explosionShockwaveEndAlpha : 0.0f},
                              {"base_scale", def ? def->explosionShockwaveModelBaseScale : 1.0f}});
        return false;
    }

    if (weaponId == "projectile_rifle") {
        const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
        const auto param = [&](const char* key, float fallback) {
            if (!def) return fallback;
            const auto it = def->customParams.find(key);
            return it != def->customParams.end() ? it->second : fallback;
        };
        const glm::vec3 normal = glm::length(surfaceNormal) > 0.001f
            ? glm::normalize(surfaceNormal) : glm::vec3(0.0f, 0.0f, 1.0f);
        const glm::vec3 impactPosition = position + normal * 0.01f;
        const glm::vec3 color(param("impactColorR", 1.0f),
                              param("impactColorG", 1.0f),
                              param("impactColorB", 1.0f));
        EffectPart stage1;
        stage1.position = impactPosition;
        stage1.normal = normal;
        stage1.maxLifetime = 1.0f / 60.0f;
        stage1.scale = param("impactRadius", 0.25f);
        stage1.endScale = stage1.scale;
        stage1.color = color;
        stage1.alpha = param("impactAlphaStage1", 0.5f);
        stage1.billboardText = false;
        stage1.replayType = "projectile_rifle_impact_stage1";
        EffectPartSystem::instance().spawn(stage1);

        EffectPart stage2 = stage1;
        stage2.maxLifetime = 5.0f / 60.0f;
        stage2.scale = stage1.scale * 1.2f;
        stage2.alpha = param("impactAlphaStage2", 0.3f);
        stage2.replayType = "projectile_rifle_impact_stage2";
        EffectPartSystem::instance().spawn(stage2);

        EffectPart stage3 = stage1;
        stage3.maxLifetime = param("impactStage3Ticks", 18.0f) / 60.0f;
        stage3.scale = stage1.scale * 1.5f;
        stage3.alpha = param("impactAlphaStage3", 0.12f);
        stage3.replayType = "projectile_rifle_impact_stage3";
        EffectPartSystem::instance().spawn(stage3);
        return false;
    }
    const auto& expCfg = WeaponHitFxConfig::instance().explosionBurstFor(weaponId);

    // Explosion sound
    const WeaponDefinition* weaponDef = WeaponRegistry::instance().get(weaponId);
    const bool grenade = weaponId == "grenade_launcher" ||
        (weaponDef && weaponDef->behaviorType == WeaponBehaviorType::GrenadeLauncher);
    const char* sound = grenade
        ? "grenadelauncher/grenadelauncherexplode"
        : "rocketlauncher/rocketlauncherexplode";
    if (playSound)
        playWorldSound(sound, position, 1.0f, 1.0f, 50.0f);

    // Explosion flash, debris, and red 1-tick impact sphere — each config-gated
    if (expCfg.muzzleFlash)
        EffectPartSystem::instance().spawnMuzzleFlash(position, weaponId + "_explosion", sizeScale, weaponId);
    if (expCfg.debris)
        EffectPartSystem::instance().spawnWorldDebris(position, glm::vec3(0.0f, 0.0f, 1.0f), 3.0f, sizeScale);
    if (expCfg.impactTick)
        EffectPartSystem::instance().spawnImpactSphereTick(position, {1.0f, 0.15f, 0.05f}, 0.5f);

    // Smoke burst — config-driven for rockets and grenades
    if (expCfg.smoke.enabled)
    {
        for (int i = 0; i < expCfg.smoke.count; ++i)
        {
            EffectPart part;
            part.position = position + glm::vec3(
                ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread,
                ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread,
                ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread);
            part.velocity = glm::vec3(
                ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.speed,
                ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.speed,
                (float)rand() / RAND_MAX * expCfg.smoke.speed * 0.5f + expCfg.smoke.upwardBias);
            part.lifetime = 0.0f;
            part.maxLifetime = expCfg.smoke.lifetime + (float)rand() / RAND_MAX * expCfg.smoke.lifetime * 0.3f;
            part.scale = expCfg.smoke.size + (float)rand() / RAND_MAX * expCfg.smoke.size * 0.5f;
            part.endScale = expCfg.smoke.endSize + (float)rand() / RAND_MAX * expCfg.smoke.endSize * 0.5f;
            part.color = expCfg.smoke.color;
            part.alpha = expCfg.smoke.alpha;
            part.gravity = 1.0f;
            part.affectedByGravity = true;
            part.billboardText = false;
            part.replayType = weaponId + "_explosion_smoke";
            if (weaponDef && !weaponDef->projectileSmokeTexture.empty()) {
                part.texturePath = weaponDef->projectileSmokeTexture;
                part.replayType = "hitfx_particle";
            }
            EffectPartSystem::instance().spawn(part);
        }
    }

    // Dedicated explosion light. Muzzle flash lighting stays separate.
    {
        const auto& lightCfg =
            DynamicLightConfig::instance().weaponLight(weaponId, "explosion");
        if (lightCfg.enabled) {
            DynamicLightManager::instance().spawn(
                position + lightCfg.positionOffset,
                lightCfg.color,
                lightCfg.intensity,
                lightCfg.radius,
                lightCfg.lifetime,
                lightCfg.fadeIn,
                lightCfg.fadeOut);
        }
    }

    // Surface-aligned shockwave. The weapon may use an authored model or a
    // plain PNG; the PNG path takes precedence when configured.
    if (weaponDef &&
        (!weaponDef->explosionShockwaveModel.empty() ||
         !weaponDef->explosionShockwaveTexture.empty())) {
        const glm::vec3 normal = glm::length(surfaceNormal) > 0.001f
            ? glm::normalize(surfaceNormal) : glm::vec3(0.0f, 0.0f, 1.0f);
        const float rand01 = (float)std::rand() / (float)RAND_MAX;
        const float rand01b = (float)std::rand() / (float)RAND_MAX;
        const float randomMin = weaponDef->explosionShockwaveRandomRotationMinDeg;
        const float randomMax = weaponDef->explosionShockwaveRandomRotationMaxDeg;
        const float randomSpan = randomMax - randomMin;
        const float startRotation = weaponDef->explosionShockwaveStartRotationDeg +
            (randomSpan > 0.0f ? randomMin + rand01 * randomSpan : 0.0f);
        const float endRotation = weaponDef->explosionShockwaveEndRotationDeg +
            (randomSpan > 0.0f ? randomMin + rand01b * randomSpan : 0.0f);
        EffectPart shockwave;
        shockwave.position = position + normal * 0.05f;
        shockwave.normal = normal;
        shockwave.maxLifetime = std::max(0.02f, weaponDef->explosionShockwaveLifetime);
        shockwave.rotation.z = startRotation;
        shockwave.endRotation.z = endRotation;
        shockwave.scale = weaponDef->explosionShockwaveStartScale * forceScale *
            std::max(0.01f, weaponDef->explosionShockwaveModelBaseScale);
        shockwave.endScale = weaponDef->explosionShockwaveEndScale * forceScale *
            std::max(0.01f, weaponDef->explosionShockwaveModelBaseScale);
        shockwave.alpha = weaponDef->explosionShockwaveStartAlpha;
        shockwave.endAlpha = weaponDef->explosionShockwaveEndAlpha;
        shockwave.useEndAlpha = true;
        shockwave.color = weaponDef->explosionShockwaveStartColor;
        shockwave.endColor = weaponDef->explosionShockwaveEndColor;
        shockwave.useEndColor = true;
        shockwave.billboardText = false;
        shockwave.replayType = "explosion_shockwave";
        shockwave.assetId = weaponId;
        shockwave.modelPath = weaponDef->explosionShockwaveModel;
        shockwave.modelNormalize = weaponDef->explosionShockwaveNormalize;
        shockwave.texturePath = weaponDef->explosionShockwaveTexture;
        EffectPartSystem::instance().spawn(shockwave);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Weapons, StructuredLevel::Important,
            "explosion.shockwave.spawn", weaponId,
            "spawned configured explosion shockwave", 0,
            nlohmann::json{{"weapon", weaponId},
                           {"texture", weaponDef->explosionShockwaveTexture},
                           {"model", weaponDef->explosionShockwaveModel},
                           {"start_rotation_deg", startRotation},
                           {"end_rotation_deg", endRotation},
                           {"start_scale", shockwave.scale},
                           {"end_scale", shockwave.endScale},
                           {"impact_force_scale", forceScale},
                           {"start_alpha", shockwave.alpha},
                           {"end_alpha", shockwave.endAlpha},
                           {"random_rotation_min_deg", randomMin},
                           {"random_rotation_max_deg", randomMax},
                           {"lifetime_seconds", shockwave.maxLifetime}});
    }

    // Surface-aligned explosion mark decal.
    const glm::vec3& decalPosition = markPosition ? *markPosition : position;
    const glm::vec3& decalNormal = markNormal ? *markNormal : surfaceNormal;
    bool markSpawned = false;
    if (spawnMark && weaponDef && !weaponDef->explosionMarkTexture.empty() &&
        glm::length(decalNormal) > 0.001f) {
        markSpawned = EffectPartSystem::instance().spawnExplosionMark(
            decalPosition, decalNormal, forceScale);
    }

    // Fireball sphere — expanding, clamped to a visible bright color
    if (expCfg.sphere.enabled)
    {
        EffectPart sphere;
        sphere.position = position;
        sphere.maxLifetime = (float)expCfg.sphere.lifetimeTicks / 60.0f;
        sphere.scale = expCfg.sphere.startRadius * forceScale;
        sphere.endScale = expCfg.sphere.endRadius * forceScale;
        sphere.color = glm::clamp(expCfg.sphere.startColor * expCfg.sphere.brightnessStart, 0.0f, 1.0f);
        sphere.alpha = expCfg.sphere.alphaStart;
        sphere.billboardText = false;
        sphere.replayType = weaponId + "_explosion_sphere";
        EffectPartSystem::instance().spawn(sphere);
    }

    // World impact burst — config-gated
    if (expCfg.hitBurst)
    {
        HitEvent ev;
        ev.position = position;
        ev.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        ev.direction = glm::vec3(0.0f);
        ev.hitWorld = true;
        ev.hitEntity = false;
        ev.damage = 0;
        ev.attacker = attacker;
        ev.weaponSource = weaponId;
        HitEffects::onHit(ev);
    }
    return markSpawned;
}



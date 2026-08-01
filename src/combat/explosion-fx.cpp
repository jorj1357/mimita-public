// 07 31 2026, 18 41
/* purpose
* Implements the shared explosion visual spawner used by rocket and grenade launchers.
* Owns the single source of truth for explosion fireball, smoke, debris, and sound visuals.
* Does NOT apply damage, knockback, camera shake, or server authority.
* Does NOT send packets or render viewmodels.
*/

#include "combat/explosion-fx.h"

#include <algorithm>
#include <cstdlib>

#include "audio/audio.h"
#include "config/weapon-hitfx-config.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"

void spawnExplosionFx(const glm::vec3& position, const std::string& weaponId,
                      const std::string& attacker, float sizeScale)
{
    // Explosion sound
    const char* sound = weaponId == "grenade_launcher"
        ? "grenadelauncher/grenadelauncherexplode"
        : "rocketlauncher/rocketlauncherexplode";
    playWorldSound(sound, position, 1.0f, 1.0f, 50.0f);

    // Explosion flash, debris, and red 1-tick impact sphere
    EffectPartSystem::instance().spawnMuzzleFlash(position, weaponId + "_explosion", sizeScale);
    EffectPartSystem::instance().spawnWorldDebris(position, glm::vec3(0.0f, 0.0f, 1.0f), 3.0f, sizeScale);
    EffectPartSystem::instance().spawnImpactSphereTick(position, {1.0f, 0.15f, 0.05f}, 0.5f);

    // Smoke burst — config-driven for rockets and grenades
    const auto& expCfg = WeaponHitFxConfig::instance().explosionBurstFor(weaponId);
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
            EffectPartSystem::instance().spawn(part);
        }
    }

    // Fireball sphere — expanding, clamped to a visible bright color
    if (expCfg.sphere.enabled)
    {
        EffectPart sphere;
        sphere.position = position;
        sphere.maxLifetime = (float)expCfg.sphere.lifetimeTicks / 60.0f;
        sphere.scale = expCfg.sphere.startRadius;
        sphere.endScale = expCfg.sphere.endRadius;
        sphere.color = glm::clamp(expCfg.sphere.startColor * expCfg.sphere.brightnessStart, 0.0f, 1.0f);
        sphere.alpha = expCfg.sphere.alphaStart;
        sphere.billboardText = false;
        sphere.replayType = weaponId + "_explosion_sphere";
        EffectPartSystem::instance().spawn(sphere);
    }

    // World impact burst
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

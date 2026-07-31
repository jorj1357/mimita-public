// 07 31 2026, 14 50
/* purpose
* Implements grenade launcher firing, spark trail update, and projectile spawn.
* Owns the local client grenade prediction and fire-time muzzle flash.
* Does NOT own server projectile authority or packet routing.
* Does NOT render viewmodels or run the fixed-step simulation tick.
*/
#include "weapon-grenade-launcher.h"
#include "weapon-types.h"

#include <glm/gtc/constants.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/projectile-render.h"
#include "config/size-scaling-config.h"
#include "debug/debug-log.h"
#include "entities/player.h"
#include "effects/effect-part.h"
#include "npc/npc.h"
#include "pobjects/persistent-physics.h"
#include "replay/replay.h"
#include "world/world.h"

namespace WeaponGrenadeLauncher {

static float cp(const WeaponDefinition& def, const char* key, float fallback)
{
    return def.customParams.count(key) ? def.customParams.at(key) : fallback;
}

void fire(const WeaponDefinition& def, WeaponRuntime& runtime,
           Player& owner, const glm::vec3& muzzlePos, const glm::vec3& aimDir)
{
    (void)runtime;
    PersistentPhysicsConfig cfg;
    cfg.shape = PersistentShape::Cylinder;
    const auto& sc = SizeScalingConfig::instance().data();
    float s = std::max(owner.sizeScale, 0.001f);
    cfg.radius = (def.projectileRadius > 0.0f ? def.projectileRadius : 0.4f) * sc.scale(1.0f, sc.projectileSizeExponent, s);
    cfg.height = cfg.radius * 1.6f;
    cfg.mass = cp(def, "mass", 1.5f);
    cfg.gravity = cp(def, "gravity", 20.0f);
    cfg.drag = cp(def, "drag", 0.15f);
    cfg.angularDrag = cp(def, "angularDrag", 0.3f);
    cfg.restitution = cp(def, "bounceRestitution", 0.35f);
    cfg.friction = cp(def, "bounceFriction", 0.5f);
    cfg.minBounceSpeed = cp(def, "minBounceSpeed", 0.1f);
    cfg.maxBounceCount = (int)cp(def, "maxBounceCount", 10.0f);
    cfg.lifetime = def.projectileLifetime > 0.0f ? def.projectileLifetime : 3.0f;
    cfg.explosionRadius = cp(def, "splashRadius", 8.0f) * sc.scale(1.0f, sc.explosionRadiusExponent, s);
    cfg.explosionDamage = cp(def, "rocketDirectDamage", 150.0f) * sc.scale(1.0f, sc.projectileDamageExponent, s);
    cfg.explosionKnockback = cp(def, "knockbackStrength", 160.0f) * sc.scale(1.0f, sc.knockbackExponent, s);
    cfg.explosionSelfKnockbackMul = cp(def, "selfKnockbackMultiplier", 0.8f);
    cfg.splashExponent = cp(def, "splashExponent", 2.0f);
    cfg.armingDistance = cp(def, "armingDistance", 2.0f);
    cfg.armingTime = cp(def, "armingTime", 0.0f);
    cfg.explosionSound = "grenadelauncher/grenadelauncherexplode";
    cfg.spawnSound = def.soundShoot;

    float forwardSpeed = def.projectileSpeed > 0.0f ? def.projectileSpeed : cp(def, "forwardSpeed", 18.0f);
    float upBias = cp(def, "upBias", 4.0f);
    float angSpeed = cp(def, "angSpeed", 6.0f);

    glm::vec3 vel = aimDir * forwardSpeed + glm::vec3(0.0f, 0.0f, upBias);

    // Deterministic angular velocity (same on client and server — no rand())
    glm::vec3 forward = glm::length(aimDir) > 0.0001f ? glm::normalize(aimDir) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 refUp = std::fabs(forward.z) < 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(forward, refUp));
    glm::vec3 angVel = right * angSpeed;

    PersistentPhysicsSystem::instance().spawn(cfg, muzzlePos, vel, angVel,
                                              0, owner.username, def.id, owner.pos);
    EffectPartSystem::instance().spawnMuzzleFlash(muzzlePos, owner.username, owner.sizeScale);

    Debug::log(Debug::Category::Weapons, "[GRENADE LAUNCHER] fired pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) "
               "projectileSpeed=%.1f radius=%.2f armingDist=%.1f bounceRest=%.2f bounceFric=%.2f\n",
               muzzlePos.x, muzzlePos.y, muzzlePos.z, vel.x, vel.y, vel.z,
               forwardSpeed, cfg.radius, cfg.armingDistance, cfg.restitution, cfg.friction);
}

void update(const WeaponDefinition& def, WeaponRuntime& runtime,
            Player& owner, NpcSystem& npcs, const World& world,
            Camera& camera, float dt)
{
    (void)runtime;
    (void)npcs;
    (void)world;

    // Read config values for sparks (hot-reloadable). Projectile visuals are
    // read in WeaponSystem::render during the render pass.
    bool sparkEnabled = cp(def, "sparkEnabled", 1.0f) > 0.0f;
    float sparkEmissionRate = cp(def, "sparkEmissionRate", 45.0f);
    int sparkParticlesPerEm = (int)cp(def, "sparkParticlesPerEmission", 2.0f);
    glm::vec3 sparkSpawnOffset(
        cp(def, "sparkSpawnOffsetX", 0.0f),
        cp(def, "sparkSpawnOffsetY", 0.0f),
        cp(def, "sparkSpawnOffsetZ", -0.7f));
    float sparkSpawnRadius = cp(def, "sparkSpawnRadius", 0.04f);
    float sparkInheritVel = cp(def, "sparkInheritVelocity", 0.15f);
    float sparkSpeed = cp(def, "sparkSpeed", 5.0f);
    float sparkSpeedRand = cp(def, "sparkSpeedRandom", 2.0f);
    float sparkSpreadDeg = cp(def, "sparkSpreadDegrees", 35.0f);
    float sparkLifetime = cp(def, "sparkLifetime", 0.35f);
    float sparkLifetimeRand = cp(def, "sparkLifetimeRandom", 0.15f);
    float sparkSize = cp(def, "sparkSize", 0.05f);
    float sparkEndSize = cp(def, "sparkEndSize", 0.01f);
    float sparkSizeRand = cp(def, "sparkSizeRandom", 0.005f);
    float sparkGravity = cp(def, "sparkGravity", 8.0f);
    float sparkDrag = cp(def, "sparkDrag", 0.2f);
    glm::vec3 sparkColor(
        cp(def, "sparkColorR", 1.0f),
        cp(def, "sparkColorG", 0.75f),
        cp(def, "sparkColorB", 0.2f));
    float sparkAlpha = cp(def, "sparkColorA", 1.0f);
    glm::vec3 sparkEndColor(
        cp(def, "sparkEndColorR", 1.0f),
        cp(def, "sparkEndColorG", 0.1f),
        cp(def, "sparkEndColorB", 0.0f));
    float sparkEndAlpha = cp(def, "sparkEndColorA", 0.0f);

    static std::unordered_map<uint32_t, float> grenadeSparkAccumulators;

    for (const PersistentPhysicsObject& obj : PersistentPhysicsSystem::instance().objects()) {
        if (obj.ownerName != owner.username || obj.weaponId != def.id)
            continue;
        if (obj.exploded || obj.sleeping)
            continue;

        // ── Textured rendering (handled in WeaponSystem::render during render pass) ──

        // ── Config-controlled spark trail ──
        if (sparkEnabled) {
            float& acc = grenadeSparkAccumulators[obj.id];
            acc += sparkEmissionRate * dt;
            while (acc >= 1.0f) {
                acc -= 1.0f;
                for (int pi = 0; pi < sparkParticlesPerEm; ++pi) {
                    EffectPart p;
                    glm::vec3 velDir = glm::length(obj.velocity) > 0.001f
                        ? glm::normalize(obj.velocity) : glm::vec3(0.0f, 0.0f, 1.0f);
                    glm::vec3 trailPos = obj.position +
                        velDir * sparkSpawnOffset.z +
                        glm::vec3(
                            ((float)rand() / RAND_MAX - 0.5f) * sparkSpawnRadius * 2.0f,
                            ((float)rand() / RAND_MAX - 0.5f) * sparkSpawnRadius * 2.0f,
                            ((float)rand() / RAND_MAX - 0.5f) * sparkSpawnRadius * 2.0f);

                    p.position = trailPos;
                    float a = (float)rand() / RAND_MAX;
                    float spreadRad = sparkSpreadDeg * 3.14159265f / 180.0f;
                    glm::vec3 spread(
                        (a - 0.5f) * spreadRad * 2.0f,
                        (a - 0.5f) * spreadRad * 2.0f,
                        (a - 0.5f) * spreadRad * 2.0f);
                    p.velocity = obj.velocity * sparkInheritVel + spread * sparkSpeed +
                        glm::vec3(
                            ((float)rand() / RAND_MAX - 0.5f) * sparkSpeedRand * 2.0f,
                            ((float)rand() / RAND_MAX - 0.5f) * sparkSpeedRand * 2.0f,
                            ((float)rand() / RAND_MAX - 0.5f) * sparkSpeedRand * 2.0f);
                    float lf = sparkLifetime + ((float)rand() / RAND_MAX - 0.5f) * sparkLifetimeRand * 2.0f;
                    p.lifetime = 0.0f;
                    p.maxLifetime = std::max(0.05f, lf);
                    float sz = sparkSize + ((float)rand() / RAND_MAX - 0.5f) * sparkSizeRand * 2.0f;
                    p.scale = std::max(0.005f, sz);
                    p.endScale = std::max(0.001f, sparkEndSize);
                    p.color = sparkColor;
                    p.alpha = sparkAlpha;
                    p.gravity = sparkGravity;
                    p.affectedByGravity = std::fabs(sparkGravity) > 0.001f;
                    p.replayType = "grenade_spark";
                    EffectPartSystem::instance().spawn(p);
                }
            }
        }

        // ── Debug ──
        if (DebugConfig::DEBUG_WEAPON_COLLISION) {
            Debug::log(Debug::Category::Weapons,
                "[GRENADE] id=%u pos=(%.1f %.1f %.1f) age=%.1f lifetime=%.1f sleep=%d bounce=%d sparkAcc=%.1f\n",
                obj.id, obj.position.x, obj.position.y, obj.position.z,
                obj.age, obj.cfg.lifetime, (int)obj.sleeping, obj.bounceCount,
                grenadeSparkAccumulators[obj.id]);
        }
    }

    // Clean up accumulators for exploded/removed grenades
    for (auto it = grenadeSparkAccumulators.begin(); it != grenadeSparkAccumulators.end(); ) {
        bool found = false;
        for (const PersistentPhysicsObject& obj : PersistentPhysicsSystem::instance().objects()) {
            if (obj.id == it->first) { found = true; break; }
        }
        if (!found)
            it = grenadeSparkAccumulators.erase(it);
        else
            ++it;
    }
}

} // namespace WeaponGrenadeLauncher

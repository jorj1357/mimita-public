#include "weapon-grenade-launcher.h"
#include "weapon-types.h"

#include <glm/gtc/constants.hpp>

#include "audio/audio.h"
#include "camera.h"
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
    cfg.explosionSound = "weapon/bomb/explosion2";
    cfg.spawnSound = def.soundShoot;

    float forwardSpeed = def.projectileSpeed > 0.0f ? def.projectileSpeed : cp(def, "forwardSpeed", 18.0f);
    float upBias = cp(def, "upBias", 4.0f);
    float angSpeed = cp(def, "angSpeed", 6.0f);

    glm::vec3 vel = aimDir * forwardSpeed + glm::vec3(0.0f, 0.0f, upBias);
    glm::vec3 angVel(0.0f, 0.0f, angSpeed);

    PersistentPhysicsSystem::instance().spawn(cfg, muzzlePos, vel, angVel,
                                              0, owner.username, def.id, owner.pos);

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
    (void)camera;
    for (const PersistentPhysicsObject& obj : PersistentPhysicsSystem::instance().objects()) {
        if (obj.ownerName != owner.username || obj.weaponId != def.id)
            continue;
        if (obj.exploded || obj.sleeping)
            continue;

        float normalizedAge = obj.age / obj.cfg.lifetime;

        EffectPart trail;
        trail.position = obj.position;
        trail.color = {0.8f, 0.8f, 0.8f};
        trail.velocity = -obj.velocity * 0.5f;
        trail.lifetime = 0.5f * (1.0f - normalizedAge * 0.5f);
        trail.maxLifetime = trail.lifetime;
        trail.scale = 0.08f * (1.0f - normalizedAge * 0.3f);
        trail.endScale = 0.0f;
        trail.alpha = 0.3f * (1.0f - normalizedAge * 0.5f);
        trail.gravity = -5.0f;
        trail.affectedByGravity = true;
        trail.replayType = "smoke";
        EffectPartSystem::instance().spawn(trail);
    }
}

} // namespace WeaponGrenadeLauncher

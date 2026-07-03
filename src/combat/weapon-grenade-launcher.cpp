#include "weapon-grenade-launcher.h"
#include "weapon-types.h"

#include <cstdio>
#include <glm/gtc/constants.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "entities/player.h"
#include "effects/effect-part.h"
#include "npc/npc.h"
#include "pobjects/persistent-physics.h"
#include "replay/replay.h"
#include "world/world.h"

namespace WeaponGrenadeLauncher {

void fire(const WeaponDefinition& def, WeaponRuntime& runtime,
           Player& owner, const glm::vec3& muzzlePos, const glm::vec3& aimDir)
{
    PersistentPhysicsConfig cfg;
    cfg.shape = PersistentShape::Cylinder;
    cfg.radius = 0.25f;
    cfg.height = 0.4f;
    cfg.mass = 1.5f;
    cfg.gravity = 20.0f;
    cfg.drag = 0.15f;
    cfg.angularDrag = 0.3f;
    cfg.restitution = 0.35f;
    cfg.friction = 0.5f;
    cfg.lifetime = 3.0f;
    cfg.explodeOnContact = false;
    cfg.collideWithPlayer = true;
    cfg.collideWithNpcs = true;
    cfg.collideWithWorld = true;
    cfg.explosionRadius = def.customParams.count("splashRadius")
        ? def.customParams.at("splashRadius") : 8.0f;
    cfg.explosionDamage = def.customParams.count("rocketDirectDamage")
        ? def.customParams.at("rocketDirectDamage") : 150.0f;
    cfg.explosionKnockback = def.customParams.count("knockbackStrength")
        ? def.customParams.at("knockbackStrength") : 160.0f;
    cfg.explosionSound = "weapon/bomb/explosion2";
    cfg.spawnSound = def.soundShoot;

    float forwardSpeed = 18.0f;
    float upBias = 4.0f;
    float angSpeed = 6.0f;

    glm::vec3 vel = aimDir * forwardSpeed + glm::vec3(0.0f, 0.0f, upBias);
    glm::vec3 angVel(0.0f, 0.0f, angSpeed);

    PersistentPhysicsSystem::instance().spawn(cfg, muzzlePos, vel, angVel,
                                              0, owner.username, def.id);

    printf("[GRENADE LAUNCHER] fired pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f)\n",
           muzzlePos.x, muzzlePos.y, muzzlePos.z, vel.x, vel.y, vel.z);
}

void update(const WeaponDefinition& def, WeaponRuntime& runtime,
            Player& owner, NpcSystem& npcs, const World& world,
            Camera& camera, float dt)
{
    (void)runtime;
    for (const PersistentPhysicsObject& obj : PersistentPhysicsSystem::instance().objects()) {
        if (obj.ownerName != owner.username || obj.weaponId != def.id)
            continue;
        if (obj.exploded || obj.sleeping)
            continue;

        float age = obj.age;
        float normalizedAge = age / obj.cfg.lifetime;

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

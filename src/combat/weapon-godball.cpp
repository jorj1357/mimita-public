#include "weapon-godball.h"
#include "weapon-audio.h"
#include "weapon-types.h"
#include "weapon-execution.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "camera.h"
#include "combat/death-system.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "ui/hitmarker.h"
#include "world/world.h"

namespace WeaponGodball {

static constexpr float MAX_GODBALL_SPEED = 120.0f;
static constexpr float GODBALL_GRAVITY = 15.0f;

extern void updatePhysics(GodballPhysics& phys, const WeaponDefinition& def,
                          WeaponRuntime& runtime, Player& owner,
                          const Camera& camera, float dt);
extern void checkOverlaps(GodballPhysics& phys, const WeaponDefinition& def,
                          WeaponRuntime& runtime, Player& owner,
                          NpcSystem& npcs, const Camera& camera, float dt);
extern void render(const Camera& camera, const GodballPhysics& phys, const glm::vec3& handPos);
extern void renderDebug(const Camera& camera, const GodballPhysics& phys,
                        const WeaponRuntime& runtime, const glm::vec3& handPos);

glm::vec3 getHandPosition(const Player& player) {
    for (const PhysicalBodyPart& part : player.physicalBody.parts) {
        if (part.name == "rightArm") {
            glm::vec3 center = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 boundsSize = part.collider.localMax - part.collider.localMin;
            int axis = boundsSize.y > boundsSize.x ? 1 : 0;
            if (boundsSize.z > boundsSize[axis]) axis = 2;
            float minDist = std::fabs(part.collider.localMin[axis]);
            float maxDist = std::fabs(part.collider.localMax[axis]);
            center[axis] = maxDist >= minDist ? part.collider.localMax[axis] : part.collider.localMin[axis];
            return glm::vec3(part.worldTransform * glm::vec4(center, 1.0f));
        }
    }
    return player.pos + glm::vec3(0.0f, 0.5f, 1.0f);
}

void spawnBall(GodballPhysics& phys, const WeaponDefinition& def, const Player& owner) {
    glm::vec3 handPos = getHandPosition(owner);
    phys.radius = def.customParams.count("ballRadius") ? def.customParams.at("ballRadius") : 0.5f;
    phys.mass = def.customParams.count("ballMass") ? def.customParams.at("ballMass") : 0.1f;
    phys.ropeLength = def.customParams.count("ropeLength") ? def.customParams.at("ropeLength") : 2.5f;
    phys.ropeStiffness = def.customParams.count("ropeStiffness") ? def.customParams.at("ropeStiffness") : 50.0f;
    phys.ropeDamping = def.customParams.count("ropeDamping") ? def.customParams.at("ropeDamping") : 2.0f;
    phys.linearDamping = def.customParams.count("linearDamping") ? def.customParams.at("linearDamping") : 0.005f;

    float yawRad = glm::radians(owner.yaw);
    glm::vec3 ownerForward = glm::vec3(std::cos(yawRad), std::sin(yawRad), 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(ownerForward, glm::vec3(0.0f, 0.0f, 1.0f)));
    if (glm::length(right) < 0.1f) right = glm::vec3(1.0f, 0.0f, 0.0f);

    phys.position = handPos - glm::vec3(0.0f, 0.0f, phys.ropeLength * 0.8f) + right * 0.5f;
    phys.prevPosition = phys.position;
    phys.velocity = right * 4.0f + owner.vel * 0.5f;
    phys.angularVelocity = glm::vec3(0.0f);
    phys.active = true;
    phys.ropeTension = 0.0f;
    phys.constraintDist = 0.0f;
    phys.prevHandPos = handPos;
    phys.hasPrevHandPos = true;
    phys.handedEnergy = 0.0f;
    phys.radialVel = 0.0f;
    phys.tangentialSpeed = 0.0f;
    phys.lastFrameHit = false;
    phys.lastHitNormal = glm::vec3(0.0f);
    phys.impactEvents.clear();
    phys.hitstopTimer = 0.0f;

    Debug::warn(Debug::Category::Weapons,
        "[GODBALL_DBG] SPAWN owner=%s pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) "
        "rope=%.2f radius=%.2f yaw=%.1f handPos=(%.2f,%.2f,%.2f)",
        owner.username.c_str(),
        phys.position.x, phys.position.y, phys.position.z,
        phys.velocity.x, phys.velocity.y, phys.velocity.z,
        phys.ropeLength, phys.radius, owner.yaw,
        handPos.x, handPos.y, handPos.z);

    if (DebugConfig::DEBUG_GODBALL) {
        printf("[GODBALL] spawned at (%.2f, %.2f, %.2f) vel (%.2f, %.2f, %.2f) rope=%.2f radius=%.2f\n",
               phys.position.x, phys.position.y, phys.position.z,
               phys.velocity.x, phys.velocity.y, phys.velocity.z,
               phys.ropeLength, phys.radius);
    }
}

void despawnBall(GodballPhysics& phys) {
    phys.active = false;
    Debug::warn(Debug::Category::Weapons, "[GODBALL_DBG] DESPAWN");
    if (DebugConfig::DEBUG_GODBALL) {
        printf("[GODBALL] despawned\n");
    }
}

float damageFromSpeed(const WeaponDefinition& def, float speed) {
    const float minDmg = WeaponExecution::paramOr(def, "minDamage", 5.0f);
    const float maxDmg = WeaponExecution::paramOr(def, "maxDamage", 25.0f);
    const float minSpd = WeaponExecution::paramOr(def, "minDamageSpeed", 0.0f);
    const float maxSpd = WeaponExecution::paramOr(def, "maxDamageSpeed", 40.0f);
    const float exp = WeaponExecution::paramOr(def, "damageCurveExponent", 1.2f);

    const float denom = std::max(maxSpd - minSpd, 0.001f);
    const float t = std::clamp((speed - minSpd) / denom, 0.0f, 1.0f);
    const float curved = std::pow(t, exp);
    float result = minDmg + curved * (maxDmg - minDmg);

    Debug::warn(Debug::Category::Weapons,
        "[GODBALL_DBG] DAMAGE_FORMULA speed=%.2f minDmg=%.1f maxDmg=%.1f "
        "minSpd=%.1f maxSpd=%.1f exp=%.2f t=%.3f curved=%.3f result=%.1f",
        speed, minDmg, maxDmg, minSpd, maxSpd, exp, t, curved, result);

    return result;
}

float computeDamage(const GodballPhysics& phys, const WeaponDefinition& def,
                     const Player& owner, const Player& target,
                     const glm::vec3& overlapPoint) {
    (void)owner;
    (void)overlapPoint;
    const float ballSpeed = glm::length(phys.velocity);
    const float totalDamage = damageFromSpeed(def, ballSpeed);

    Debug::warn(Debug::Category::Weapons,
        "[GODBALL_DBG] COMPUTE_DAMAGE target=%s speed=%.2f totalDamage=%.1f",
        target.username.c_str(), ballSpeed, totalDamage);

    if (DebugConfig::DEBUG_GODBALL) {
        printf("[GODBALL DAMAGE] speed=%.1f total=%.1f\n",
               ballSpeed, totalDamage);
    }

    return totalDamage;
}

} // namespace WeaponGodball

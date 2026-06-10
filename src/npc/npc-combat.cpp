#include "npc-combat.h"
#include "npc.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

#include "audio/audio.h"
#include "effects/effect-part.h"
#include "world/world.h"

namespace {

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
float difficulty01(float difficulty) { return clamp01(difficulty / 10.0f); }

float random01(unsigned int& state)
{
    state = state * 1664525u + 1013904223u;
    return (float)((state >> 8) & 0x00ffffffu) / (float)0x01000000u;
}

glm::vec3 rotatePlanar(glm::vec3 v, float radians)
{
    float c = std::cos(radians);
    float s = std::sin(radians);
    return {
        v.x * c - v.y * s,
        v.x * s + v.y * c,
        0.0f
    };
}

// Line of sight check using world triangles
bool lineOfSight(glm::vec3 from, glm::vec3 to, const World& world)
{
    glm::vec3 dir = to - from;
    float maxDist = glm::length(dir);
    if (maxDist < 0.1f) return false;
    dir /= maxDist;

    for (const CollisionTriangle& tri : world.collisionMesh.triangles)
    {
        glm::vec3 e1 = tri.b - tri.a;
        glm::vec3 e2 = tri.c - tri.a;
        glm::vec3 pVec = glm::cross(dir, e2);
        float det = glm::dot(e1, pVec);
        if (std::fabs(det) < 0.0001f) continue;

        float invDet = 1.0f / det;
        glm::vec3 tVec = from - tri.a;
        float u = glm::dot(tVec, pVec) * invDet;
        if (u < 0.0f || u > 1.0f) continue;

        glm::vec3 qVec = glm::cross(tVec, e1);
        float v = glm::dot(dir, qVec) * invDet;
        if (v < 0.0f || u + v > 1.0f) continue;

        float t = glm::dot(e2, qVec) * invDet;
        // if (t > 0.1f && t < maxDist - 0.3f)
        // v2 test if LOS disable is bad thsi mroe forgivig 
        if (t > 0.1f && t < maxDist - 1.5f)
            return false;
    }
    return true;
}

} // anonymous namespace

glm::vec3 NpcCombat::aimAtTarget(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos, glm::vec3 targetVel)
{
    float d01 = difficulty01(npc.difficulty);
    glm::vec3 toTarget = targetPos - npcPos;
    float dist = glm::length(toTarget);
    if (dist < 0.1f)
        return glm::vec3{1.0f, 0.0f, 0.0f};

    // Predict target movement based on difficulty
    float predictionFactor = 0.05f + npc.tuning.prediction * 0.55f;
    glm::vec3 predicted = targetPos + targetVel * predictionFactor;
    glm::vec3 aimDir = predicted - npcPos;
    aimDir.z = 0.0f;
    float aimLen = glm::length(aimDir);
    if (aimLen < 0.001f)
        aimDir = {1.0f, 0.0f, 0.0f};
    else
        aimDir /= aimLen;

    // Add accuracy error based on difficulty
    float error = (random01(const_cast<Npc&>(npc).rngState) * 2.0f - 1.0f) * npc.tuning.aimErrorRadians;
    aimDir = rotatePlanar(aimDir, error);

    return aimDir;
}

bool NpcCombat::tryFire(Npc& npc, const World& world, Player& player, float dt)
{
    if (npc.attackCooldown > 0.0f)
        return false;

    float dist = npc.sensors.targetDistance;
    if (dist > 30.0f)
        return false;

    glm::vec3 npcPos = npc.body.pos;
    npcPos.z += 0.8f;

    glm::vec3 aimDir = aimAtTarget(npc, npcPos, npc.sensors.targetPos, npc.sensors.targetVel);
    glm::vec3 predictedTarget = npcPos + aimDir * dist;

    // Line of sight check
    if (!lineOfSight(npcPos, predictedTarget, world))
        return false;

    float d01 = difficulty01(npc.difficulty);
    float baseDmg = 6.0f + d01 * 14.0f;
    int dmg = std::max(1, (int)(baseDmg + (random01(npc.rngState) * 12.0f - 6.0f)));

    glm::vec3 knockbackDir = predictedTarget - npcPos;
    knockbackDir.z = 0.2f;

    glm::vec3 hitPoint = predictedTarget;

    EffectPartSystem::instance().spawnBloodSphereBurst(
        hitPoint, aimDir, (float)dmg / 60.0f,
        npc.body.username, player.username);
    EffectPartSystem::instance().spawnEntityImpact(
        hitPoint, -aimDir, npc.body.username, player.username);
    EffectPartSystem::instance().spawnProjectedBlood(
        hitPoint, aimDir, (float)dmg, dist, "torso", world);
    EffectPartSystem::instance().spawnMuzzleFlash(npcPos, npc.body.username);
    EffectPartSystem::instance().spawnTracer(npcPos, hitPoint, npc.body.username);
    AudioManager::instance().play(
        {"revolvershoot", AudioCategory::Weapons, true, npcPos, 0.5f, 0.9f, 40.0f, npc.id});

    player.takeDamage(dmg, knockbackDir, 8.0f);

    // float cd = 1.2f - d01 * 0.9f;
    // cd = std::max(cd, 0.3f);

    // buffed 6 7 2026 
    float cd =
        0.45f -
        d01 * 0.35f;

    cd = std::max(cd, 0.06f);
    npc.attackCooldown = cd;

    return true;
}

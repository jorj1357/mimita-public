#include "npc-combat.h"
#include "npc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/constants.hpp>

#include "audio/audio.h"
#include "config.h"
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
        if (t > 0.1f && t < maxDist - 1.5f)
            return false;
    }
    return true;
}

} // anonymous namespace

float NpcCombat::aimErrorDegrees(float difficulty)
{
    if (difficulty <= 1.0f) return 12.0f;
    if (difficulty >= 10.0f) return 0.5f;
    float t = (difficulty - 1.0f) / 9.0f;
    if (t <= 0.444f) {
        float p = t / 0.444f;
        return 12.0f - p * (12.0f - 4.0f);
    } else {
        float p = (t - 0.444f) / (1.0f - 0.444f);
        return 4.0f - p * (4.0f - 0.5f);
    }
}

// Ray vs capsule (line segment + radius) intersection test
bool NpcCombat::rayCapsule(const glm::vec3& origin, const glm::vec3& dir,
                           const glm::vec3& a, const glm::vec3& b, float radius,
                           float& outDist, glm::vec3& outNormal)
{
    glm::vec3 ab = b - a;
    float abLen = glm::length(ab);
    glm::vec3 abDir = abLen > 0.001f ? ab / abLen : glm::vec3(0.0f, 0.0f, 1.0f);

    // Find closest point on ray to segment ab
    glm::vec3 oa = origin - a;
    float dot_ray_ab = glm::dot(dir, abDir);
    float dot_ab_ab = 1.0f;
    float dot_oa_ray = glm::dot(oa, dir);
    float dot_oa_ab = glm::dot(oa, abDir);

    float denom = 1.0f - dot_ray_ab * dot_ray_ab;
    if (std::fabs(denom) < 0.0001f) {
        // Parallel
        float t = glm::dot(-oa, dir);
        float s = 0.0f;
        glm::vec3 closestRay = origin + dir * t;
        glm::vec3 closestSeg = a;
        glm::vec3 diff = closestRay - closestSeg;
        float dist = glm::length(diff);
        if (dist < radius) {
            outDist = t;
            outNormal = dist > 0.001f ? diff / dist : -dir;
            return true;
        }
        return false;
    }

    float t = (dot_oa_ray - dot_ray_ab * dot_oa_ab) / denom;
    float s = (dot_ray_ab * dot_oa_ray - dot_oa_ab) / denom;

    s = std::clamp(s, 0.0f, abLen);

    glm::vec3 closestRay = origin + dir * t;
    glm::vec3 closestSeg = a + abDir * s;
    glm::vec3 diff = closestRay - closestSeg;
    float dist = glm::length(diff);

    if (dist < radius && t > 0.0f) {
        outDist = t;
        outNormal = dist > 0.001f ? diff / dist : -dir;
        return true;
    }
    return false;
}

glm::vec3 NpcCombat::aimAtTarget(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos, glm::vec3 targetVel)
{
    glm::vec3 toTarget = targetPos - npcPos;
    float dist = glm::length(toTarget);
    if (dist < 0.1f)
        return glm::vec3{1.0f, 0.0f, 0.0f};

    float predictionFactor = 0.05f + npc.tuning.prediction * 0.55f;
    glm::vec3 predicted = targetPos + targetVel * predictionFactor;
    glm::vec3 aimDir = predicted - npcPos;
    aimDir.z = 0.0f;
    float aimLen = glm::length(aimDir);
    if (aimLen < 0.001f)
        aimDir = {1.0f, 0.0f, 0.0f};
    else
        aimDir /= aimLen;

    float errorDeg = aimErrorDegrees(npc.difficulty);
    float errorRad = glm::radians(errorDeg) * (random01(const_cast<Npc&>(npc).rngState) * 2.0f - 1.0f);
    aimDir = rotatePlanar(aimDir, errorRad);

    return aimDir;
}

bool NpcCombat::tryFire(Npc& npc, const World& world, Player& player, float dt)
{
    if (npc.attackCooldown > 0.0f)
        return false;

    // --- Range check: 150m max ---
    float dist = npc.sensors.targetDistance;
    if (dist > 150.0f)
        return false;

    // --- Aim settle timer ---
    // NPC must track target briefly before firing
    float settleTime = 0.1f + (1.0f - difficulty01(npc.difficulty)) * 0.5f;
    npc.aimTimer += dt;
    if (npc.aimTimer < settleTime)
        return false;

    // --- Line of sight check ---
    glm::vec3 npcPos = npc.body.pos;
    npcPos.z += 0.8f;
    glm::vec3 toPlayer = player.pos - npcPos;
    float losDist = glm::length(toPlayer);
    if (!lineOfSight(npcPos, player.pos, world))
        return false;

    // --- Compute aim direction ---
    glm::vec3 aimDir = aimAtTarget(npc, npcPos, npc.sensors.targetPos, npc.sensors.targetVel);
    float aimErrorDeg = aimErrorDegrees(npc.difficulty);
    bool canSeePlayer = true; // LOS already confirmed

    // --- Raycast: fire a bullet at the player ---
    // Get player capsule for hit detection
    Capsule playerCap = player.getCapsule();
    float hitDist = 0.0f;
    glm::vec3 hitNormal;
    bool hitPlayer = rayCapsule(npcPos, aimDir, playerCap.a, playerCap.b, playerCap.r,
                                 hitDist, hitNormal);

    // Check if world geometry blocks the shot before reaching the player
    bool blockedByWorld = false;
    float worldDist = hitDist;
    if (hitPlayer) {
        for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
            glm::vec3 e1 = tri.b - tri.a;
            glm::vec3 e2 = tri.c - tri.a;
            glm::vec3 pVec = glm::cross(aimDir, e2);
            float det = glm::dot(e1, pVec);
            if (std::fabs(det) < 0.0001f) continue;
            float invDet = 1.0f / det;
            glm::vec3 tVec = npcPos - tri.a;
            float u = glm::dot(tVec, pVec) * invDet;
            if (u < 0.0f || u > 1.0f) continue;
            glm::vec3 qVec = glm::cross(tVec, e1);
            float v = glm::dot(aimDir, qVec) * invDet;
            if (v < 0.0f || u + v > 1.0f) continue;
            float t = glm::dot(e2, qVec) * invDet;
            if (t > 0.1f && t < hitDist - 0.3f) {
                blockedByWorld = true;
                worldDist = t;
                break;
            }
        }
    }

    // --- Fire the shot (visual effects always play) ---
    float d01 = difficulty01(npc.difficulty);
    float baseDmg = 6.0f + d01 * 14.0f;
    int dmg = std::max(1, (int)(baseDmg + (random01(npc.rngState) * 12.0f - 6.0f)));

    glm::vec3 hitPoint = npcPos + aimDir * (hitPlayer && !blockedByWorld ? hitDist : dist * 0.8f);
    glm::vec3 knockbackDir = aimDir;
    knockbackDir.z = 0.2f;

    EffectPartSystem::instance().spawnMuzzleFlash(npcPos, npc.body.username);
    EffectPartSystem::instance().spawnTracer(npcPos, hitPoint, npc.body.username);

    if (!blockedByWorld && hitPlayer) {
        EffectPartSystem::instance().spawnBloodSphereBurst(
            hitPoint, aimDir, (float)dmg / 60.0f,
            npc.body.username, player.username);
        EffectPartSystem::instance().spawnEntityImpact(
            hitPoint, -aimDir, npc.body.username, player.username);
        EffectPartSystem::instance().spawnProjectedBlood(
            hitPoint, aimDir, (float)dmg, dist, "torso", world);
    } else {
        EffectPartSystem::instance().spawnWorldImpact(hitPoint, -aimDir);
    }

    AudioManager::instance().play(
        {"revolvershoot", AudioCategory::Weapons, true, npcPos, 0.5f, 0.9f, 40.0f, npc.id});

    // --- Apply damage only if actually hit ---
    bool dealtDamage = false;
    if (!blockedByWorld && hitPlayer) {
        player.takeDamage(dmg, knockbackDir, 8.0f);
        dealtDamage = true;
    }

    // Reset aim timer after firing
    npc.aimTimer = 0.0f;

    // Cooldown scales with distance: closer = faster firing
    float rangeFactor = std::clamp(dist / 150.0f, 0.0f, 1.0f);
    float cd = 0.45f - d01 * 0.35f;
    cd += rangeFactor * 0.5f; // longer range = slower fire
    cd = std::max(cd, 0.06f);
    npc.attackCooldown = cd;

    // --- Debug logging ---
    if (DebugConfig::DEBUG_NPC_COMBAT) {
        printf("[NPC SHOT] id=%u hit=%d blocked=%d dist=%.1f aimError=%.2f canSee=%d "
               "dmg=%d settleTime=%.2f\n",
               npc.id, (int)dealtDamage, (int)blockedByWorld, dist, aimErrorDeg, (int)canSeePlayer,
               dmg, settleTime);
    }

    return true;
}

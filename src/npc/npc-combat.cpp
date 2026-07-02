#include "npc-combat.h"
#include "npc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/constants.hpp>

#include "audio/audio.h"
#include "combat/weapon-fire.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "config.h"
#include "debug/debug-log.h"
#include "effects/effect-part.h"
#include "physics/movement/physics-collision.h"
#include "physics/physics-types.h"
#include "world/world.h"
#include "npc/npc-internal.h"

bool gNpcForceHit = false;

namespace {

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

    AABB rayBounds;
    rayBounds.min = glm::min(from, to);
    rayBounds.max = glm::max(from, to);
    std::vector<int> candidates;
    appendChunkTrianglesForAABB(world, rayBounds, 0.1f, candidates);

    for (int ti : candidates)
    {
        if (ti < 0 || ti >= (int)world.collisionMesh.triangles.size()) continue;
        const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
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

float effectiveRange(const WeaponDefinition& def)
{
    auto it = def.customParams.find("effectiveRange");
    if (it != def.customParams.end()) return it->second;
    if (def.projectileSpeed > 0.0f)
        return def.projectileSpeed * std::max(def.projectileLifetime, 2.0f);
    return 150.0f;
}

} // anonymous namespace

float NpcCombat::aimErrorDegrees(float difficulty)
{
    float d = std::clamp(difficulty, 1.0f, 10.0f);
    float t = (d - 1.0f) / 9.0f;
    // Difficulty 1 = 6 deg (poor), Diff 5 = 1.5 deg (competent), Diff 10 = 0.2 deg (near-perfect)
    if (t <= 0.5f)
        return 6.0f - (t / 0.5f) * 4.5f;
    else
        return 1.5f - ((t - 0.5f) / 0.5f) * 1.3f;
}

bool NpcCombat::rayCapsule(const glm::vec3& origin, const glm::vec3& dir,
                           const glm::vec3& a, const glm::vec3& b, float radius,
                           float& outDist, glm::vec3& outNormal)
{
    const float MAX_RAY = 1000.0f;

    glm::vec3 ab = b - a;
    float abLen = glm::length(ab);
    if (abLen < 0.0001f) return false;
    glm::vec3 abDir = ab / abLen;

    glm::vec3 rayEnd = origin + dir * MAX_RAY;
    glm::vec3 raySeg = rayEnd - origin;
    float rayLen = glm::length(raySeg);
    if (rayLen < 0.0001f) return false;
    glm::vec3 rayDir = raySeg / rayLen;

    glm::vec3 r = origin - a;
    float a_dot_b = glm::dot(rayDir, abDir);
    float a_dot_r = glm::dot(rayDir, r);
    float b_dot_r = glm::dot(abDir, r);

    float denom = 1.0f - a_dot_b * a_dot_b;
    float t, s;

    if (std::fabs(denom) < 0.0001f) {
        t = 0.0f;
        s = b_dot_r;
    } else {
        t = (a_dot_r - a_dot_b * b_dot_r) / denom;
        s = (a_dot_b * a_dot_r - b_dot_r) / denom;
    }

    struct { float s, t, d; } best = {s, 0.0f, 1e30f};
    auto checkS = [&](float sVal) {
        float tVal = (sVal <= 0.0f) ? a_dot_r : a_dot_r + a_dot_b * sVal;
        tVal = std::clamp(tVal, 0.0f, MAX_RAY);
        float dVal = glm::length((origin + rayDir * tVal) - (a + abDir * sVal));
        if (dVal < best.d) { best = {sVal, tVal, dVal}; }
    };
    if (s >= 0.0f && s <= abLen) {
        checkS(s);
    } else {
        checkS(0.0f);
        checkS(abLen);
    }
    t = best.t; s = best.s;

    glm::vec3 closestRay = origin + rayDir * t;
    glm::vec3 closestSeg = a + abDir * s;
    glm::vec3 diff = closestRay - closestSeg;
    float dist = glm::length(diff);

    if (dist < radius) {
        outDist = t;
        outNormal = dist > 0.001f ? diff / dist : -rayDir;
        return true;
    }
    return false;
}

glm::vec3 NpcCombat::aimAtTarget(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos, glm::vec3 targetVel)
{
    glm::vec3 aimTarget = targetPos + glm::vec3(0.0f, 0.0f, 0.8f);
    glm::vec3 toTarget = aimTarget - npcPos;
    float dist = glm::length(toTarget);
    if (dist < 0.1f)
        return glm::vec3{1.0f, 0.0f, 0.0f};

    float predictionFactor = 0.05f + npc.tuning.prediction * 0.55f;
    glm::vec3 predicted = aimTarget + targetVel * predictionFactor;
    glm::vec3 aimDir = predicted - npcPos;
    float aimLen = glm::length(aimDir);
    if (aimLen < 0.001f)
        aimDir = {1.0f, 0.0f, 0.0f};
    else
        aimDir /= aimLen;

    float errorDeg = aimErrorDegrees(npc.difficulty);
    float errorRad = glm::radians(errorDeg) * (random01(const_cast<Npc&>(npc).rngState) * 2.0f - 1.0f);
    {
        float c = std::cos(errorRad);
        float s = std::sin(errorRad);
        float dx = aimDir.x * c - aimDir.y * s;
        float dy = aimDir.x * s + aimDir.y * c;
        aimDir.x = dx; aimDir.y = dy;
    }

    return aimDir;
}

static glm::vec3 aimAtTargetProjectile(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos, glm::vec3 targetVel, float projectileSpeed, float gravity)
{
    glm::vec3 aimTarget = targetPos + glm::vec3(0.0f, 0.0f, 0.8f);
    glm::vec3 toTarget = aimTarget - npcPos;
    float dist = glm::length(toTarget);
    if (dist < 0.1f || projectileSpeed < 0.1f)
        return glm::vec3{1.0f, 0.0f, 0.0f};

    float travelTime = dist / projectileSpeed;
    float d01 = difficulty01(npc.difficulty);
    float predictionQuality = 0.3f + d01 * 0.6f;
    travelTime *= predictionQuality;

    glm::vec3 predicted = aimTarget + targetVel * travelTime;

    if (gravity > 0.0f)
    {
        predicted.z += 0.5f * gravity * travelTime * travelTime;
    }

    glm::vec3 aimDir = predicted - npcPos;
    float aimLen = glm::length(aimDir);
    if (aimLen < 0.001f)
        aimDir = {1.0f, 0.0f, 0.0f};
    else
        aimDir /= aimLen;

    float errorDeg = NpcCombat::aimErrorDegrees(npc.difficulty) * 0.8f;
    float errorRad = glm::radians(errorDeg) * (random01(const_cast<Npc&>(npc).rngState) * 2.0f - 1.0f);
    {
        float c = std::cos(errorRad);
        float s = std::sin(errorRad);
        float dx = aimDir.x * c - aimDir.y * s;
        float dy = aimDir.x * s + aimDir.y * c;
        aimDir.x = dx; aimDir.y = dy;
    }

    return aimDir;
}

bool NpcCombat::tryFire(Npc& npc, const World& world, Player& player, float dt)
{
    if (npc.attackCooldown > 0.0f)
    {
        Debug::logThrottled(Debug::Category::NpcCombat, "npc-cd",
            DebugConfig::PRINT_INTERVAL, "[NPC] id=%u fire blocked: attackCooldown=%.2f\n",
            npc.id, npc.attackCooldown);
        return false;
    }

    float dist = npc.sensors.targetDistance;

    const WeaponDefinition* def = WeaponRegistry::instance().get(npc.body.equippedWeaponId);
    if (!def)
    {
        Debug::log(Debug::Category::NpcCombat,
            "[NPC] id=%u fire blocked: no weapon equipped\n", npc.id);
        return false;
    }

    float maxRange = effectiveRange(*def);
    if (dist > maxRange)
    {
        Debug::logThrottled(Debug::Category::NpcCombat, "npc-range",
            DebugConfig::PRINT_INTERVAL, "[NPC] id=%u fire blocked: dist=%.1f > maxRange=%.1f\n",
            npc.id, dist, maxRange);
        return false;
    }

    auto& rt = npc.body.weaponRuntimes[def->id];

    // No ammo and no reserve — can't fire
    if (rt.currentAmmo <= 0 && rt.reserveAmmo <= 0)
    {
        Debug::log(Debug::Category::NpcCombat,
            "[NPC] id=%u fire blocked: no ammo (current=%d reserve=%d)\n",
            npc.id, rt.currentAmmo, rt.reserveAmmo);
        return false;
    }

    // Reload if empty
    if (rt.currentAmmo <= 0 && rt.reserveAmmo > 0 && !rt.isReloading)
    {
        rt.isReloading = true;
        rt.reloadTimer = def->reloadTime;
        Debug::log(Debug::Category::NpcCombat,
            "[NPC RELOAD] npc=%u weapon=%s started reload=%.2fs ammo=%d reserve=%d",
            npc.id, def->id.c_str(), def->reloadTime, rt.currentAmmo, rt.reserveAmmo);
        return false;
    }

    // Currently reloading
    if (rt.isReloading)
    {
        Debug::logThrottled(Debug::Category::NpcCombat, "npc-reloading",
            DebugConfig::PRINT_INTERVAL, "[NPC] id=%u fire blocked: reloading %.2fs left\n",
            npc.id, rt.reloadTimer);
        return false;
    }

    // No ammo in magazine
    if (rt.currentAmmo <= 0)
    {
        Debug::log(Debug::Category::NpcCombat,
            "[NPC] id=%u fire blocked: magazine empty but reserve=%d (should have triggered reload)\n",
            npc.id, rt.reserveAmmo);
        return false;
    }

    // Minimal aim settle — only enough to prevent instant firing on state transitions
    // The weapon's own fireDelay handles fire rate. We just need a tiny grace period.
    float settleTime = 0.05f;
    npc.aimTimer += dt;
    if (npc.aimTimer < settleTime)
    {
        Debug::logThrottled(Debug::Category::NpcCombat, "npc-aim",
            DebugConfig::PRINT_INTERVAL, "[NPC] id=%u fire blocked: aiming %.2f/%.2f\n",
            npc.id, npc.aimTimer, settleTime);
        return false;
    }

    glm::vec3 npcPos = npc.body.pos;
    npcPos.z += 0.8f;
    if (!lineOfSight(npcPos, player.pos + glm::vec3(0.0f, 0.0f, 0.8f), world))
    {
        Debug::logThrottled(Debug::Category::NpcCombat, "npc-los",
            DebugConfig::PRINT_INTERVAL, "[NPC] id=%u fire blocked: no line of sight\n",
            npc.id);
        return false;
    }

    glm::vec3 aimDir;
    bool isProjectile = (def->behaviorType == WeaponBehaviorType::Projectile ||
                         def->behaviorType == WeaponBehaviorType::RocketLauncher);

    if (isProjectile && def->projectileSpeed > 0.0f)
    {
        float gravity = 0.0f;
        auto git = def->customParams.find("projectileGravity");
        if (git != def->customParams.end()) gravity = git->second;
        aimDir = aimAtTargetProjectile(npc, npcPos, npc.sensors.targetPos, npc.sensors.targetVel, def->projectileSpeed, gravity);
    }
    else
    {
        aimDir = aimAtTarget(npc, npcPos, npc.sensors.targetPos, npc.sensors.targetVel);
    }

    // Decrement ammo BEFORE firing
    if (def->magazineSize > 0)
        rt.currentAmmo = std::max(0, rt.currentAmmo - 1);

    RevolverShotResult shot;
    if (isProjectile && def->spread > 0.0f)
    {
        glm::vec3 spreadDir = WeaponFire::computeSpreadDirection(aimDir, def->spread, const_cast<Npc&>(npc).rngState);
        shot = WeaponFire::tryFireHitscanDir(*def, rt, npc.body, world, npcPos, spreadDir, &player);
    }
    else
    {
        shot = WeaponFire::tryFireHitscanDir(*def, rt, npc.body, world, npcPos, aimDir, &player);
    }

    Debug::log(Debug::Category::NpcCombat,
        "[NPC SHOT] npc=%u weapon=%s ammo=%d/%d dist=%.1f hit=%d dmg=%.0f aimDir=(%.3f %.3f %.3f) error=%.1fdeg",
        npc.id, def->id.c_str(), rt.currentAmmo, def->magazineSize, dist,
        (int)shot.hitEntity, shot.damage,
        aimDir.x, aimDir.y, aimDir.z,
        aimErrorDegrees(npc.difficulty));

    npc.aimTimer = 0.0f;

    // Use weapon's natural fire delay as cooldown
    float d01 = difficulty01(npc.difficulty);
    float cd = def->fireDelay * (1.1f - d01 * 0.2f);
    cd = std::max(cd, def->fireDelay * 0.85f);
    npc.attackCooldown = cd;
    return true;
}

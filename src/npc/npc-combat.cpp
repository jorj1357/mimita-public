#include "npc-combat.h"
#include "npc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/constants.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/weapon-fire.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "combat/weapon-audio.h"
#include "combat/weapon-rocket-launcher.h"
#include "config.h"
#include "debug/debug-log.h"
#include "effects/effect-part.h"
#include "physics/movement/physics-collision.h"
#include "physics/physics-types.h"
#include "world/world.h"
#include "npc/npc-internal.h"
#include "npc/npc-difficulty-config.h"

// Shared NPC projectile state (rockets, grenades, etc.)
static RocketLauncherState gNpcRocketState;

namespace {

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
    appendChunkTrianglesForAABB(world, rayBounds, 0.1f, candidates, "npcLineOfSight");

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

static void logAimDebug(const Npc& npc, const WeaponDefinition& def,
                         const glm::vec3& idealDir,
                         const glm::vec3& finalDir,
                         float maxErrorDeg, float actualErrorDeg)
{
    FILE* f = fopen("logs/npc_aim_debug.txt", "a");
    if (!f) return;
    float angleDiff = glm::degrees(std::acos(std::clamp(glm::dot(idealDir, finalDir), -1.0f, 1.0f)));
    fprintf(f, "NPC id=%u weapon=%s maxError=%.1fdeg actualError=%.1fdeg "
               "targetDir=(%.3f %.3f %.3f) finalDir=(%.3f %.3f %.3f) angleDiff=%.1fdeg\n",
            npc.id, def.id.c_str(),
            maxErrorDeg, actualErrorDeg,
            idealDir.x, idealDir.y, idealDir.z,
            finalDir.x, finalDir.y, finalDir.z,
            angleDiff);
    fclose(f);
}

} // anonymous namespace

float NpcCombat::aimErrorDegrees(float difficulty)
{
    const auto& cfg = NpcDifficultyConfig::instance().settings();
    if (cfg.forceHit)
        return 0.0f;
    float d01 = difficulty01(difficulty);
    return cfg.maxAngularErrorDegrees * (1.0f - d01 * cfg.difficultyErrorScale);
}

float NpcCombat::maxAngularErrorForAccuracy(float acc)
{
    if (acc > 0.0f) {
        return 45.0f / (1.0f + acc * 4.0f);
    }
    if (acc == 0.0f) return 45.0f;
    float t = -acc;
    float clamped = std::min(t / 400.0f, 1.0f);
    return 45.0f + 315.0f * clamped * clamped;
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

glm::vec3 NpcCombat::aimAtTarget(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos)
{
    glm::vec3 aimTarget = targetPos + glm::vec3(0.0f, 0.0f, 0.8f);
    glm::vec3 aimDir = aimTarget - npcPos;
    float aimLen = glm::length(aimDir);
    if (aimLen < 0.001f)
        return glm::vec3{1.0f, 0.0f, 0.0f};
    aimDir /= aimLen;

    float errorDeg = aimErrorDegrees(npc.difficulty);
    float maxErrorRad = glm::radians(errorDeg);
    if (maxErrorRad > 0.0001f) {
        float theta = random01(const_cast<Npc&>(npc).rngState) * glm::two_pi<float>();
        float radius = random01(const_cast<Npc&>(npc).rngState);
        radius = std::sqrt(radius) * std::tan(maxErrorRad);
        glm::vec3 up = std::fabs(aimDir.z) < 0.99f
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(aimDir, up));
        glm::vec3 fwd = glm::normalize(glm::cross(right, aimDir));
        aimDir = glm::normalize(aimDir +
            (right * std::cos(theta) + fwd * std::sin(theta)) * radius);
    }

    return aimDir;
}

static float computeFireAggression(const Npc& npc)
{
    float base = npc.tuning.aggression;
    float healthFrac = (float)npc.body.currentHp / (float)npc.body.maxHp;
    float lowHealth = (1.0f - healthFrac) * 0.3f;
    float closeTarget = npc.sensors.targetDistance < 5.0f ? 0.3f : 0.0f;
    float recentlyHit = npc.hitReactionTimer > 0.0f ? 0.4f : 0.0f;
    float visible = npc.cachedLoSBlocked ? 0.0f : 0.2f;
    return glm::clamp(base + lowHealth + closeTarget + recentlyHit + visible, 0.0f, 1.0f);
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

    const float dmgMul = NpcDifficultyConfig::instance().settings().damageMultiplier;

    const WeaponDefinition* def = WeaponRegistry::instance().get(npc.body.equippedWeaponId);
    if (!def)
    {
        Debug::log(Debug::Category::NpcCombat,
            "[NPC] id=%u fire blocked: no weapon equipped\n", npc.id);
        return false;
    }

    // Range gate is effectively unlimited so an NPC never idles purely because
    // a target is far away. The hitscan/projectile itself still has its own
    // weapon range, so damage only lands within reach; the NPC always fires.
    if (npcFiringRangeBlocked(dist))
    {
        Debug::logThrottled(Debug::Category::NpcCombat, "npc-range",
            DebugConfig::PRINT_INTERVAL, "[NPC] id=%u fire blocked: dist=%.1f > cap=%.1f\n",
            npc.id, dist, NpcCombat::kNpcFiringRangeCap);
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

    glm::vec3 aimDir;
    glm::vec3 npcPos = npc.body.pos + npcMuzzleOffset();

    // Use cached LOS from updateOneNpc (avoids redundant gather + triangle loop)
    if (npc.cachedLoSBlocked)
    {
        Debug::logThrottled(Debug::Category::NpcCombat, "npc-los",
            DebugConfig::PRINT_INTERVAL, "[NPC] id=%u fire blocked: no line of sight (cached)\n",
            npc.id);
        return false;
    }
    aimDir = aimAtTarget(npc, npcPos, npc.sensors.targetPos);

    glm::vec3 planarAim = glm::normalize(glm::vec3(aimDir.x, aimDir.y, 0.0f));
    if (glm::length(planarAim) > 0.001f)
        npc.currentFacing = planarAim;

    glm::vec3 idealDir = glm::normalize(npc.sensors.targetPos + glm::vec3(0.0f, 0.0f, 0.8f) - npcPos);
    float errorDeg = aimErrorDegrees(npc.difficulty);
    float angleDiff = glm::degrees(std::acos(std::clamp(glm::dot(idealDir, aimDir), -1.0f, 1.0f)));

    logAimDebug(npc, *def, idealDir, aimDir, errorDeg, angleDiff);

    printf("[NPC SHOT] id=%u dist=%.1fm maxError=%.1fdeg "
           "ideal=(%.3f,%.3f,%.3f) final=(%.3f,%.3f,%.3f) diff=%.1fdeg "
           "weapon=%s ready=%s\n",
           npc.id, dist,
           errorDeg,
           idealDir.x, idealDir.y, idealDir.z,
           aimDir.x, aimDir.y, aimDir.z,
           angleDiff,
           def->id.c_str(),
           rt.currentAmmo > 0 ? "yes" : "empty");

    // Decrement ammo BEFORE firing
    if (def->magazineSize > 0)
        rt.currentAmmo = std::max(0, rt.currentAmmo - 1);

    bool fired = false;
    // Actual fired direction and endpoint (the damage trace). The NPC body
    // facing and the server broadcast read these so look == shoot == hit.
    glm::vec3 firedDir = aimDir;
    glm::vec3 shotEnd = npcPos + aimDir * 100.0f;
    switch (def->behaviorType) {
    case WeaponBehaviorType::Hitscan:
    {
        RevolverShotResult shot;
        if (def->spread > 0.0f) {
            glm::vec3 spreadDir = WeaponFire::computeSpreadDirection(aimDir, def->spread, const_cast<Npc&>(npc).rngState);
            shot = WeaponFire::tryFireHitscanDir(*def, rt, npc.body, world, npcPos, spreadDir, &player, dmgMul);
        } else {
            shot = WeaponFire::tryFireHitscanDir(*def, rt, npc.body, world, npcPos, aimDir, &player, dmgMul);
        }
        fired = shot.fired;
        if (fired) { firedDir = shot.direction; shotEnd = shot.end; }
        Debug::log(Debug::Category::NpcCombat, "[NPC SHOT] id=%u weapon=%s hitscan hit=%d damage=%.0f\n",
                   npc.id, def->id.c_str(), (int)shot.hitEntity, shot.damage);
        break;
    }
    case WeaponBehaviorType::Projectile:
    case WeaponBehaviorType::RocketLauncher:
    {
        WeaponRocketLauncher::fire(gNpcRocketState, *def, rt, npc.body, npcPos, aimDir);
        fired = true;
        firedDir = aimDir;
        {
            float range = effectiveRange(*def);
            shotEnd = npcPos + aimDir * (range > 0.0f ? range : 100.0f);
        }
        Debug::log(Debug::Category::NpcCombat, "[NPC SHOT] id=%u weapon=%s rocketLauncher dir=(%.2f %.2f %.2f)\n",
                   npc.id, def->id.c_str(), aimDir.x, aimDir.y, aimDir.z);
        break;
    }
    case WeaponBehaviorType::Melee:
    case WeaponBehaviorType::Swordsword:
    {
        // Melee: use hitscan at close range for now (future: full melee AI)
        RevolverShotResult shot = WeaponFire::tryFireHitscanDir(*def, rt, npc.body, world, npcPos, aimDir, &player, dmgMul);
        fired = shot.fired;
        if (fired) { firedDir = shot.direction; shotEnd = shot.end; }
        Debug::log(Debug::Category::NpcCombat, "[NPC SHOT] id=%u weapon=%s melee(approx) hit=%d\n",
                   npc.id, def->id.c_str(), (int)shot.hitEntity);
        break;
    }
    case WeaponBehaviorType::Godball:
    case WeaponBehaviorType::GrenadeLauncher:
    {
        // Fallback: use hitscan (godball/grenade AI not yet implemented)
        RevolverShotResult shot = WeaponFire::tryFireHitscanDir(*def, rt, npc.body, world, npcPos, aimDir, &player, dmgMul);
        fired = shot.fired;
        if (fired) { firedDir = shot.direction; shotEnd = shot.end; }
        Debug::log(Debug::Category::NpcCombat, "[NPC SHOT] id=%u weapon=%s fallback-hitscan (full AI pending)\n",
                   npc.id, def->id.c_str());
        break;
    }
    }

    // Snap the body to the actual fired direction and remember the shot so the
    // server broadcast sends the true tracer (look == shoot == damage line).
    if (fired)
    {
        glm::vec3 planarFired(firedDir.x, firedDir.y, 0.0f);
        const float planarLen = glm::length(planarFired);
        if (planarLen > 0.001f)
        {
            planarFired /= planarLen;
            npc.currentFacing = planarFired;
            npc.body.yaw = glm::degrees(std::atan2(planarFired.y, planarFired.x));
        }
        npc.lastShotOrigin = npcPos;
        npc.lastShotEnd = shotEnd;
        npc.hasLastShot = true;
    }

    // Variable fire delay: blend between min and max based on aggression + rhythm.
    // The difficulty config overrides the weapon's own fire_delay for NPCs.
    const auto& npcCfg = NpcDifficultyConfig::instance().settings();
    float minDelay = std::max(npcCfg.fireDelayMin, def->fireDelay);
    float maxDelay = std::max(minDelay, npcCfg.fireDelayMax);
    float rawPos = random01(npc.rngState);
    float aggression = glm::clamp(computeFireAggression(npc) + npcCfg.aggressionBonus, 0.0f, 1.0f);
    npc.fireAggressionBias = aggression;
    float calm = 1.0f - aggression;
    float pos = rawPos - aggression * 0.35f + calm * 0.15f + npc.fireRhythmOffset * 0.2f;
    pos = glm::clamp(pos, 0.0f, 1.0f);
    npc.attackCooldown = minDelay + pos * (maxDelay - minDelay);
    Debug::log(Debug::Category::NpcCombat,
        "[NPC FIRE DECISION] npc=%u weapon=%s min=%.3f max=%.1f "
        "aggression=%.2f rhythm=%+.2f pos=%.2f result=%.3f\n",
        npc.id, def->id.c_str(), minDelay, maxDelay,
        aggression, npc.fireRhythmOffset, pos, npc.attackCooldown);
    return fired;
}

void NpcCombat::updateNpcProjectiles(const World& world, NpcSystem& npcSystem,
                                     const Camera& camera, float dt) {
    // Update NPC rocket launcher projectiles
    if (!gNpcRocketState.activeRockets.empty()) {
        // Need a weapon definition for rockets — find it from any NPC with rocket launcher
        static const RocketLauncherState* lastState = &gNpcRocketState;
        (void)lastState;
        // For now, the rockets will be inert and timeout after their lifetime.
        // Full update requires a WeaponDefinition which is accessed from the NPC.
        // This is a placeholder for the full implementation.
        Debug::log(Debug::Category::NpcCombat, "[NPC ROCKET] %zu active rockets (lifetime expiry only)\n",
                   gNpcRocketState.activeRockets.size());
    }
}

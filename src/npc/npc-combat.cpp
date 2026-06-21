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

bool gNpcForceHit = false;

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

    // Use chunk spatial hashing to only test triangles near the ray path
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

} // anonymous namespace

float NpcCombat::aimErrorDegrees(float difficulty)
{
    float d = std::clamp(difficulty, 1.0f, 10.0f);
    // Diff 1 = 12 deg, Diff 5 = 4 deg, Diff 10 = 0.5 deg
    float t = (d - 1.0f) / 9.0f;
    if (t <= 0.5f)
        return 12.0f - (t / 0.5f) * 8.0f;
    else
        return 4.0f - ((t - 0.5f) / 0.5f) * 3.5f;
}

// Ray vs capsule (line segment + radius) intersection test.
// Uses segment-segment closest-points to correctly handle rays
// where the infinite-line closest approach is behind the ray origin.
bool NpcCombat::rayCapsule(const glm::vec3& origin, const glm::vec3& dir,
                           const glm::vec3& a, const glm::vec3& b, float radius,
                           float& outDist, glm::vec3& outNormal)
{
    const float MAX_RAY = 1000.0f;

    glm::vec3 ab = b - a;
    float abLen = glm::length(ab);
    if (abLen < 0.0001f) return false;
    glm::vec3 abDir = ab / abLen;

    // Treat the ray as a finite segment for correct segment-segment distance
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

    // Check distance at the unclamped s (if within segment), or at both endpoints
    // when the infinite-line closest point falls outside the capsule segment.
    // The nearest-endpoint clamp alone misses the case where the ray passes closer
    // to the FAR end (e.g., NPC above player — s wants to be below capsule bottom,
    // clamps to bottom, but the actual closest approach is at the capsule top).
    struct { float s, t, d; } best = {s, 0.0f, 1e30f};
    auto checkS = [&](float sVal) {
        float tVal = (sVal <= 0.0f) ? a_dot_r : a_dot_r + a_dot_b * sVal;
        tVal = std::clamp(tVal, 0.0f, MAX_RAY);
        float dVal = glm::length((origin + rayDir * tVal) - (a + abDir * sVal));
        if (dVal < best.d) { best = {sVal, tVal, dVal}; }
    };
    if (s >= 0.0f && s <= abLen) {
        checkS(s); // unclamped point is within segment
    } else {
        checkS(0.0f);       // capsule bottom
        checkS(abLen);      // capsule top — may be closer than bottom!
    }
    t = best.t; s = best.s;

    glm::vec3 closestRay = origin + rayDir * t;
    glm::vec3 closestSeg = a + abDir * s;
    glm::vec3 diff = closestRay - closestSeg;
    float dist = glm::length(diff);

    Debug::log(Debug::Category::NpcCombat,
        "[RAY CAPSULE] closestRay=(%.3f %.3f %.3f) closestCapsule=(%.3f %.3f %.3f) "
        "distBetween=%.4f radius=%.4f t=%.4f s=%.4f",
        closestRay.x, closestRay.y, closestRay.z,
        closestSeg.x, closestSeg.y, closestSeg.z,
        dist, radius, t, s);

    if (dist < radius) {
        outDist = t;
        outNormal = dist > 0.001f ? diff / dist : -rayDir;
        return true;
    }
    return false;
}

glm::vec3 NpcCombat::aimAtTarget(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos, glm::vec3 targetVel)
{
    // Aim at the player's center mass (z = +0.8m above ground) instead of feet.
    // The NPC fires from chest height (npcPos.z ≈ 0.8). Without this offset the
    // natural aimDir would point slightly downward, but the old code forced z=0
    // which made the horizontal ray miss the player capsule at low aim error.
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
    // Apply error in the horizontal plane, preserving the natural vertical component
    {
        float c = std::cos(errorRad);
        float s = std::sin(errorRad);
        float dx = aimDir.x * c - aimDir.y * s;
        float dy = aimDir.x * s + aimDir.y * c;
        aimDir.x = dx; aimDir.y = dy;
        // Z component preserved so the ray passes through the player capsule
    }

    return aimDir;
}

bool NpcCombat::tryFire(Npc& npc, const World& world, Player& player, float dt)
{
    if (npc.attackCooldown > 0.0f)
        return false;

    float dist = npc.sensors.targetDistance;
    if (dist > 150.0f)
        return false;

    // Auto-equip weapon if not set
    if (npc.body.equippedSlot < 1 || npc.body.equippedWeaponId.empty()) {
        float d01 = difficulty01(npc.difficulty);
        if (d01 < 0.4f) {
            npc.body.equippedSlot = 1;
            npc.body.equippedWeaponId = "revolver";
        } else if (d01 < 0.7f) {
            npc.body.equippedSlot = (rand() % 2 == 0) ? 1 : 3;
            npc.body.equippedWeaponId = (npc.body.equippedSlot == 1) ? "revolver" : "shotgun";
        } else {
            npc.body.equippedSlot = (rand() % 2 == 0) ? 3 : 1;
            npc.body.equippedWeaponId = (npc.body.equippedSlot == 3) ? "shotgun" : "revolver";
        }
        Debug::log(Debug::Category::NpcCombat,
            "[NPC WEAPON] npc=%u auto-equipped slot=%d weapon=%s",
            npc.id, npc.body.equippedSlot, npc.body.equippedWeaponId.c_str());
    }

    const WeaponDefinition* def = WeaponRegistry::instance().get(npc.body.equippedWeaponId);
    if (!def) return false;

    // Initialize runtime if needed
    auto& rt = npc.body.weaponRuntimes[def->id];
    if (rt.currentAmmo <= 0 && def->magazineSize > 0) {
        rt.currentAmmo = def->magazineSize;
        auto it = def->customParams.find("reserveAmmo");
        rt.reserveAmmo = (it != def->customParams.end()) ? (int)it->second : 999;
    }

    float settleTime = 0.1f + (1.0f - difficulty01(npc.difficulty)) * 0.5f;
    npc.aimTimer += dt;
    if (npc.aimTimer < settleTime)
        return false;

    glm::vec3 npcPos = npc.body.pos;
    npcPos.z += 0.8f;
    if (!lineOfSight(npcPos, player.pos, world))
        return false;

    glm::vec3 aimDir = aimAtTarget(npc, npcPos, npc.sensors.targetPos, npc.sensors.targetVel);

    // Decrement ammo
    if (def->magazineSize > 0)
        rt.currentAmmo = std::max(0, rt.currentAmmo - 1);

    // Fire using shared weapon system
    RevolverShotResult shot = WeaponFire::tryFireHitscanDir(
        *def, rt, npc.body, world, npcPos, aimDir, &player);

    Debug::log(Debug::Category::NpcCombat,
        "[NPC WEAPON FIRE] npc=%u weapon=%s origin=(%.2f %.2f %.2f) "
        "aimDir=(%.2f %.2f %.2f) ammo=%d",
        npc.id, def->id.c_str(), npcPos.x, npcPos.y, npcPos.z,
        aimDir.x, aimDir.y, aimDir.z, rt.currentAmmo);
    Debug::log(Debug::Category::NpcCombat,
        "[NPC WEAPON RESULT] npc=%u fired=%d hitEntity=%d hitWorld=%d "
        "damage=%.0f hpAfter=%d",
        npc.id, (int)shot.fired, (int)shot.hitEntity, (int)shot.hitWorld,
        shot.damage, player.currentHp);

    Debug::log(Debug::Category::NpcCombat,
        "[NPC SHOT] hit=%d weapon=%s damage=%.0f hpAfter=%d dist=%.1f aimError=%.1fdeg",
        (int)shot.hitEntity, def->id.c_str(), shot.damage, player.currentHp, dist,
        aimErrorDegrees(npc.difficulty));

    npc.aimTimer = 0.0f;

    float d01 = difficulty01(npc.difficulty);
    float rangeFactor = std::clamp(dist / 150.0f, 0.0f, 1.0f);
    float cd = 0.45f - d01 * 0.35f;
    cd += rangeFactor * 0.5f;
    cd = std::max(cd, 0.06f);
    npc.attackCooldown = cd;
    return true;
}

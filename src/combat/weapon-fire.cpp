#include "weapon-fire.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "combat/death-system.h"
#include "config/player-settings.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "replay/replay.h"
#include "ui/hitmarker.h"

namespace WeaponFire {

bool rayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                 const CollisionTriangle& tri, float& distance) {
    glm::vec3 e1 = tri.b - tri.a;
    glm::vec3 e2 = tri.c - tri.a;
    glm::vec3 p = glm::cross(direction, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < 0.000001f) return false;
    float inv = 1.0f / det;
    glm::vec3 t = origin - tri.a;
    float u = glm::dot(t, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(t, e1);
    float v = glm::dot(direction, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    distance = glm::dot(e2, q) * inv;
    return distance > 0.0f;
}

bool rayAabb(const glm::vec3& origin, const glm::vec3& direction,
             const glm::vec3& mn, const glm::vec3& mx,
             float& distance, glm::vec3& normal) {
    float tmin = 0.0f;
    float tmax = 1000.0f;
    normal = glm::vec3(0.0f);
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(direction[axis]) < 0.000001f) {
            if (origin[axis] < mn[axis] || origin[axis] > mx[axis]) return false;
            continue;
        }
        float inv = 1.0f / direction[axis];
        float a = (mn[axis] - origin[axis]) * inv;
        float b = (mx[axis] - origin[axis]) * inv;
        float sign = -1.0f;
        if (a > b) { std::swap(a, b); sign = 1.0f; }
        if (a > tmin) {
            tmin = a;
            normal = glm::vec3(0.0f);
            normal[axis] = sign;
        }
        tmax = std::min(tmax, b);
        if (tmin > tmax) return false;
    }
    distance = tmin;
    return distance >= 0.0f;
}

static float partBaseDamage(const std::string& part, float height) {
    if (part == "head") return 100.0f;
    if (part == "torso") return height >= 0.5f ? 50.0f : 30.0f;
    if (part.find("Arm") != std::string::npos) return height >= 0.5f ? 20.0f : 10.0f;
    if (part.find("Leg") != std::string::npos) return height >= 0.5f ? 25.0f : 15.0f;
    return 10.0f;
}

glm::vec3 computeSpreadDirection(const glm::vec3& baseDir, float spreadDegrees, unsigned int& rngState) {
    if (spreadDegrees <= 0.0f) return baseDir;
    rngState = rngState * 1103515245u + 12345u;
    float theta = ((float)(rngState & 0x7FFF) / 32767.0f) * 6.2831853f;
    rngState = rngState * 1103515245u + 12345u;
    float radius = ((float)(rngState & 0x7FFF) / 32767.0f) * std::tan(glm::radians(spreadDegrees));
    glm::vec3 up = std::fabs(baseDir.z) < 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(baseDir, up));
    glm::vec3 fwd = glm::normalize(glm::cross(right, up));
    return glm::normalize(baseDir + (right * std::cos(theta) + fwd * std::sin(theta)) * radius);
}

void applyRecoil(Player& shooter, const WeaponDefinition& def,
                 const glm::vec3& shotDirection, float& inOutRecoil, float dt) {
    const PlayerSettings& cfg = GetPlayerSettings();
    float recoilStrength = cfg.weaponRecoilStrength;
    glm::vec3 recoilDir(
        -shotDirection.x,
        -shotDirection.y,
        0.0f
    );
    if (glm::length(recoilDir) > 0.001f)
        recoilDir = glm::normalize(recoilDir);
    shooter.externalImpulse += recoilDir * recoilStrength;
    inOutRecoil = std::min(inOutRecoil + def.recoil * 0.25f, 8.0f);
}

int applyDamageToEntity(const DamageContext& ctx, Npc& victim,
                         const WeaponDefinition& def, Player& shooter,
                         NpcSystem& npcs, const glm::vec3& muzzlePos,
                         const glm::vec3& shotDirection) {
    float base = partBaseDamage(ctx.bodyPart, ctx.hitPosition.z);
    float distanceFalloffStart = 110.0f;
    auto it = def.customParams.find("distanceFalloffStart");
    if (it != def.customParams.end()) distanceFalloffStart = it->second;

    float minDamageFrac = 0.10f;
    it = def.customParams.find("minDamageFraction");
    if (it != def.customParams.end()) minDamageFrac = it->second;

    float minAngleFrac = 0.15f;
    it = def.customParams.find("minAngleFactor");
    if (it != def.customParams.end()) minAngleFrac = it->second;

    float distanceFactor = std::clamp(1.0f - ctx.distance / distanceFalloffStart, minDamageFrac, 1.0f);
    float angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, ctx.hitNormal)), minAngleFrac, 1.0f);
    float damage = std::min(base, std::max(base * distanceFactor * angleFactor, ctx.distance >= 100.0f ? 10.0f : 1.0f));
    int rounded = std::max(1, (int)std::round(damage));
    float knockback = damage * distanceFactor * (0.08f + angleFactor * 0.12f);

    victim.body.currentHp = std::max(0, victim.body.currentHp - rounded);
    victim.body.vel += shotDirection * knockback + glm::vec3(0, 0, knockback * 0.12f);
    victim.hitReactionTimer = 0.3f;

    EffectPartSystem::instance().spawnDamage(ctx.hitPosition, victim.body.username, rounded);

    if (GetPlayerSettings().debugCombat) {
        char debug[320];
        snprintf(debug, sizeof(debug),
                 "[DAMAGE] part=%s distance=%.2fm angleFactor=%.2f damage=%d knockback=%.2f",
                 ctx.bodyPart.c_str(), ctx.distance, angleFactor, rounded, knockback);
        Terminal::instance().addLog(debug);
    }

    if (victim.body.currentHp <= 0) {
        DeathSystem::instance().kill(
            victim.body,
            "npc_" + std::to_string(victim->id),
            "npc",
            shooter.username,
            shotDirection,
            18.0f);
        std::string line = shooter.username + " killed " + victim.body.username + " with " + def.displayName;
        Terminal::instance().addLog(line);
    }

    return rounded;
}

RevolverShotResult tryFireHitscan(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    const Camera& camera,
    Player& shooter,
    NpcSystem& npcs,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir)
{
    RevolverShotResult result;

    if (!def.soundShoot.empty()) {
        float rndPitch = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        float rndVolume = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        playWorldSound(def.soundShoot, muzzlePos, rndVolume, rndPitch, 80.0f);
    }

    result.fired = true;
    result.start = muzzlePos;

    constexpr float MAX_SHOT_DISTANCE = 100.0f;

    float cameraNearest = MAX_SHOT_DISTANCE;
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        float distance = 0.0f;
        if (rayTriangle(camera.pos, camera.front, tri, distance))
            cameraNearest = std::min(cameraNearest, distance);
    }
    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
        npc.body.updateModelWorldTransforms();
        for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
            float distance = 0.0f;
            glm::vec3 normal;
            if (rayAabb(camera.pos, camera.front, center - half, center + half, distance, normal))
                cameraNearest = std::min(cameraNearest, distance);
        }
    }

    glm::vec3 cameraTarget = camera.pos + camera.front * cameraNearest;
    glm::vec3 shotDirection = cameraTarget - muzzlePos;
    if (glm::length(shotDirection) <= 0.001f)
        shotDirection = camera.front;
    shotDirection = glm::normalize(shotDirection);

    static unsigned int spreadRng = 1;
    shotDirection = computeSpreadDirection(shotDirection, def.spread, spreadRng);

    float nearest = MAX_SHOT_DISTANCE;
    bool hitWorld = false;
    glm::vec3 worldNormal = -shotDirection;
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        float distance = 0.0f;
        if (rayTriangle(muzzlePos, shotDirection, tri, distance) && distance < nearest) {
            nearest = distance;
            hitWorld = true;
            worldNormal = tri.normal;
        }
    }

    Npc* victim = nullptr;
    std::string hitPart;
    glm::vec3 hitNormal{0.0f};
    float localHeight = 0.5f;
    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
        npc.body.updateModelWorldTransforms();
        for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            glm::vec3 half = (part.collider.localMax - part.collider.localMin) * 0.5f;
            half = glm::max(half, glm::vec3(0.12f));
            float distance = 0.0f;
            glm::vec3 normal;
            if (rayAabb(muzzlePos, shotDirection, center - half, center + half, distance, normal) && distance < nearest) {
                nearest = distance;
                hitWorld = false;
                victim = &npc;
                hitPart = part.name;
                hitNormal = normal;
                glm::vec3 hit = muzzlePos + shotDirection * distance;
                localHeight = std::clamp((hit.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
            }
        }
    }

    result.end = muzzlePos + shotDirection * nearest;

    ReplayEffectEvent gunshotEvent;
    gunshotEvent.type = "gunshot";
    gunshotEvent.position = muzzlePos;
    gunshotEvent.direction = shotDirection;
    gunshotEvent.from = muzzlePos;
    gunshotEvent.to = result.end;
    gunshotEvent.sourceActorId = shooter.username;
    captureReplayEffect(gunshotEvent);

    EffectPartSystem::instance().spawnMuzzleFlash(muzzlePos, shooter.username);
    EffectPartSystem::instance().spawnTracer(muzzlePos, result.end, shooter.username);

    if (victim) {
        DamageContext ctx;
        ctx.baseDamage = def.damage;
        ctx.distance = nearest;
        ctx.angleFactor = std::clamp(std::fabs(glm::dot(-shotDirection, hitNormal)), 0.15f, 1.0f);
        ctx.bodyPart = hitPart;
        ctx.hitPosition = result.end;
        ctx.hitNormal = hitNormal;
        ctx.shotDirection = shotDirection;
        ctx.shooterId = 0;
        ctx.shooterName = shooter.username;

        int totalDamage = applyDamageToEntity(ctx, *victim, def, shooter, npcs, muzzlePos, shotDirection);

        result.hitEntity = true;
        result.bodyPart = hitPart;
        result.damage = (float)totalDamage;
        result.targetId = victim->id;
        hitmarker();

        EffectPartSystem::instance().spawnBloodSphereBurst(
            result.end, shotDirection, (float)totalDamage / 60.0f,
            shooter.username, "npc_" + std::to_string(victim->id));
        EffectPartSystem::instance().spawnEntityImpact(
            result.end, hitNormal, shooter.username, "npc_" + std::to_string(victim->id));
        EffectPartSystem::instance().spawnProjectedBlood(result.end, shotDirection, (float)totalDamage, nearest, hitPart, world);
        EffectPartSystem::instance().spawnBloodSpurt(
            result.end, shotDirection, shooter.username, "npc_" + std::to_string(victim->id));
        playWorldSound(def.soundHit, result.end, 0.85f, 1.0f, 35.0f);
    } else if (hitWorld) {
        EffectPartSystem::instance().spawnWorldImpact(result.end, worldNormal);
        EffectPartSystem::instance().spawnBulletImpact(result.end);
        EffectPartSystem::instance().spawnWorldDebris(result.end, worldNormal);
        playWorldSound("hitworld", result.end, 0.8f, 1.0f, 35.0f);
    }

    return result;
}

} // namespace WeaponFire
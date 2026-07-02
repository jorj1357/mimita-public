#include "weapon-fire.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "entities/player.h"
#include "network/multiplayer-context.h"
#include "world/world.h"
#include "npc/npc.h"
#include "replay/replay.h"

namespace WeaponFire {

namespace {
bool gDebugWeapon = false;
}

void setWeaponDebug(bool enabled) { gDebugWeapon = enabled; }
bool weaponDebugEnabled() { return gDebugWeapon; }

RevolverShotResult tryFireHitscan(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    const Camera& camera,
    Player& shooter,
    NpcSystem& npcs,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir,
    const std::unordered_map<uint32_t, Player>* remotePlayers)
{
    RevolverShotResult result;

    if (!def.soundShoot.empty()) {
        float rndPitch = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        float rndVolume = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        printf("[SOUND] weapon=%s event=shoot path=%s pitch=%.3f volume=%.3f\n",
               def.id.c_str(), def.soundShoot.c_str(), rndPitch, rndVolume);
        playWorldSound(def.soundShoot, muzzlePos, rndVolume, rndPitch, 80.0f);
    }

    result.fired = true;
    result.start = muzzlePos;

    // Fixed aim point: always 100 units in front of the camera.
    // This decouples shot direction from whatever world geometry happens to be under the crosshair.
    constexpr float AIM_DISTANCE = 100.0f;
    glm::vec3 aimPoint = camera.pos + camera.front * AIM_DISTANCE;
    glm::vec3 shotDirection = aimPoint - muzzlePos;
    if (glm::length(shotDirection) <= 0.001f)
        shotDirection = camera.front;
    shotDirection = glm::normalize(shotDirection);

    if (gDebugWeapon) {
        printf("[AIM FIXED] cameraPos=(%.2f %.2f %.2f) cameraFront=(%.3f %.3f %.3f) "
               "aimPoint=(%.2f %.2f %.2f) muzzlePos=(%.2f %.2f %.2f) "
               "shotDir=(%.4f %.4f %.4f)\n",
               camera.pos.x, camera.pos.y, camera.pos.z,
               camera.front.x, camera.front.y, camera.front.z,
               aimPoint.x, aimPoint.y, aimPoint.z,
               muzzlePos.x, muzzlePos.y, muzzlePos.z,
               shotDirection.x, shotDirection.y, shotDirection.z);
    }

    static unsigned int spreadRng = 1;
    shotDirection = computeSpreadDirection(shotDirection, def.spread, spreadRng);

    constexpr float MAX_SHOT_DISTANCE = 100.0f;
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
    uint32_t remoteTargetId = 0;
    const Player* remoteVictim = nullptr;
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
    if (remotePlayers) {
        for (const auto& entry : *remotePlayers) {
            const Player& remote = entry.second;
            if (remote.dead || remote.currentHp <= 0) continue;
            const Capsule capsule = remote.getCapsule();
            const glm::vec3 mn(
                remote.pos.x - capsule.r,
                remote.pos.y - capsule.r,
                capsule.a.z - capsule.r);
            const glm::vec3 mx(
                remote.pos.x + capsule.r,
                remote.pos.y + capsule.r,
                capsule.b.z + capsule.r);
            float distance = 0.0f;
            glm::vec3 normal;
            if (MimitaNet::gNetHitDebug) {
                printf("[NET HIT TEST] remote id=%u capsul mn=(%.1f,%.1f,%.1f) "
                       "mx=(%.1f,%.1f,%.1f) muzzle=(%.1f,%.1f,%.1f) "
                       "dir=(%.2f,%.2f,%.2f)\n",
                       entry.first, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z,
                       muzzlePos.x, muzzlePos.y, muzzlePos.z,
                       shotDirection.x, shotDirection.y, shotDirection.z);
            }
            if (rayAabb(muzzlePos, shotDirection, mn, mx, distance, normal) &&
                distance < nearest) {
                if (MimitaNet::gNetHitDebug)
                    printf("[NET HIT REMOTE] id=%u distance=%.2f\n",
                           entry.first, distance);
                nearest = distance;
                hitWorld = false;
                victim = nullptr;
                remoteVictim = &remote;
                remoteTargetId = entry.first;
                hitNormal = normal;
                glm::vec3 hit = muzzlePos + shotDirection * distance;
                localHeight = std::clamp(
                    (hit.z - mn.z) / std::max(mx.z - mn.z, 0.001f),
                    0.0f, 1.0f);
                hitPart = localHeight > 0.78f ? "head" :
                    localHeight > 0.32f ? "torso" : "leg";
            }
        }
    }

    result.end = muzzlePos + shotDirection * nearest;
    result.hitNormal = victim || remoteVictim ? hitNormal : worldNormal;

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
        processNpcHit(result, def, *victim, hitPart, hitNormal, result.end, shotDirection, nearest, shooter, npcs, muzzlePos, shotDirection);
    } else if (remoteVictim) {
        processRemotePlayerHit(result, def, hitPart, hitNormal, result.end, shotDirection, nearest, shooter, remoteTargetId, remoteVictim);
    } else if (hitWorld) {
        processWorldHit(result, def, result.end, worldNormal, shotDirection, shooter.username);
    }

    return result;
}

RevolverShotResult tryFireHitscanDir(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& shooter,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& aimDir,
    const Player* targetPlayer)
{
    RevolverShotResult result;

    if (!def.soundShoot.empty()) {
        float rndPitch = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        float rndVolume = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        playWorldSound(def.soundShoot, muzzlePos, rndVolume, rndPitch, 80.0f);
    }

    result.fired = true;
    result.start = muzzlePos;

    glm::vec3 shotDirection = glm::normalize(aimDir);
    static unsigned int spreadRng = 1;
    shotDirection = computeSpreadDirection(shotDirection, def.spread, spreadRng);

    constexpr float MAX_SHOT_DISTANCE = 100.0f;
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

    bool hitPlayer = false;
    std::string hitPart;
    glm::vec3 hitNormal{0.0f};
    float localHeight = 0.5f;

    if (targetPlayer && !targetPlayer->dead && targetPlayer->currentHp > 0) {
        Capsule cap = targetPlayer->getCapsule();
        glm::vec3 mn(cap.a.x - cap.r, cap.a.y - cap.r, cap.a.z - cap.r);
        glm::vec3 mx(cap.b.x + cap.r, cap.b.y + cap.r, cap.b.z + cap.r);
        float distance = 0.0f;
        glm::vec3 normal;
        if (rayAabb(muzzlePos, shotDirection, mn, mx, distance, normal) && distance < nearest) {
            nearest = distance;
            hitWorld = false;
            hitPlayer = true;
            hitNormal = normal;
            glm::vec3 hit = muzzlePos + shotDirection * distance;
            localHeight = std::clamp(
                (hit.z - mn.z) / std::max(mx.z - mn.z, 0.001f), 0.0f, 1.0f);
            hitPart = localHeight > 0.78f ? "head" :
                      localHeight > 0.32f ? "torso" : "leg";
        }
    }

    result.end = muzzlePos + shotDirection * nearest;
    result.hitNormal = hitPlayer ? hitNormal : worldNormal;

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

    if (hitPlayer && targetPlayer) {
        processPlayerHit(result, def, hitPart, hitNormal, result.end, shotDirection, nearest, shooter, const_cast<Player*>(targetPlayer));
    } else if (hitWorld) {
        processWorldHit(result, def, result.end, worldNormal, shotDirection, shooter.username);
    }

    return result;
}

void fireMultiPellet(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    const Camera& camera,
    Player& shooter,
    NpcSystem& npcs,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir,
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    RevolverShotResult& outResult)
{
    if (!def.soundShoot.empty()) {
        float rndPitch = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        float rndVolume = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        printf("[SOUND] weapon=%s event=shoot path=%s pitch=%.3f volume=%.3f\n",
               def.id.c_str(), def.soundShoot.c_str(), rndPitch, rndVolume);
        playWorldSound(def.soundShoot, muzzlePos, rndVolume, rndPitch, 80.0f);
    }

    float spreadDeg = std::max(0.1f, def.spread);

    constexpr float AIM_DISTANCE = 100.0f;
    glm::vec3 aimPoint = camera.pos + camera.front * AIM_DISTANCE;
    glm::vec3 baseDir = glm::normalize(aimPoint - muzzlePos);
    if (glm::length(baseDir) < 0.001f)
        baseDir = camera.front;

    glm::vec3 up(0.0f, 0.0f, 1.0f);
    if (std::fabs(glm::dot(baseDir, up)) > 0.99f)
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(baseDir, up));
    glm::vec3 localUp = glm::normalize(glm::cross(right, baseDir));

    float halfAngleRad = glm::radians(spreadDeg * 0.5f);

    int pelletCount = std::max(1, def.pelletCount);
    int cols = std::max(1, (int)std::ceil(std::sqrt((float)pelletCount)));
    int rows = std::max(1, (int)std::ceil((float)pelletCount / (float)cols));
    int totalPellets = 0;
    float accumulatedDamage = 0.0f;
    constexpr float MAX_SHOT_DISTANCE = 100.0f;
    glm::vec3 lastPelletEnd = muzzlePos + baseDir * MAX_SHOT_DISTANCE;
    glm::vec3 lastHitNormal(0.0f);
    bool anyHitEntity = false;
    bool anyHitWorld = false;
    uint32_t lastTargetId = 0;
    glm::vec3 accumulatedKnockback(0.0f);
    float nearestPelletDist = MAX_SHOT_DISTANCE;

    if (gDebugWeapon) {
        printf("[SHOTGUN]\nweapon=%s\npellets=%d\nspread=%.1f\ndamage=%.0f\nheadshotMultiplier=%.0f\nrecoil=%.0f\nfireDelay=%.2f\nreloadTime=%.1f\nmagazine=%d\n",
               def.id.c_str(), pelletCount, spreadDeg, def.damage, def.headshotMultiplier, def.recoil, def.fireDelay, def.reloadTime, def.magazineSize);
    }

    int pelletIndex = 0;
    for (int row = 0; row < rows && pelletIndex < pelletCount; ++row) {
        for (int col = 0; col < cols && pelletIndex < pelletCount; ++col, ++pelletIndex) {
            float fracX = cols > 1 ? (col / ((float)cols - 1.0f)) * 2.0f - 1.0f : 0.0f;
            float fracY = rows > 1 ? (row / ((float)rows - 1.0f)) * 2.0f - 1.0f : 0.0f;
            float hAngle = halfAngleRad * fracX;
            float vAngle = halfAngleRad * fracY;
            glm::quat rot = glm::angleAxis(hAngle, localUp) * glm::angleAxis(vAngle, right);
            glm::vec3 pelletDir = glm::normalize(rot * baseDir);

            float pelletNearest = MAX_SHOT_DISTANCE;
            glm::vec3 worldNml = -pelletDir;
            bool hitW = false;
            for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
                float d = 0.0f;
                if (rayTriangle(muzzlePos, pelletDir, tri, d) && d < pelletNearest) {
                    pelletNearest = d;
                    hitW = true;
                    worldNml = tri.normal;
                }
            }

            Npc* pelletVictim = nullptr;
            std::string pelletPart;
            glm::vec3 pelletHitNml(0.0f);
            float pelletHeight = 0.5f;
            for (Npc& npc : npcs.all()) {
                if (npc.body.currentHp <= 0) continue;
                npc.body.updateModelWorldTransforms();
                for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
                    glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                    glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                    glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                    float d = 0.0f;
                    glm::vec3 nml;
                    if (rayAabb(muzzlePos, pelletDir, center - half, center + half, d, nml) && d < pelletNearest) {
                        pelletNearest = d;
                        hitW = false;
                        pelletVictim = &npc;
                        pelletPart = part.name;
                        pelletHitNml = nml;
                        glm::vec3 hit = muzzlePos + pelletDir * d;
                        pelletHeight = std::clamp((hit.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
                    }
                }
            }

            uint32_t pelletRemoteTargetId = 0;
            const Player* pelletRemoteVictim = nullptr;
    if (remotePlayers) {
        for (const auto& entry : *remotePlayers) {
            const Player& remote = entry.second;
            if (remote.dead || remote.currentHp <= 0) {
                if (MimitaNet::gNetHitDebug)
                    printf("[NET HIT SKIP] remote id=%u reason=%s hp=%d\n",
                           entry.first,
                           remote.dead ? "dead" : "hp-zero",
                           remote.currentHp);
                continue;
            }
                    const Capsule capsule = remote.getCapsule();
                    const glm::vec3 mn(remote.pos.x - capsule.r, remote.pos.y - capsule.r, capsule.a.z - capsule.r);
                    const glm::vec3 mx(remote.pos.x + capsule.r, remote.pos.y + capsule.r, capsule.b.z + capsule.r);
                    float d = 0.0f;
                    glm::vec3 nml;
                    if (rayAabb(muzzlePos, pelletDir, mn, mx, d, nml) && d < pelletNearest) {
                        pelletNearest = d;
                        hitW = false;
                        pelletVictim = nullptr;
                        pelletRemoteVictim = &remote;
                        pelletRemoteTargetId = entry.first;
                        pelletHitNml = nml;
                        glm::vec3 hit = muzzlePos + pelletDir * d;
                        pelletHeight = std::clamp((hit.z - mn.z) / std::max(mx.z - mn.z, 0.001f), 0.0f, 1.0f);
                        pelletPart = pelletHeight > 0.78f ? "head" :
                            pelletHeight > 0.32f ? "torso" : "leg";
                    }
                }
            }

            glm::vec3 pelletEnd = muzzlePos + pelletDir * pelletNearest;

            if (gDebugWeapon && pelletIndex < 3)
                printf("pellet%d direction=(%.4f %.4f %.4f)\n", pelletIndex, pelletDir.x, pelletDir.y, pelletDir.z);

            EffectPartSystem::instance().spawnTracer(muzzlePos, pelletEnd, shooter.username);

            if (pelletVictim) {
                processMultiPelletNpcHit(outResult, def, *pelletVictim, pelletPart, pelletHitNml, pelletEnd, pelletDir, pelletNearest, shooter, npcs, muzzlePos, accumulatedDamage, anyHitEntity, lastTargetId, accumulatedKnockback, nearestPelletDist, lastPelletEnd, lastHitNormal);
            } else if (pelletRemoteVictim) {
                processMultiPelletRemoteHit(outResult, def, pelletPart, pelletHitNml, pelletEnd, pelletDir, pelletNearest, shooter, pelletRemoteTargetId, accumulatedDamage, anyHitEntity, lastTargetId, accumulatedKnockback, nearestPelletDist, lastPelletEnd, lastHitNormal, pelletRemoteVictim->username);
            } else if (hitW) {
                processMultiPelletWorldHit(def, pelletEnd, worldNml, pelletDir, pelletNearest, shooter, anyHitWorld, nearestPelletDist, lastPelletEnd, lastHitNormal);
            } else {
                if (pelletNearest < nearestPelletDist) {
                    nearestPelletDist = pelletNearest;
                    lastPelletEnd = pelletEnd;
                }
            }

            if (!hitW && !pelletVictim && !pelletRemoteVictim && pelletNearest < nearestPelletDist) {
                lastPelletEnd = pelletEnd;
            }
        }
    }

    finalizeMultiPelletResult(outResult, muzzlePos, lastPelletEnd, lastHitNormal, accumulatedDamage, anyHitEntity, anyHitWorld, lastTargetId, accumulatedKnockback, totalPellets, def, shooter);
}

} // namespace WeaponFire

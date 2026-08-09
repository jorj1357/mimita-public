// 07 31 2026, 13 34
/* purpose
* Implements local hitscan firing prediction: aim computation, single-ray
* (revolver) and multi-pellet (shotgun) collision, and tracer/muzzle-flash spawning.
* Routes local hits into damage/effect handlers shared with the network event path.
* Does NOT own server weapon authority, packet send/receive, or damage validation.
* Does NOT simulate projectiles, drive audio backends, or render viewmodels.
*/
#include "weapon-fire.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/weapon-audio.h"
#include "combat/shot-profiler.h"
#include "config/networking-config.h"
#include "debug/debug-log.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "entities/player.h"
#include "network/multiplayer-context.h"
#include "world/world.h"
#include "npc/npc.h"
#include "replay/replay.h"
#include "perf/perf.h"

extern int gGlobalTick;

namespace WeaponFire {

namespace {
bool gDebugWeapon = false;

float hitscanMaxRange(const WeaponDefinition& def)
{
    auto it = def.customParams.find("range");
    if (it != def.customParams.end()) return it->second;
    it = def.customParams.find("maxRange");
    if (it != def.customParams.end()) return it->second;
    return 250.0f;
}

// Tracer endpoint: with beam_continue_after_hit the beam runs to the weapon's
// max range past the first hit; otherwise it stops at the hit point.
glm::vec3 hitscanTracerEnd(const WeaponDefinition& def, const glm::vec3& muzzle,
                           const glm::vec3& dir, const glm::vec3& hitPoint)
{
    if (!NetworkingConfig::instance().data().combat.beamContinueAfterHit)
        return hitPoint;
    glm::vec3 d = glm::length(dir) > 0.001f ? glm::normalize(dir) : glm::vec3(1.0f, 0.0f, 0.0f);
    return muzzle + d * hitscanMaxRange(def);
}

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
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    std::unordered_map<uint32_t, Player>* remoteNpcs)
{
    RevolverShotResult result;

    WeaponAudio::playShootSound(def, muzzlePos);
    result.fired = true;
    result.start = muzzlePos;

    AimSolution aim = computeAim(camera, world, npcs, muzzlePos, remotePlayers, remoteNpcs);
    logAimDebug("hitscan", camera, aim);
    glm::vec3 shotDirection = aim.direction;

    static unsigned int spreadRng = 1;
    shotDirection = computeSpreadDirection(shotDirection, def.spread, spreadRng);
    result.direction = shotDirection;
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Final Direction Sent Into Weapon: (%.4f, %.4f, %.4f)\n",
        shotDirection.x, shotDirection.y, shotDirection.z);

    constexpr float MAX_SHOT_DISTANCE = 100.0f;

    // Push ray origin forward to prevent starting inside geometry
    glm::vec3 rayOrigin = muzzlePos + shotDirection * 0.01f;

    Debug::log(Debug::Category::Weapons,
        "[BEAM] weapon=%s hitscan=true beamThickness=%.2f collisionType=%s\n",
        def.id.c_str(), def.beamThickness,
        (def.beamThickness > 0.0f) ? "SphereCast" : "Raycast");

    BeamCollisionResult beam = collideBeam(
        rayOrigin, shotDirection, MAX_SHOT_DISTANCE, def.beamThickness,
        world, &npcs, remotePlayers, nullptr,
        aim.usesCameraTarget, remoteNpcs, def.beamWorldThickness);

    float nearest = beam.nearest;
    bool hitWorld = beam.hitWorld;
    glm::vec3 worldNormal = beam.worldNormal;
    Npc* victim = beam.victim;
    std::string hitPart = beam.hitPart;
    glm::vec3 hitNormal = beam.hitNormal;
    float localHeight = beam.localHeight;
    uint32_t remoteTargetId = beam.remoteTargetId;
    Player* remoteVictim = beam.remoteVictim;
    uint32_t remoteNpcTargetId = beam.remoteNpcTargetId;

    // Use exact surface contact for visuals, entity hits, and world effects.
    // For a miss, hitPosition is the default endpoint at maxDistance.
    result.end = beam.hitPosition;
    result.hitNormal = victim || remoteVictim || remoteNpcTargetId ? hitNormal : worldNormal;

    // Crosshair aim skips world collision so the beam hits exactly the camera
    // aim point; stop the tracer at that surface instead of running through it.
    if (aim.usesCameraTarget && !victim && !remoteVictim && !remoteNpcTargetId && !hitWorld)
        result.end = aim.aimPoint;

    if (gDebugWeapon)
    {
        printf("[BEAM COLLISION] weapon=%s type=%s radius=%.2f "
               "origin=(%.2f,%.2f,%.2f) direction=(%.4f,%.4f,%.4f) "
               "distance=%.2f sweepCenter=(%.2f,%.2f,%.2f) "
               "surfacePoint=(%.2f,%.2f,%.2f) normal=(%.2f,%.2f,%.2f) "
               "hitKind=%s remoteNpcId=%u\n",
               def.id.c_str(),
               (def.beamThickness > 0.0f) ? "sphere" : "ray",
               def.beamThickness,
               rayOrigin.x, rayOrigin.y, rayOrigin.z,
               shotDirection.x, shotDirection.y, shotDirection.z,
               nearest,
               beam.sweepCenterPosition.x, beam.sweepCenterPosition.y, beam.sweepCenterPosition.z,
               beam.hitPosition.x, beam.hitPosition.y, beam.hitPosition.z,
               worldNormal.x, worldNormal.y, worldNormal.z,
               victim ? "npc" : (remoteVictim ? "remote" : (remoteNpcTargetId ? "remote_npc" : (hitWorld ? "world" : "none"))),
               remoteNpcTargetId);
        printf("[BEAM ENDPOINT] muzzle=(%.2f,%.2f,%.2f) rayOrigin=(%.2f,%.2f,%.2f) "
               "sweepCenter=(%.2f,%.2f,%.2f) surfacePoint=(%.2f,%.2f,%.2f) "
               "visualEnd=(%.2f,%.2f,%.2f) diffCenterToSurface=%.2f\n",
               muzzlePos.x, muzzlePos.y, muzzlePos.z,
               rayOrigin.x, rayOrigin.y, rayOrigin.z,
               beam.sweepCenterPosition.x, beam.sweepCenterPosition.y, beam.sweepCenterPosition.z,
               beam.hitPosition.x, beam.hitPosition.y, beam.hitPosition.z,
               result.end.x, result.end.y, result.end.z,
               glm::length(beam.sweepCenterPosition - beam.hitPosition));
    }

    Debug::warn(Debug::Category::Weapons,
        "[AIM] Final Direction Used By Hitscan: (%.4f, %.4f, %.4f)\n"
        "[AIM] Impact Position: (%.2f, %.2f, %.2f) hit=%s distance=%.2f\n"
        "[BEAM] beamThickness=%.2f collisionType=%s\n",
        shotDirection.x, shotDirection.y, shotDirection.z,
        result.end.x, result.end.y, result.end.z,
        victim || remoteVictim || remoteNpcTargetId ? "entity" : (hitWorld ? "world" : "none"),
        nearest,
        def.beamThickness,
        (def.beamThickness > 0.0f) ? "SphereCast" : "Raycast");

    ReplayEffectEvent gunshotEvent;
    gunshotEvent.type = "gunshot";
    gunshotEvent.position = muzzlePos;
    gunshotEvent.direction = shotDirection;
    gunshotEvent.from = muzzlePos;
    gunshotEvent.to = result.end;
    gunshotEvent.sourceActorId = shooter.username;
    captureReplayEffect(gunshotEvent);

    EffectPartSystem::instance().spawnMuzzleFlash(muzzlePos, shooter.username, shooter.sizeScale);
    EffectPartSystem::instance().spawnTracer(
        muzzlePos, hitscanTracerEnd(def, muzzlePos, shotDirection, result.end),
        shooter.username, shooter.sizeScale, def.id);
    if (victim) {
        processNpcHit(result, def, *victim, hitPart, hitNormal, result.end, shotDirection, nearest, shooter, npcs, muzzlePos, shotDirection);
    } else if (remoteVictim) {
        processRemotePlayerHit(result, def, hitPart, hitNormal, result.end, shotDirection, nearest, shooter, remoteTargetId, remoteVictim);
    } else if (remoteNpcTargetId) {
        processRemoteNpcHit(result, def, hitPart, result.end, nearest, shooter,
                            remoteNpcTargetId, beam.remoteNpcVictim);
    } else if (hitWorld) {
        processWorldHit(result, def, result.end, worldNormal, shotDirection, shooter.username);
    } else if (aim.usesCameraTarget) {
        result.end = aim.aimPoint;
        result.hitNormal = aim.cameraWorldNormal;
        processWorldHit(result, def, aim.aimPoint, aim.cameraWorldNormal, shotDirection, shooter.username);
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

    WeaponAudio::playShootSound(def, muzzlePos);
    result.fired = true;
    result.start = muzzlePos;

    glm::vec3 shotDirection = glm::normalize(aimDir);
    static unsigned int spreadRng = 1;
    shotDirection = computeSpreadDirection(shotDirection, def.spread, spreadRng);

    constexpr float MAX_SHOT_DISTANCE = 100.0f;

    // Push ray origin forward to prevent starting inside geometry
    glm::vec3 rayOrigin = muzzlePos + shotDirection * 0.01f;

    Debug::log(Debug::Category::Weapons,
        "[BEAM] weapon=%s hitscan=true beamThickness=%.2f collisionType=%s\n",
        def.id.c_str(), def.beamThickness,
        (def.beamThickness > 0.0f) ? "SphereCast" : "Raycast");

    BeamCollisionResult beam = collideBeam(
        rayOrigin, shotDirection, MAX_SHOT_DISTANCE, def.beamThickness,
        world, nullptr, nullptr, targetPlayer, false, nullptr,
        def.beamWorldThickness);

    float nearest = beam.nearest;
    bool hitWorld = beam.hitWorld;
    glm::vec3 worldNormal = beam.worldNormal;
    bool hitPlayer = (beam.victim || beam.remoteVictim || (beam.hitPart.length() > 0));
    std::string hitPart = beam.hitPart;
    glm::vec3 hitNormal = beam.hitNormal;
    float localHeight = beam.localHeight;

    result.end = beam.hitPosition;
    result.hitNormal = hitPlayer ? hitNormal : worldNormal;

    ReplayEffectEvent gunshotEvent;
    gunshotEvent.type = "gunshot";
    gunshotEvent.position = muzzlePos;
    gunshotEvent.direction = shotDirection;
    gunshotEvent.from = muzzlePos;
    gunshotEvent.to = result.end;
    gunshotEvent.sourceActorId = shooter.username;
    captureReplayEffect(gunshotEvent);

    EffectPartSystem::instance().spawnMuzzleFlash(muzzlePos, shooter.username, shooter.sizeScale);
    EffectPartSystem::instance().spawnTracer(
        muzzlePos, hitscanTracerEnd(def, muzzlePos, shotDirection, result.end),
        shooter.username, shooter.sizeScale, def.id);

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
    RevolverShotResult& outResult,
    std::unordered_map<uint32_t, Player>* remoteNpcs)
{
    Perf::ScopedTimer _shotgun("Shotgun");

    ShotProfiler shotProf;
    shotProf.reset(def.id, gGlobalTick);
    gShotProfiler = &shotProf;

    shotProf.totalPellets = std::max(1, def.pelletCount);

    {
        auto ts = ShotProfiler::Scope(&shotProf.audioMs);
        WeaponAudio::playShootSound(def, muzzlePos);
    }
    EffectPartSystem::instance().spawnMuzzleFlash(muzzlePos, shooter.username, shooter.sizeScale);

    float spreadDeg = std::max(0.1f, def.spread);

    glm::vec3 baseDir;
    bool aimUsesCameraTarget = false;
    glm::vec3 cameraAimPoint(0.0f);
    glm::vec3 cameraAimNormal(0.0f, 0.0f, 1.0f);
    {
        auto ts = ShotProfiler::Scope(&shotProf.aimMs);
        AimSolution aim = computeAim(camera, world, npcs, muzzlePos, remotePlayers, remoteNpcs);
        logAimDebug("multi_pellet", camera, aim);
        baseDir = aim.direction;
        aimUsesCameraTarget = aim.usesCameraTarget;
        cameraAimPoint = aim.aimPoint;
        cameraAimNormal = aim.cameraWorldNormal;
    }

    Debug::warn(Debug::Category::Weapons,
        "[AIM] Final Direction Sent Into Weapon: (%.4f, %.4f, %.4f)\n",
        baseDir.x, baseDir.y, baseDir.z);

    glm::vec3 up(0.0f, 0.0f, 1.0f);
    if (std::fabs(glm::dot(baseDir, up)) > 0.99f)
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(baseDir, up));
    glm::vec3 localUp = glm::normalize(glm::cross(right, baseDir));

    glm::vec3 pelletDirs[16];
    {
        auto ts = ShotProfiler::Scope(&shotProf.pelletGenMs);
        float halfAngleRad = glm::radians(spreadDeg * 0.5f);
        int pelletCount = shotProf.totalPellets;
        int cols = std::max(1, (int)std::ceil(std::sqrt((float)pelletCount)));
        int rows = std::max(1, (int)std::ceil((float)pelletCount / (float)cols));
        int idx = 0;
        for (int r = 0; r < rows && idx < pelletCount; ++r)
            for (int c = 0; c < cols && idx < pelletCount; ++c, ++idx) {
                float fx = cols > 1 ? (c / ((float)cols - 1.0f)) * 2.0f - 1.0f : 0.0f;
                float fy = rows > 1 ? (r / ((float)rows - 1.0f)) * 2.0f - 1.0f : 0.0f;
                float ha = halfAngleRad * fx;
                float va = halfAngleRad * fy;
                glm::quat rot = glm::angleAxis(ha, localUp) * glm::angleAxis(va, right);
                pelletDirs[idx] = glm::normalize(rot * baseDir);
            }
    }

    int cols = std::max(1, (int)std::ceil(std::sqrt((float)shotProf.totalPellets)));
    int rows = std::max(1, (int)std::ceil((float)shotProf.totalPellets / (float)cols));
    int totalPellets = 0;
    float accumulatedDamage = 0.0f;
    constexpr float MAX_SHOT_DISTANCE = 100.0f;
    glm::vec3 lastPelletEnd = muzzlePos + baseDir * MAX_SHOT_DISTANCE;
    glm::vec3 lastHitNormal(0.0f);
    bool anyHitEntity = false;
    bool anyHitWorld = false;
    int remoteNpcPelletHits = 0;
    int remotePlayersHit = 0;
    float totalRemoteDamage = 0.0f;
    int worldPellets = 0;
    int missedPellets = 0;
    uint32_t lastTargetId = 0;
    glm::vec3 accumulatedKnockback(0.0f);
    float nearestPelletDist = MAX_SHOT_DISTANCE;

    Debug::log(Debug::Category::Weapons,
        "[BEAM] weapon=%s hitscan=true beamThickness=%.2f collisionType=%s\n",
        def.id.c_str(), def.beamThickness,
        (def.beamThickness > 0.0f) ? "SphereCast" : "Raycast");

    {
        auto ts = ShotProfiler::Scope(&shotProf.pelletLoopMs);
        for (int p = 0; p < shotProf.totalPellets; ++p) {
            ++totalPellets;
            const glm::vec3& pelletDir = pelletDirs[p];

            {
                auto tc = ShotProfiler::Scope(&shotProf.worldCollisionMs);
                shotProf.collisionCalls++;
                glm::vec3 pelletOrigin = muzzlePos + pelletDir * 0.01f;
                BeamCollisionResult pelletBeam = collideBeam(
                    pelletOrigin, pelletDir, MAX_SHOT_DISTANCE, def.beamThickness,
                    world, &npcs, remotePlayers, nullptr,
                    false, remoteNpcs, def.beamWorldThickness);
                (void)tc;

                float pelletNearest = pelletBeam.nearest;
                bool hitW = pelletBeam.hitWorld;
                glm::vec3 worldNml = pelletBeam.worldNormal;
                Npc* pelletVictim = pelletBeam.victim;
                std::string pelletPart = pelletBeam.hitPart;
                glm::vec3 pelletHitNml = pelletBeam.hitNormal;
                uint32_t pelletRemoteTargetId = pelletBeam.remoteTargetId;
                const Player* pelletRemoteVictim = pelletBeam.remoteVictim;
                uint32_t pelletRemoteNpcTargetId = pelletBeam.remoteNpcTargetId;

                // Use exact surface contact for pellet visual endpoint
                glm::vec3 pelletEnd = pelletBeam.hitPosition;

                if (gDebugWeapon && p < 3)
                    printf("pellet%d direction=(%.4f %.4f %.4f)\n", p, pelletDir.x, pelletDir.y, pelletDir.z);

                {
                    auto tt = ShotProfiler::Scope(&shotProf.tracerMs);
                    EffectPartSystem::instance().spawnTracer(
                        muzzlePos, hitscanTracerEnd(def, muzzlePos, pelletDir, pelletEnd),
                        shooter.username, shooter.sizeScale, def.id);
                    shotProf.tracersSpawned++;
                }

                if (pelletVictim) {
                    shotProf.npcHits++;
                    auto td = ShotProfiler::Scope(&shotProf.damageMs);
                    processMultiPelletNpcHit(outResult, def, *pelletVictim, pelletPart, pelletHitNml, pelletEnd, pelletDir, pelletNearest, shooter, npcs, muzzlePos, accumulatedDamage, anyHitEntity, lastTargetId, accumulatedKnockback, nearestPelletDist, lastPelletEnd, lastHitNormal);
                } else if (pelletRemoteVictim) {
                    shotProf.npcHits++;
                    auto td = ShotProfiler::Scope(&shotProf.damageMs);
                    const float damageBefore = accumulatedDamage;
                    processMultiPelletRemoteHit(outResult, def, pelletPart, pelletHitNml, pelletEnd, pelletDir, pelletNearest, shooter, pelletRemoteTargetId, accumulatedDamage, anyHitEntity, lastTargetId, accumulatedKnockback, nearestPelletDist, lastPelletEnd, lastHitNormal, pelletRemoteVictim->username);
                    totalRemoteDamage += accumulatedDamage - damageBefore;
                    ++remotePlayersHit;
                } else if (pelletRemoteNpcTargetId) {
                    // Remote NPC pellet hit: instant predicted feedback (hitmarker
                    // + damage number + HP overlay) like remote players; the
                    // server confirm reconciles it.
                    shotProf.npcHits++;
                    auto td = ShotProfiler::Scope(&shotProf.damageMs);
                    processMultiPelletRemoteNpcHit(
                        outResult, def, pelletPart, pelletHitNml, pelletEnd, pelletDir,
                        pelletNearest, shooter, pelletRemoteNpcTargetId,
                        accumulatedDamage, anyHitEntity, lastTargetId,
                        accumulatedKnockback, nearestPelletDist, lastPelletEnd,
                        lastHitNormal);
                    ++remoteNpcPelletHits;
                } else if (hitW) {
                    shotProf.worldHits++;
                    ++worldPellets;
                    auto td = ShotProfiler::Scope(&shotProf.damageMs);
                    processMultiPelletWorldHit(def, pelletEnd, worldNml, pelletDir, pelletNearest, shooter, anyHitWorld, nearestPelletDist, lastPelletEnd, lastHitNormal);
                } else {
                    shotProf.misses++;
                    ++missedPellets;
                    if (aimUsesCameraTarget) {
                        // Camera-target aim skips world collision (collideBeam
                        // skipWorldCollision), so render the world impact at the
                        // crosshair aim point instead (mirrors tryFireHitscan).
                        processMultiPelletWorldHit(def, cameraAimPoint, cameraAimNormal,
                            pelletDir, pelletNearest, shooter, anyHitWorld,
                            nearestPelletDist, lastPelletEnd, lastHitNormal);
                    } else if (pelletNearest < nearestPelletDist) {
                        nearestPelletDist = pelletNearest;
                        lastPelletEnd = pelletEnd;
                    }
                }

                if (!hitW && !pelletVictim && !pelletRemoteVictim &&
                    !pelletRemoteNpcTargetId && pelletNearest < nearestPelletDist) {
                    lastPelletEnd = pelletEnd;
                }
            }
        }
    }

    EffectPartSystem::instance().drainPendingWorldHits(6);

    finalizeMultiPelletResult(outResult, muzzlePos, lastPelletEnd, lastHitNormal, accumulatedDamage, anyHitEntity, anyHitWorld, lastTargetId, accumulatedKnockback, totalPellets, def, shooter);
    outResult.direction = baseDir;
    printf("[SHOTGUN LOCAL RESULT] shotSerial=pending pelletCount=%d remotePlayersHit=%d "
           "remoteNpcPelletHits=%d "
           "totalRemoteDamage=%.0f primaryTargetId=%u returnedTargetId=%u "
           "returnedDamage=%.0f worldPellets=%d missedPellets=%d\n",
           totalPellets, remotePlayersHit, remoteNpcPelletHits,
           totalRemoteDamage,
           outResult.targetIsRemotePlayer ? outResult.targetId : 0,
           outResult.targetId, outResult.damage,
           worldPellets, missedPellets);
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Impact Position: (%.2f, %.2f, %.2f) hit=%s\n",
        outResult.end.x, outResult.end.y, outResult.end.z,
        outResult.hitEntity ? "entity" : (outResult.hitWorld ? "world" : "none"));

    gShotProfiler = nullptr;

    if (DebugConfig::WEAPON_PERF_SHOTS) {
        shotProf.print();
    }
}

} // namespace WeaponFire

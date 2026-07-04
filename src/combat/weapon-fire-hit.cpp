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

    AimSolution aim = computeAim(camera, world, npcs, muzzlePos, remotePlayers);
    logAimDebug("hitscan", camera, aim);
    glm::vec3 shotDirection = aim.direction;

    static unsigned int spreadRng = 1;
    shotDirection = computeSpreadDirection(shotDirection, def.spread, spreadRng);
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Final Direction Sent Into Weapon: (%.4f, %.4f, %.4f)\n",
        shotDirection.x, shotDirection.y, shotDirection.z);

    constexpr float MAX_SHOT_DISTANCE = 100.0f;

    Debug::log(Debug::Category::Weapons,
        "[BEAM] weapon=%s hitscan=true beamThickness=%.2f collisionType=%s\n",
        def.id.c_str(), def.beamThickness,
        (def.beamThickness > 0.0f) ? "SphereCast" : "Raycast");

    BeamCollisionResult beam = collideBeam(
        muzzlePos, shotDirection, MAX_SHOT_DISTANCE, def.beamThickness,
        world, &npcs, remotePlayers, nullptr);

    float nearest = beam.nearest;
    bool hitWorld = beam.hitWorld;
    glm::vec3 worldNormal = beam.worldNormal;
    Npc* victim = beam.victim;
    std::string hitPart = beam.hitPart;
    glm::vec3 hitNormal = beam.hitNormal;
    float localHeight = beam.localHeight;
    uint32_t remoteTargetId = beam.remoteTargetId;
    const Player* remoteVictim = beam.remoteVictim;

    result.end = muzzlePos + shotDirection * nearest;
    result.hitNormal = victim || remoteVictim ? hitNormal : worldNormal;
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Final Direction Used By Hitscan: (%.4f, %.4f, %.4f)\n"
        "[AIM] Impact Position: (%.2f, %.2f, %.2f) hit=%s distance=%.2f\n"
        "[BEAM] beamThickness=%.2f collisionType=%s\n",
        shotDirection.x, shotDirection.y, shotDirection.z,
        result.end.x, result.end.y, result.end.z,
        victim || remoteVictim ? "entity" : (hitWorld ? "world" : "none"),
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

    Debug::log(Debug::Category::Weapons,
        "[BEAM] weapon=%s hitscan=true beamThickness=%.2f collisionType=%s\n",
        def.id.c_str(), def.beamThickness,
        (def.beamThickness > 0.0f) ? "SphereCast" : "Raycast");

    BeamCollisionResult beam = collideBeam(
        muzzlePos, shotDirection, MAX_SHOT_DISTANCE, def.beamThickness,
        world, nullptr, nullptr, targetPlayer);

    float nearest = beam.nearest;
    bool hitWorld = beam.hitWorld;
    glm::vec3 worldNormal = beam.worldNormal;
    bool hitPlayer = (beam.victim || beam.remoteVictim || (beam.hitPart.length() > 0));
    std::string hitPart = beam.hitPart;
    glm::vec3 hitNormal = beam.hitNormal;
    float localHeight = beam.localHeight;

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
    Perf::ScopedTimer _shotgun("Shotgun");

    using clock = std::chrono::steady_clock;
    auto tStart = clock::now();
    double msCollision = 0.0, msHitFX = 0.0, msDebris = 0.0, msTracers = 0.0;
    int pelletCount = std::max(1, def.pelletCount);
    int tracerCount = 0, hitCount = 0, worldHitCount = 0;

    if (!def.soundShoot.empty()) {
        float rndPitch = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        float rndVolume = 1.0f + ((rand() % 201 - 100) / 10000.0f);
        printf("[SOUND] weapon=%s event=shoot path=%s pitch=%.3f volume=%.3f\n",
               def.id.c_str(), def.soundShoot.c_str(), rndPitch, rndVolume);
        playWorldSound(def.soundShoot, muzzlePos, rndVolume, rndPitch, 80.0f);
    }

    float spreadDeg = std::max(0.1f, def.spread);

    AimSolution aim = computeAim(camera, world, npcs, muzzlePos, remotePlayers);
    logAimDebug("multi_pellet", camera, aim);
    glm::vec3 baseDir = aim.direction;
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Final Direction Sent Into Weapon: (%.4f, %.4f, %.4f)\n",
        baseDir.x, baseDir.y, baseDir.z);

    glm::vec3 up(0.0f, 0.0f, 1.0f);
    if (std::fabs(glm::dot(baseDir, up)) > 0.99f)
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(baseDir, up));
    glm::vec3 localUp = glm::normalize(glm::cross(right, baseDir));

    float halfAngleRad = glm::radians(spreadDeg * 0.5f);

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

    Debug::log(Debug::Category::Weapons,
        "[BEAM] weapon=%s hitscan=true beamThickness=%.2f collisionType=%s\n",
        def.id.c_str(), def.beamThickness,
        (def.beamThickness > 0.0f) ? "SphereCast" : "Raycast");

    int pelletIndex = 0;
    for (int row = 0; row < rows && pelletIndex < pelletCount; ++row) {
        for (int col = 0; col < cols && pelletIndex < pelletCount; ++col, ++pelletIndex) {
            float fracX = cols > 1 ? (col / ((float)cols - 1.0f)) * 2.0f - 1.0f : 0.0f;
            float fracY = rows > 1 ? (row / ((float)rows - 1.0f)) * 2.0f - 1.0f : 0.0f;
            float hAngle = halfAngleRad * fracX;
            float vAngle = halfAngleRad * fracY;
            glm::quat rot = glm::angleAxis(hAngle, localUp) * glm::angleAxis(vAngle, right);
            glm::vec3 pelletDir = glm::normalize(rot * baseDir);

            {
                Perf::ScopedTimer _pelletCollision("ShotgunCollision");
                auto tc = clock::now();
                BeamCollisionResult pelletBeam = collideBeam(
                    muzzlePos, pelletDir, MAX_SHOT_DISTANCE, def.beamThickness,
                    world, &npcs, remotePlayers, nullptr);
                msCollision += std::chrono::duration<double, std::milli>(clock::now() - tc).count();

                float pelletNearest = pelletBeam.nearest;
                bool hitW = pelletBeam.hitWorld;
                glm::vec3 worldNml = pelletBeam.worldNormal;
                Npc* pelletVictim = pelletBeam.victim;
                std::string pelletPart = pelletBeam.hitPart;
                glm::vec3 pelletHitNml = pelletBeam.hitNormal;
                float pelletHeight = pelletBeam.localHeight;
                uint32_t pelletRemoteTargetId = pelletBeam.remoteTargetId;
                const Player* pelletRemoteVictim = pelletBeam.remoteVictim;

                glm::vec3 pelletEnd = muzzlePos + pelletDir * pelletNearest;

                if (gDebugWeapon && pelletIndex < 3)
                    printf("pellet%d direction=(%.4f %.4f %.4f)\n", pelletIndex, pelletDir.x, pelletDir.y, pelletDir.z);
                if (pelletIndex == 0) {
                    Debug::warn(Debug::Category::Weapons,
                        "[AIM] Final Direction Used By Hitscan: (%.4f, %.4f, %.4f)\n",
                        pelletDir.x, pelletDir.y, pelletDir.z);
                }

                {
                    auto tt = clock::now();
                    EffectPartSystem::instance().spawnTracer(muzzlePos, pelletEnd, shooter.username);
                    msTracers += std::chrono::duration<double, std::milli>(clock::now() - tt).count();
                    tracerCount++;
                }

                if (pelletVictim) {
                    hitCount++;
                    auto th = clock::now();
                    processMultiPelletNpcHit(outResult, def, *pelletVictim, pelletPart, pelletHitNml, pelletEnd, pelletDir, pelletNearest, shooter, npcs, muzzlePos, accumulatedDamage, anyHitEntity, lastTargetId, accumulatedKnockback, nearestPelletDist, lastPelletEnd, lastHitNormal);
                    msHitFX += std::chrono::duration<double, std::milli>(clock::now() - th).count();
                } else if (pelletRemoteVictim) {
                    hitCount++;
                    auto th = clock::now();
                    processMultiPelletRemoteHit(outResult, def, pelletPart, pelletHitNml, pelletEnd, pelletDir, pelletNearest, shooter, pelletRemoteTargetId, accumulatedDamage, anyHitEntity, lastTargetId, accumulatedKnockback, nearestPelletDist, lastPelletEnd, lastHitNormal, pelletRemoteVictim->username);
                    msHitFX += std::chrono::duration<double, std::milli>(clock::now() - th).count();
                } else if (hitW) {
                    worldHitCount++;
                    auto td = clock::now();
                    processMultiPelletWorldHit(def, pelletEnd, worldNml, pelletDir, pelletNearest, shooter, anyHitWorld, nearestPelletDist, lastPelletEnd, lastHitNormal);
                    msDebris += std::chrono::duration<double, std::milli>(clock::now() - td).count();
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
    }

    finalizeMultiPelletResult(outResult, muzzlePos, lastPelletEnd, lastHitNormal, accumulatedDamage, anyHitEntity, anyHitWorld, lastTargetId, accumulatedKnockback, totalPellets, def, shooter);
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Impact Position: (%.2f, %.2f, %.2f) hit=%s\n",
        outResult.end.x, outResult.end.y, outResult.end.z,
        outResult.hitEntity ? "entity" : (outResult.hitWorld ? "world" : "none"));

    if (DebugConfig::WEAPON_PERF_SHOTS) {
        double totalMs = std::chrono::duration<double, std::milli>(clock::now() - tStart).count();
        int activeBefore = (int)EffectPartSystem::instance().activeCount();
        printf("\n========== WEAPON SHOT PERF ==========\n");
        printf("Weapon: %s\n", def.id.c_str());
        printf("Frame: %d\n", gGlobalTick);
        printf("FrameMs: %.1f\n", Perf::state().avgFrameTimeMs);
        printf("WeaponMs: %.2f\n", totalMs);
        printf("\n");
        printf("Pellets: %d\n", pelletCount);
        printf("WorldHits: %d\n", worldHitCount);
        printf("NpcHits: %d\n", hitCount);
        printf("Misses: %d\n", pelletCount - hitCount - worldHitCount);
        printf("\n");
        printf("CollisionMs: %.3f\n", msCollision);
        printf("HitFXMs: %.3f\n", msHitFX);
        printf("HitFXCalls: %d\n", hitCount);
        printf("TracerCount: %d\n", tracerCount);
        printf("AudioMs: 0.1\n");
        printf("\n");
        printf("ActiveEffectsBefore: %d\n", activeBefore);
        printf("ActiveEffectsAfter: %d\n", (int)EffectPartSystem::instance().activeCount());
        printf("\n");
        std::string stage = "Collision";
        double maxMs = msCollision;
        if (msHitFX > maxMs) { maxMs = msHitFX; stage = "HitFX"; }
        if (msTracers > maxMs) { maxMs = msTracers; stage = "Tracers"; }
        printf("LargestStage: %s\n", stage.c_str());
        printf("LikelyCause: EffectPart burst / replay burst / allocation burst\n");
        printf("======================================\n");
    }
}

} // namespace WeaponFire

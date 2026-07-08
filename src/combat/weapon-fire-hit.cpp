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

    WeaponAudio::playShootSound(def, muzzlePos);
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

    WeaponAudio::playShootSound(def, muzzlePos);
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

    ShotProfiler shotProf;
    shotProf.reset(def.id, gGlobalTick);
    gShotProfiler = &shotProf;

    shotProf.totalPellets = std::max(1, def.pelletCount);

    {
        auto ts = ShotProfiler::Scope(&shotProf.audioMs);
        WeaponAudio::playShootSound(def, muzzlePos);
    }

    float spreadDeg = std::max(0.1f, def.spread);

    glm::vec3 baseDir;
    {
        auto ts = ShotProfiler::Scope(&shotProf.aimMs);
        AimSolution aim = computeAim(camera, world, npcs, muzzlePos, remotePlayers);
        logAimDebug("multi_pellet", camera, aim);
        baseDir = aim.direction;
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
            const glm::vec3& pelletDir = pelletDirs[p];

            {
                auto tc = ShotProfiler::Scope(&shotProf.worldCollisionMs);
                shotProf.collisionCalls++;
                BeamCollisionResult pelletBeam = collideBeam(
                    muzzlePos, pelletDir, MAX_SHOT_DISTANCE, def.beamThickness,
                    world, &npcs, remotePlayers, nullptr);
                (void)tc;

                float pelletNearest = pelletBeam.nearest;
                bool hitW = pelletBeam.hitWorld;
                glm::vec3 worldNml = pelletBeam.worldNormal;
                Npc* pelletVictim = pelletBeam.victim;
                std::string pelletPart = pelletBeam.hitPart;
                glm::vec3 pelletHitNml = pelletBeam.hitNormal;
                uint32_t pelletRemoteTargetId = pelletBeam.remoteTargetId;
                const Player* pelletRemoteVictim = pelletBeam.remoteVictim;

                glm::vec3 pelletEnd = muzzlePos + pelletDir * pelletNearest;

                if (gDebugWeapon && p < 3)
                    printf("pellet%d direction=(%.4f %.4f %.4f)\n", p, pelletDir.x, pelletDir.y, pelletDir.z);

                {
                    auto tt = ShotProfiler::Scope(&shotProf.tracerMs);
                    EffectPartSystem::instance().spawnTracer(muzzlePos, pelletEnd, shooter.username);
                    shotProf.tracersSpawned++;
                }

                if (pelletVictim) {
                    shotProf.npcHits++;
                    auto td = ShotProfiler::Scope(&shotProf.damageMs);
                    processMultiPelletNpcHit(outResult, def, *pelletVictim, pelletPart, pelletHitNml, pelletEnd, pelletDir, pelletNearest, shooter, npcs, muzzlePos, accumulatedDamage, anyHitEntity, lastTargetId, accumulatedKnockback, nearestPelletDist, lastPelletEnd, lastHitNormal);
                } else if (pelletRemoteVictim) {
                    shotProf.npcHits++;
                    auto td = ShotProfiler::Scope(&shotProf.damageMs);
                    processMultiPelletRemoteHit(outResult, def, pelletPart, pelletHitNml, pelletEnd, pelletDir, pelletNearest, shooter, pelletRemoteTargetId, accumulatedDamage, anyHitEntity, lastTargetId, accumulatedKnockback, nearestPelletDist, lastPelletEnd, lastHitNormal, pelletRemoteVictim->username);
                } else if (hitW) {
                    shotProf.worldHits++;
                    auto td = ShotProfiler::Scope(&shotProf.damageMs);
                    processMultiPelletWorldHit(def, pelletEnd, worldNml, pelletDir, pelletNearest, shooter, anyHitWorld, nearestPelletDist, lastPelletEnd, lastHitNormal);
                } else {
                    shotProf.misses++;
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

    EffectPartSystem::instance().drainPendingWorldHits(6);

    finalizeMultiPelletResult(outResult, muzzlePos, lastPelletEnd, lastHitNormal, accumulatedDamage, anyHitEntity, anyHitWorld, lastTargetId, accumulatedKnockback, totalPellets, def, shooter);
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

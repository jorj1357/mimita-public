#include "npc-combat.h"
#include "npc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/gtc/constants.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/weapon-fire.h"
#include "combat/pellet-pattern.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "combat/weapon-audio.h"
#include "combat/weapon-rocket-launcher.h"
#include "combat/weapon-grenade-launcher.h"
#include "config.h"
#include "debug/debug-log.h"
#include "effects/effect-part.h"
#include "physics/movement/physics-collision.h"
#include "physics/physics-types.h"
#include "world/world.h"
#include "npc/npc-internal.h"
#include "npc/npc-difficulty-config.h"
#include "npc/npc-combat-log.h"

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
    static thread_local std::vector<int> candidates;
    candidates.clear();
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
    float angleDiff = glm::degrees(std::acos(std::clamp(glm::dot(idealDir, finalDir), -1.0f, 1.0f)));
    // Gun-vs-bullet mismatch: degrees between the model's planar facing and the
    // shot's planar direction. Near 0 = the gun points where the bullet goes.
    glm::vec3 planarFacing = glm::normalize(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f));
    glm::vec3 planarShot = glm::normalize(glm::vec3(finalDir.x, finalDir.y, 0.0f));
    float facingAimDeg = 0.0f;
    if (glm::length(planarFacing) > 0.001f && glm::length(planarShot) > 0.001f)
        facingAimDeg = glm::degrees(std::acos(std::clamp(glm::dot(planarFacing, planarShot), -1.0f, 1.0f)));
    npcLog("npc-shot npc=%u weapon=%s maxError=%.1fdeg err=%.1fdeg facingAim=%.1fdeg "
           "target=(%.3f %.3f %.3f) aim=(%.3f %.3f %.3f) angleDiff=%.1fdeg",
           npc.id, def.id.c_str(), maxErrorDeg, actualErrorDeg, facingAimDeg,
           idealDir.x, idealDir.y, idealDir.z,
           finalDir.x, finalDir.y, finalDir.z, angleDiff);
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

glm::vec3 NpcCombat::applyAimError(const Npc& npc, glm::vec3 aimDir)
{
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
    // Never fire at a dead/unconscious target. Stops the post-death shot/sound
    // spam where NPCs kept shooting the corpse every frame.
    if (player.dead || player.currentHp <= 0)
        return false;

    const int hpBeforeShot = player.currentHp;

    if (npc.attackCooldown > 0.0f)
    {
        Debug::logThrottled(Debug::Category::NpcCombat, "npc-cd",
            DebugConfig::PRINT_INTERVAL, "[NPC] id=%u fire blocked: attackCooldown=%.2f\n",
            npc.id, npc.attackCooldown);
        return false;
    }

    float dist = npc.sensors.targetDistance;

    const auto& npcDiffSettings = NpcDifficultyConfig::instance().settings();
    const float dmgMul = npcDiffSettings.damageMultiplier;
    // NPC bullets are thin (npcHitRadius) so the aim error cone matters and
    // shots can genuinely miss; the player's own weapon keeps its fat beam.
    const float beamOverride = npcDiffSettings.npcHitRadius;

    const WeaponDefinition* def = WeaponRegistry::instance().get(npc.body.equippedWeaponId);
    if (!def)
    {
        Debug::log(Debug::Category::NpcCombat,
            "[NPC] id=%u fire blocked: no weapon equipped\n", npc.id);
        return false;
    }

    Debug::log(Debug::Category::NpcCombat,
        "[NPC FIRE] id=%u equippedWeaponId=%s defId=%s behaviorType=%d pellets=%d damage=%.0f\n",
        npc.id, npc.body.equippedWeaponId.c_str(), def->id.c_str(),
        (int)def->behaviorType, def->pelletCount, def->damage);

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
    // Gun-tip muzzle: project the shot origin forward along the model's facing
    // so tracers come from the revolver barrel, not the body center.
    glm::vec3 facingPlanar = glm::normalize(glm::vec3(npc.currentFacing.x, npc.currentFacing.y, 0.0f));
    if (glm::length(facingPlanar) < 0.001f)
        facingPlanar = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 npcPos = npc.body.pos + npcMuzzleOffset() + facingPlanar * 0.7f;

    // Use cached LOS from updateOneNpc (avoids redundant gather + triangle loop)
    if (npc.cachedLoSBlocked)
    {
        Debug::logThrottled(Debug::Category::NpcCombat, "npc-los",
            DebugConfig::PRINT_INTERVAL, "[NPC] id=%u fire blocked: no line of sight (cached)\n",
            npc.id);
        return false;
    }

    // The bullet fires where the gun points (the model's smooth facing), tilted
    // up/down to the target's chest, plus a small arcade error cone. The NPC
    // must actually aim its gun at the target to hit; while the gun is still
    // turning, shots go where it points (wide). No snap: the model is not
    // teleported to the shot direction.
    glm::vec3 toChest = (npc.sensors.targetPos + glm::vec3(0.0f, 0.0f, 0.8f)) - npcPos;
    float horizDist = glm::length(glm::vec2(toChest.x, toChest.y));
    float pitch = horizDist > 0.001f ? std::atan2(toChest.z, horizDist) : 0.0f;
    aimDir = glm::normalize(glm::vec3(
        facingPlanar.x * std::cos(pitch),
        facingPlanar.y * std::cos(pitch),
        std::sin(pitch)));
    aimDir = NpcCombat::applyAimError(npc, aimDir);

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
    bool shotHitWorld = false;
    // Actual endpoint (the damage trace). The server broadcast reads this so
    // the remote tracer goes exactly where the damage ray went (look == shoot == hit).
    glm::vec3 shotEnd = npcPos + aimDir * 100.0f;
    glm::vec3 shotNormal = glm::vec3(0.0f, 0.0f, 1.0f);
    switch (def->behaviorType) {
    case WeaponBehaviorType::Hitscan:
    {
        if (def->pelletCount > 1) {
            // Multi-pellet weapon (shotgun): fire all pellets in a spread pattern.
            // Play sound ONCE before the loop (not per-pellet).
            PelletPatternConfig ppc;
            ppc.pelletCount = def->pelletCount;
            ppc.spreadDegrees = def->spread;
            ppc.spreadSeed = const_cast<Npc&>(npc).rngState;
            glm::vec3 pelletDirs[MAX_PELLETS_PER_BLAST];
            int pelletCount = generatePelletDirections(aimDir, ppc, pelletDirs, MAX_PELLETS_PER_BLAST);

            WeaponAudio::playShootSound(*def, npcPos);

            float totalDamage = 0.0f;
            bool anyHit = false;
            bool anyHitWorld = false;
            glm::vec3 lastEnd = npcPos + aimDir * 100.0f;
            npc.pelletResultCount = 0;
            npc.pelletSpreadSeed = ppc.spreadSeed;
            for (int p = 0; p < pelletCount && p < Npc::MAX_NPC_PELLETS; ++p) {
                RevolverShotResult shot = WeaponFire::tryFireHitscanDir(
                    *def, rt, npc.body, world, npcPos, pelletDirs[p], &player, dmgMul, beamOverride, false, true);
                totalDamage += shot.damage;
                npc.pelletResults[p].hitPos = shot.end;
                npc.pelletResults[p].hitNormal = shot.hitNormal;
                npc.pelletResults[p].hitEntity = shot.hitEntity;
                npc.pelletResults[p].hitWorld = shot.hitWorld;
                npc.pelletResultCount = p + 1;
                if (shot.hitEntity) {
                    anyHit = true;
                    lastEnd = shot.end;
                    shotNormal = shot.hitNormal;
                } else if (shot.hitWorld) {
                    anyHitWorld = true;
                    lastEnd = shot.end;
                    shotNormal = shot.hitNormal;
                }
            }
            fired = true;
            shotEnd = lastEnd;
            shotHitWorld = anyHitWorld && !anyHit;
            Debug::log(Debug::Category::NpcCombat,
                "[NPC SHOT] id=%u weapon=%s pellets=%d totalDamage=%.0f hit=%d\n",
                npc.id, def->id.c_str(), pelletCount, totalDamage, (int)anyHit);
        } else {
            // Single-pellet weapon (revolver): tryFireHitscanDir applies
            // its own spread internally — do NOT pre-spread here.
            RevolverShotResult shot = WeaponFire::tryFireHitscanDir(
                *def, rt, npc.body, world, npcPos, aimDir, &player, dmgMul, beamOverride);
            fired = shot.fired;
            if (fired) { shotEnd = shot.end; shotNormal = shot.hitNormal; shotHitWorld = shot.hitWorld; }
            Debug::log(Debug::Category::NpcCombat, "[NPC SHOT] id=%u weapon=%s hitscan hit=%d damage=%.0f\n",
                       npc.id, def->id.c_str(), (int)shot.hitEntity, shot.damage);
        }
        break;
    }
    case WeaponBehaviorType::Projectile:
    case WeaponBehaviorType::RocketLauncher:
    {
        WeaponRocketLauncher::fire(gNpcRocketState, *def, rt, npc.body, npcPos, aimDir);
        fired = true;
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
        RevolverShotResult shot = WeaponFire::tryFireHitscanDir(*def, rt, npc.body, world, npcPos, aimDir, &player, dmgMul, beamOverride);
        fired = shot.fired;
        if (fired) { shotEnd = shot.end; shotNormal = shot.hitNormal; }
        Debug::log(Debug::Category::NpcCombat, "[NPC SHOT] id=%u weapon=%s melee(approx) hit=%d\n",
                   npc.id, def->id.c_str(), (int)shot.hitEntity);
        break;
    }
    case WeaponBehaviorType::Godball:
    case WeaponBehaviorType::GrenadeLauncher:
    {
        WeaponGrenadeLauncher::fire(*def, rt, npc.body, npcPos, aimDir);
        fired = true;
        {
            float range = effectiveRange(*def);
            shotEnd = npcPos + aimDir * (range > 0.0f ? range : 100.0f);
        }
        Debug::log(Debug::Category::NpcCombat, "[NPC SHOT] id=%u weapon=%s grenadeLauncher dir=(%.2f %.2f %.2f)\n",
                   npc.id, def->id.c_str(), aimDir.x, aimDir.y, aimDir.z);
        break;
    }
    }

    if (player.currentHp < hpBeforeShot)
    {
        player.lastDamagedBy = npc.body.username.empty()
            ? "npc-" + std::to_string(npc.id) : npc.body.username;
        player.killedByWeapon = def->displayName.empty()
            ? def->id : def->displayName;
        Debug::log(Debug::Category::NpcCombat,
            "[NPC DAMAGE ATTRIBUTION] npc=%u name=%s weapon=%s victim=%s hpBefore=%d hpAfter=%d\n",
            npc.id, player.lastDamagedBy.c_str(), player.killedByWeapon.c_str(),
            player.username.c_str(), hpBeforeShot, player.currentHp);
    }

    // Remember the shot so the server broadcast sends the true tracer
    // (look == shoot == bullet endpoint). The model is NOT snapped to the shot
    // direction here — body.yaw follows currentFacing smoothly in buildInputState.
    if (fired)
    {
        npc.lastShotOrigin = npcPos;
        npc.lastShotEnd = shotEnd;
        npc.lastShotNormal = shotNormal;
        npc.hasLastShot = true;
        npc.lastShotHitWorld = shotHitWorld;
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
                                     Camera& camera, Player& player, float dt) {
    // Update NPC rocket launcher projectiles using the same logic as the player.
    // WeaponRocketLauncher::update handles movement, world collision, player/NPC
    // collision, and explosion damage — exactly the same path the player's rockets use.
    if (!gNpcRocketState.activeRockets.empty()) {
        // Find a rocket launcher definition from any NPC with one equipped
        for (Npc& npc : npcSystem.all()) {
            if (npc.body.dead || npc.body.currentHp <= 0.0f) continue;
            const WeaponDefinition* def = WeaponRegistry::instance().get(npc.body.equippedWeaponId);
            if (def && def->behaviorType == WeaponBehaviorType::RocketLauncher) {
                auto& rt = npc.body.weaponRuntimes[def->id];
                WeaponRocketLauncher::update(gNpcRocketState, *def, rt, npc.body, npcSystem, world, camera, dt, &player);
                break;
            }
        }
        // If no NPC has a rocket launcher equipped, still tick the game time
        // so rockets don't freeze if the NPC switched weapons mid-flight.
        gNpcRocketState.gameTime += dt;
    }
}

#include "weapon-rocket-launcher.h"
#include "weapon-types.h"
#include "weapon-fire.h"
#include "weapon-audio.h"
#include "config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "audio/audio.h"
#include "camera.h"
#include "combat/projectile-render.h"
#include "config/size-scaling-config.h"
#include "config/weapon-hitfx-config.h"
#include "entities/player.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "replay/replay.h"
#include "npc/npc.h"
#include "world/world.h"
#include "physics/physics-types.h"

namespace WeaponRocketLauncher {

static constexpr float IGNORE_OWNER_DIST = 0.3f;
static bool gDebugRockets = false;

static float cp(const WeaponDefinition& def, const char* key, float fallback)
{
    return def.customParams.count(key) ? def.customParams.at(key) : fallback;
}

void toggleDebug() { gDebugRockets = !gDebugRockets; }

static void doExplosion(
    RocketLauncherState& state,
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& owner,
    NpcSystem& npcs,
    Camera& camera,
    const glm::vec3& position,
    uint32_t directHitEntityId,
    bool directHitIsNpc)
{
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(owner.sizeScale, 0.001f);
    const float splashRadius = cp(def, "splashRadius", 8.0f) * sc.scale(1.0f, sc.explosionRadiusExponent, ss);
    const float splashExponent = cp(def, "splashExponent", 2.0f);
    const float baseDamage = cp(def, "rocketDirectDamage", 150.0f) * sc.scale(1.0f, sc.projectileDamageExponent, ss);
    const float knockbackStrength = cp(def, "knockbackStrength", 40.0f) * sc.scale(1.0f, sc.knockbackExponent, ss);
    const float selfKnockbackMul = cp(def, "selfKnockbackMultiplier", 0.8f);
    const float knockbackHorizontalMul = cp(def, "knockbackHorizontalMultiplier", 1.0f);
    const float knockbackVerticalMul = cp(def, "knockbackVerticalMultiplier", 1.0f);

    playWorldSound("rocketlauncher/rocketlauncherexplode", position, 1.0f, 1.0f, 50.0f);

    {
        HitEvent ev;
        ev.position = position;
        ev.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        ev.direction = glm::vec3(0.0f);
        ev.hitWorld = true;
        ev.hitEntity = false;
        ev.damage = 0;
        ev.attacker = owner.username;
        ev.weaponSource = "rocket_launcher";
        HitEffects::onHit(ev);
    }

    EffectPartSystem::instance().spawnMuzzleFlash(position, "rocket_explosion", owner.sizeScale);
    EffectPartSystem::instance().spawnWorldDebris(position, glm::vec3(0.0f, 0.0f, 1.0f), 3.0f, owner.sizeScale);
    {
        const auto& expCfg = WeaponHitFxConfig::instance().explosionBurstFor("rocket_launcher");
        if (expCfg.smoke.enabled)
        {
            for (int i = 0; i < expCfg.smoke.count; ++i) {
                EffectPart p;
                p.position = position + glm::vec3(
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread,
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread,
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread);
                p.velocity = glm::vec3(
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.speed,
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.speed,
                    (float)rand() / RAND_MAX * expCfg.smoke.speed * 0.5f + expCfg.smoke.upwardBias);
                p.lifetime = 0.0f;
                p.maxLifetime = expCfg.smoke.lifetime + (float)rand() / RAND_MAX * expCfg.smoke.lifetime * 0.3f;
                p.scale = expCfg.smoke.size + (float)rand() / RAND_MAX * expCfg.smoke.size * 0.5f;
                p.endScale = expCfg.smoke.endSize + (float)rand() / RAND_MAX * expCfg.smoke.endSize * 0.5f;
                p.color = expCfg.smoke.color;
                p.alpha = expCfg.smoke.alpha;
                p.gravity = 1.0f;
                p.affectedByGravity = true;
                p.billboardText = false;
                p.replayType = "rocket_launcher_explosion_smoke";
                EffectPartSystem::instance().spawn(p);
            }
        }
    }

    // ── Config-driven explosion sphere (expanding fireball) ──────────
    {
        const auto& expCfg = WeaponHitFxConfig::instance().explosionBurstFor("rocket_launcher");
        if (expCfg.sphere.enabled)
        {
            EffectPart sphere;
            sphere.position = position;
            sphere.maxLifetime = (float)expCfg.sphere.lifetimeTicks / 60.0f;
            sphere.scale = expCfg.sphere.startRadius;
            sphere.endScale = expCfg.sphere.endRadius;
            sphere.color = expCfg.sphere.startColor * expCfg.sphere.brightnessStart;
            sphere.alpha = expCfg.sphere.alphaStart;
            sphere.billboardText = false;
            sphere.replayType = "rocket_launcher_explosion_sphere";
            EffectPartSystem::instance().spawn(sphere);
        }
    }

    {
        float distToCam = glm::length(position - camera.pos);
        float shakeStrength = std::clamp(1.0f - distToCam / splashRadius, 0.0f, 1.0f);
        const auto& ssc = SizeScalingConfig::instance().data();
        float shakeMul = ssc.scale(1.0f, ssc.cameraShakeExponent, std::max(owner.sizeScale, 0.001f));
        camera.addPunch(shakeStrength * 4.0f * shakeMul, shakeStrength * 2.0f * shakeMul);
    }

    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
        glm::vec3 toEntity = npc.body.pos - position;
        float dist = glm::length(toEntity);
        if (dist >= splashRadius) continue;
        glm::vec3 dir = dist > 0.001f ? toEntity / dist : glm::vec3(0.0f, 1.0f, 0.0f);

        float dmg;
        if (directHitIsNpc && dist < 1.5f)
            dmg = baseDamage;
        else
            dmg = baseDamage * std::exp(-std::pow(dist / splashRadius, 2.0f) * splashExponent);

        int finalDmg = std::max(1, (int)std::round(dmg));
        npc.body.currentHp -= finalDmg;
        if (npc.body.currentHp < 0) npc.body.currentHp = 0;

        float t = dist / splashRadius;
        float knockScale = 1.0f - t * t;
        knockScale = knockScale * 0.85f + 0.15f;
        {
            float kb = knockbackStrength * knockScale;
            glm::vec3 kbVec(dir.x * kb * knockbackHorizontalMul,
                            dir.y * kb * knockbackHorizontalMul,
                            dir.z * kb * knockbackVerticalMul);
            npc.body.externalImpulse += kbVec;
        }

        HitEvent ev;
        ev.position = npc.body.pos;
        ev.normal = -dir;
        ev.direction = dir;
        ev.hitEntity = true;
        ev.damage = finalDmg;
        ev.attacker = owner.username;
        ev.weaponSource = "rocket_launcher";
        HitEffects::onHit(ev);
    }

    {
        glm::vec3 toSelf = owner.pos - position;
        float dist = glm::length(toSelf);
        if (dist < splashRadius) {
            glm::vec3 dir = dist > 0.001f ? toSelf / dist : glm::vec3(0.0f, 1.0f, 0.0f);
            float dmg;
            if (!directHitIsNpc && dist < 1.5f && directHitEntityId > 0) {
                dmg = baseDamage;
            } else {
                dmg = baseDamage * std::exp(-std::pow(dist / splashRadius, 2.0f) * splashExponent);
            }
            int finalDmg = std::max(1, (int)std::round(dmg));
            float t = dist / splashRadius;
            float knockScale = 1.0f - t * t;
            knockScale = knockScale * 0.85f + 0.15f;
            {
                float kb = knockbackStrength * selfKnockbackMul * knockScale;
                glm::vec3 kbDir(dir.x * knockbackHorizontalMul,
                                dir.y * knockbackHorizontalMul,
                                dir.z * knockbackVerticalMul);
                float kbLen = glm::length(kbDir);
                if (kbLen < 0.0001f) kbDir = glm::vec3(0.0f, 1.0f, 0.0f);
                else kbDir /= kbLen;
                owner.takeDamage(finalDmg, kbDir, kb);
            }
        }
    }
}

void fire(
    RocketLauncherState& state,
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& owner,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir)
{
    const float rocketSpeed = def.projectileSpeed > 0.0f
        ? def.projectileSpeed : 50.0f;

    unsigned int rng = (unsigned int)(runtime.shootEffectTimer * 1000.0f) + 1;
    glm::vec3 dir = WeaponFire::computeSpreadDirection(muzzleDir, def.spread, rng);
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Final Direction Used By Projectile: (%.4f, %.4f, %.4f)\n",
        dir.x, dir.y, dir.z);

    float spawnAhead = 1.2f;
    glm::vec3 spawnPos = muzzlePos + dir * spawnAhead;
    Debug::warn(Debug::Category::Weapons,
        "[AIM] Projectile Origin: (%.2f, %.2f, %.2f) spawn=(%.2f, %.2f, %.2f)\n",
        muzzlePos.x, muzzlePos.y, muzzlePos.z,
        spawnPos.x, spawnPos.y, spawnPos.z);

    RocketLauncherState::Rocket rocket;
    rocket.position = spawnPos;
    rocket.prevPosition = muzzlePos;
    rocket.velocity = dir * rocketSpeed;
    rocket.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    rocket.lifetime = def.projectileLifetime > 0.0f ? def.projectileLifetime : 5.0f;
    rocket.distanceTraveled = 0.0f;
    rocket.exploded = false;
    rocket.ownerId = (uint32_t)(uintptr_t)(&owner);
    rocket.spawnTime = state.gameTime;
    rocket.smokeAccumulator = 0.0f;
    rocket.fireSerial = 0;
    rocket.authoritativeProjectileId = 0;

    state.activeRockets.push_back(rocket);

    {
        ReplayEffectEvent projEvent;
        projEvent.type = "projectile_spawn";
        projEvent.position = spawnPos;
        projEvent.velocity = dir * rocketSpeed;
        projEvent.lifetime = rocket.lifetime;
        projEvent.sourceActorId = std::to_string(rocket.ownerId);
        captureReplayEffect(projEvent);
    }
    {
        ReplayEffectEvent gunshotEvent;
        gunshotEvent.type = "gunshot";
        gunshotEvent.from = muzzlePos;
        gunshotEvent.to = muzzlePos + dir * 2.0f;
        gunshotEvent.sourceActorId = std::to_string(rocket.ownerId);
        captureReplayEffect(gunshotEvent);
    }

    runtime.currentAmmo--;
    runtime.fireCooldown = def.fireDelay;

    if (!def.soundShoot.empty())
        WeaponAudio::playShootSound(def, owner.pos);
}

void update(
    RocketLauncherState& state,
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& owner,
    NpcSystem& npcs,
    const World& world,
    Camera& camera,
    float dt)
{
    if (dt <= 0.0f) return;
    state.gameTime += dt;

    // Read config values for smoke (hot-reloadable). Projectile visuals are
    // read in WeaponSystem::render during the render pass.
    bool smokeEnabled = cp(def, "smokeEnabled", 1.0f) > 0.0f;
    float smokeEmissionRate = cp(def, "smokeEmissionRate", 50.0f);
    int smokeParticlesPerEm = (int)cp(def, "smokeParticlesPerEmission", 2.0f);
    glm::vec3 smokeSpawnOffset(
        cp(def, "smokeSpawnOffsetX", 0.0f),
        cp(def, "smokeSpawnOffsetY", 0.0f),
        cp(def, "smokeSpawnOffsetZ", -0.5f));
    float smokeSpawnRadius = cp(def, "smokeSpawnRadius", 0.08f);
    float smokeInheritVel = cp(def, "smokeInheritVelocity", -0.1f);
    float smokeSpeed = cp(def, "smokeSpeed", 1.0f);
    float smokeSpeedRand = cp(def, "smokeSpeedRandom", 0.5f);
    float smokeSpreadDeg = cp(def, "smokeSpreadDegrees", 25.0f);
    float smokeLifetime = cp(def, "smokeLifetime", 1.2f);
    float smokeLifetimeRand = cp(def, "smokeLifetimeRandom", 0.25f);
    float smokeSize = cp(def, "smokeSize", 0.25f);
    float smokeEndSize = cp(def, "smokeEndSize", 0.8f);
    float smokeSizeRand = cp(def, "smokeSizeRandom", 0.1f);
    float smokeGravity = cp(def, "smokeGravity", 0.0f);
    float smokeDrag = cp(def, "smokeDrag", 1.5f);
    glm::vec3 smokeColor(
        cp(def, "smokeColorR", 0.7f),
        cp(def, "smokeColorG", 0.7f),
        cp(def, "smokeColorB", 0.7f));
    float smokeAlpha = cp(def, "smokeColorA", 0.8f);
    glm::vec3 smokeEndColor(
        cp(def, "smokeEndColorR", 0.2f),
        cp(def, "smokeEndColorG", 0.2f),
        cp(def, "smokeEndColorB", 0.2f));
    float smokeEndAlpha = cp(def, "smokeEndColorA", 0.0f);

    for (auto it = state.activeRockets.begin(); it != state.activeRockets.end(); )
    {
        RocketLauncherState::Rocket& rocket = *it;

        if (rocket.exploded) {
            it = state.activeRockets.erase(it);
            continue;
        }

        rocket.lifetime -= dt;
        if (rocket.lifetime <= 0.0f) {
            doExplosion(state, def, runtime, owner, npcs, camera, rocket.position, 0, false);
            rocket.exploded = true;
            it = state.activeRockets.erase(it);
            continue;
        }

        rocket.prevPosition = rocket.position;
        float stepDist = glm::length(rocket.velocity * dt);
        glm::vec3 newPos = rocket.position + rocket.velocity * dt;
        rocket.distanceTraveled += stepDist;

        // ── World collision ──
        {
            bool hitWorld = false;
            glm::vec3 worldHitPos = newPos;
            glm::vec3 worldNormal{0.0f, 0.0f, 1.0f};
            glm::vec3 rayDir = glm::normalize(rocket.velocity);
            float maxDist = stepDist;

            if (maxDist > 0.001f) {
                float nearest = maxDist;
                for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
                    float t;
                    if (WeaponFire::rayTriangle(rocket.position, rayDir, tri, t) && t < nearest) {
                        nearest = t;
                        hitWorld = true;
                        worldHitPos = rocket.position + rayDir * t;
                        worldNormal = tri.normal;
                    }
                }
                if (hitWorld) {
                    doExplosion(state, def, runtime, owner, npcs, camera, worldHitPos, 0, false);
                    rocket.exploded = true;
                    it = state.activeRockets.erase(it);
                    continue;
                }
            }
        }

        // ── Player collision ──
        {
            bool hitPlayer = false;
            glm::vec3 checkPos = newPos;
            Capsule ownerCapsule = owner.getCapsule();
            glm::vec3 mn(owner.pos.x - ownerCapsule.r,
                         owner.pos.y - ownerCapsule.r,
                         ownerCapsule.a.z - ownerCapsule.r);
            glm::vec3 mx(owner.pos.x + ownerCapsule.r,
                         owner.pos.y + ownerCapsule.r,
                         ownerCapsule.b.z + ownerCapsule.r);
            glm::vec3 closest = glm::clamp(checkPos, mn, mx);
            float dist = glm::length(checkPos - closest);
            if (dist < 0.5f && rocket.distanceTraveled >= IGNORE_OWNER_DIST) {
                hitPlayer = true;
            }
            if (!hitPlayer) {
                // ── NPC collision ──
                bool hitNpc = false;
                uint32_t hitNpcId = 0;
                for (Npc& npc : npcs.all()) {
                    if (npc.body.currentHp <= 0) continue;
                    npc.body.updateModelWorldTransforms();
                    for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
                        glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                        glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                        glm::vec3 half = (part.collider.localMax - part.collider.localMin) * 0.5f;
                        half = glm::max(half, glm::vec3(0.2f));
                        glm::vec3 closest2 = glm::clamp(checkPos, center - half, center + half);
                        float dist2 = glm::length(checkPos - closest2);
                        if (dist2 < 0.5f) {
                            hitNpc = true;
                            hitNpcId = npc.id;
                            break;
                        }
                    }
                    if (hitNpc) break;
                }
                if (hitNpc) {
                    doExplosion(state, def, runtime, owner, npcs, camera, checkPos, hitNpcId, true);
                    rocket.exploded = true;
                    it = state.activeRockets.erase(it);
                    continue;
                }
                if (hitPlayer) {
                    doExplosion(state, def, runtime, owner, npcs, camera, checkPos, 0, false);
                    rocket.exploded = true;
                    it = state.activeRockets.erase(it);
                    continue;
                }
            }
        }

        rocket.position = newPos;

        // Update orientation from velocity direction
        float velLen = glm::length(rocket.velocity);
        if (velLen > 0.001f) {
            glm::vec3 velDir = rocket.velocity / velLen;
            glm::quat targetOrient = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            // Align local +Z to velocity direction
            if (std::fabs(velDir.z) < 0.9999f) {
                glm::vec3 up(0.0f, 0.0f, 1.0f);
                glm::vec3 axis = glm::cross(up, velDir);
                float angle = std::acos(glm::clamp(glm::dot(up, velDir), -1.0f, 1.0f));
                if (glm::length(axis) > 0.0001f)
                    targetOrient = glm::angleAxis(angle, glm::normalize(axis));
                else if (velDir.z < 0.0f)
                    targetOrient = glm::angleAxis(3.14159265f, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            rocket.orientation = glm::mix(rocket.orientation, targetOrient, std::min(1.0f, dt * 10.0f));
        }

        // ── In-air looping sound ──
        {
            float inAirInterval = 0.15f;
            if (state.gameTime - rocket.lastInAirSoundTime >= inAirInterval) {
                float volDb = cp(def, "flightSoundVolumeDb", 15.0f);
                float volMul = powf(10.0f, volDb / 20.0f);
                playWorldSound("rocketlauncher/rocketlauncherinair", rocket.position,
                    0.5f * volMul, 1.0f, 40.0f);
                rocket.lastInAirSoundTime = state.gameTime;
            }
        }

        // ── Textured rendering (handled in WeaponSystem::render during render pass) ──

        // ── Config-controlled smoke trail ──
        if (smokeEnabled) {
            rocket.smokeAccumulator += smokeEmissionRate * dt;
            while (rocket.smokeAccumulator >= 1.0f) {
                rocket.smokeAccumulator -= 1.0f;
                for (int pi = 0; pi < smokeParticlesPerEm; ++pi) {
                    EffectPart p;
                    // Spawn behind the rocket based on direction
                    glm::vec3 velDir = glm::length(rocket.velocity) > 0.001f
                        ? glm::normalize(rocket.velocity) : glm::vec3(0.0f, 0.0f, 1.0f);
                    glm::vec3 trailPos = rocket.position +
                        velDir * smokeSpawnOffset.z +
                        glm::vec3(
                            ((float)rand() / RAND_MAX - 0.5f) * smokeSpawnRadius * 2.0f,
                            ((float)rand() / RAND_MAX - 0.5f) * smokeSpawnRadius * 2.0f,
                            ((float)rand() / RAND_MAX - 0.5f) * smokeSpawnRadius * 2.0f);

                    p.position = trailPos;
                    float a = (float)rand() / RAND_MAX;
                    float spreadRad = smokeSpreadDeg * 3.14159265f / 180.0f;
                    glm::vec3 spread(
                        (a - 0.5f) * spreadRad,
                        (a - 0.5f) * spreadRad,
                        (a - 0.5f) * spreadRad);
                    p.velocity = rocket.velocity * smokeInheritVel + spread * smokeSpeed +
                        glm::vec3(
                            ((float)rand() / RAND_MAX - 0.5f) * smokeSpeedRand * 2.0f,
                            ((float)rand() / RAND_MAX - 0.5f) * smokeSpeedRand * 2.0f,
                            ((float)rand() / RAND_MAX - 0.5f) * smokeSpeedRand * 2.0f);
                    float lf = smokeLifetime + ((float)rand() / RAND_MAX - 0.5f) * smokeLifetimeRand * 2.0f;
                    p.lifetime = 0.0f;
                    p.maxLifetime = std::max(0.5f, lf);
                    float sz = smokeSize + ((float)rand() / RAND_MAX - 0.5f) * smokeSizeRand * 2.0f;
                    p.scale = std::max(0.01f, sz);
                    p.endScale = std::max(0.01f, smokeEndSize);
                    p.color = smokeColor;
                    p.alpha = smokeAlpha;
                    p.gravity = smokeGravity;
                    p.affectedByGravity = std::fabs(smokeGravity) > 0.001f;
                    p.billboardText = false;
                    p.replayType = "rocket_trail";
                    EffectPartSystem::instance().spawn(p);
                }
            }
        }

        // ── Debug ──
        if (gDebugRockets) {
            DebugVis::drawFilledSphere(camera, rocket.position, 0.15f,
                {0.0f, 1.0f, 1.0f, 1.0f});
            DebugVis::drawLine(camera, rocket.prevPosition, rocket.position,
                {1.0f, 1.0f, 0.0f, 1.0f});
            glm::vec3 velEnd = rocket.position + glm::normalize(rocket.velocity) * 2.0f;
            DebugVis::drawLine(camera, rocket.position, velEnd,
                {0.0f, 1.0f, 0.0f, 1.0f});
            float splashR = cp(def, "splashRadius", 8.0f);
            DebugVis::drawWireSphere(camera, rocket.position, splashR,
                {1.0f, 0.0f, 0.0f, 0.3f});
            printf("[ROCKET DEBUG] distTraveled=%.2f/%.1f pos=(%.1f,%.1f,%.1f)\n",
                   rocket.distanceTraveled, IGNORE_OWNER_DIST,
                   rocket.position.x, rocket.position.y, rocket.position.z);
        }

        ++it;
    }
}

void clear(RocketLauncherState& state) {
    state.activeRockets.clear();
}

void tagLatestLocalRocket(RocketLauncherState& state, uint32_t fireSerial)
{
    if (fireSerial == 0 || state.activeRockets.empty())
        return;
    for (auto it = state.activeRockets.rbegin(); it != state.activeRockets.rend(); ++it)
    {
        if (it->fireSerial == 0 && it->authoritativeProjectileId == 0)
        {
            it->fireSerial = fireSerial;
            return;
        }
    }
}

bool attachAuthoritativeRocket(RocketLauncherState& state, uint32_t fireSerial, uint32_t projectileId)
{
    if (fireSerial == 0 || projectileId == 0)
        return false;
    for (auto& rocket : state.activeRockets)
    {
        if (rocket.fireSerial == fireSerial)
        {
            rocket.authoritativeProjectileId = projectileId;
            return true;
        }
    }
    return false;
}

bool removeAuthoritativeRocket(RocketLauncherState& state, uint32_t projectileId)
{
    if (projectileId == 0)
        return false;
    for (auto it = state.activeRockets.begin(); it != state.activeRockets.end(); ++it)
    {
        if (it->authoritativeProjectileId == projectileId)
        {
            state.activeRockets.erase(it);
            return true;
        }
    }
    return false;
}

bool removeLocalRocketByFireSerial(RocketLauncherState& state, uint32_t fireSerial)
{
    if (fireSerial == 0)
        return false;
    for (auto it = state.activeRockets.begin(); it != state.activeRockets.end(); ++it)
    {
        if (it->fireSerial == fireSerial)
        {
            state.activeRockets.erase(it);
            return true;
        }
    }
    return false;
}

} // namespace WeaponRocketLauncher

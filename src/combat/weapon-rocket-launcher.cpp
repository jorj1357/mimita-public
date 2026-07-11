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
#include "config/size-scaling-config.h"
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
    const float splashRadius = (def.customParams.count("splashRadius")
        ? def.customParams.at("splashRadius") : 8.0f) * sc.scale(1.0f, sc.explosionRadiusExponent, ss);
    const float splashExponent = def.customParams.count("splashExponent")
        ? def.customParams.at("splashExponent") : 2.0f;
    const float baseDamage = (def.customParams.count("rocketDirectDamage")
        ? def.customParams.at("rocketDirectDamage") : 150.0f) * sc.scale(1.0f, sc.projectileDamageExponent, ss);
    const float knockbackStrength = (def.customParams.count("knockbackStrength")
        ? def.customParams.at("knockbackStrength") : 40.0f) * sc.scale(1.0f, sc.knockbackExponent, ss);
    const float selfKnockbackMul = def.customParams.count("selfKnockbackMultiplier")
        ? def.customParams.at("selfKnockbackMultiplier") : 0.8f;
    const float knockbackHorizontalMul = def.customParams.count("knockbackHorizontalMultiplier")
        ? def.customParams.at("knockbackHorizontalMultiplier") : 1.0f;
    const float knockbackVerticalMul = def.customParams.count("knockbackVerticalMultiplier")
        ? def.customParams.at("knockbackVerticalMultiplier") : 1.0f;

    playWorldSound("weapon/bomb/explosion2", position, 1.0f, 1.0f, 50.0f);

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

    // Spawn well in front of the muzzle (outside player capsule)
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
    rocket.lifetime = def.projectileLifetime > 0.0f ? def.projectileLifetime : 5.0f;
    rocket.distanceTraveled = 0.0f;
    rocket.exploded = false;
    rocket.ownerId = (uint32_t)(uintptr_t)(&owner);
    rocket.spawnTime = state.gameTime;

    state.activeRockets.push_back(rocket);

    // Record projectile spawn for replay
    {
        ReplayEffectEvent projEvent;
        projEvent.type = "projectile_spawn";
        projEvent.position = spawnPos;
        projEvent.velocity = dir * rocketSpeed;
        projEvent.lifetime = rocket.lifetime;
        projEvent.sourceActorId = std::to_string(rocket.ownerId);
        captureReplayEffect(projEvent);
    }
    // Record gunshot (muzzle flash + tracer) for replay
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

        // ── Player collision (skip owner if too close to spawn) ──
        bool hitPlayer = false;
        glm::vec3 checkPos = newPos;
        {
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
            } else if (dist < 0.5f && rocket.distanceTraveled < IGNORE_OWNER_DIST) {
                // Too close to owner — skip owner collision but still check NPCs
                printf("[ROCKET] Skipping owner collision: distTraveled=%.2f < IGNORE(%.1f)\n",
                       rocket.distanceTraveled, IGNORE_OWNER_DIST);
            }
        }

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
                glm::vec3 closest = glm::clamp(checkPos, center - half, center + half);
                float dist = glm::length(checkPos - closest);
                if (dist < 0.5f) {
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

        rocket.position = newPos;

        // ── Render rocket as bright yellow cylinder facing movement direction ──
        {
            glm::vec3 velDir = glm::normalize(rocket.velocity);
            // Main body: bright yellow cylinder
            float cylRadius = 0.18f;
            float cylHeight = 0.6f;
            DebugVis::drawFilledCylinder(camera, rocket.position, velDir, cylRadius, cylHeight,
                {1.0f, 1.0f, 0.0f, 1.0f});

            // Nose cone: bright sphere at the front tip
            glm::vec3 tipPos = rocket.position + velDir * (cylHeight * 0.5f + 0.1f);
            DebugVis::drawFilledSphere(camera, tipPos, cylRadius * 1.3f,
                {1.0f, 0.95f, 0.1f, 1.0f});

            // Outer glow: large semi-transparent sphere for visibility
            DebugVis::drawFilledSphere(camera, rocket.position, 0.5f,
                {1.0f, 0.8f, 0.0f, 0.3f});
        }

        // ── Smoke trail ──
        {
            glm::vec3 velDir = glm::normalize(rocket.velocity);
            glm::vec3 trailPos = rocket.position - velDir * 0.3f;
            for (int ti = 0; ti < 3; ++ti) {
                EffectPart p;
                p.position = trailPos;
                p.velocity = glm::vec3(
                    ((float)rand() / RAND_MAX - 0.5f) * 2.0f,
                    ((float)rand() / RAND_MAX - 0.5f) * 2.0f,
                    ((float)rand() / RAND_MAX - 0.5f) * 2.0f);
                float t = (float)rand() / RAND_MAX;
                p.color = {1.0f, 0.6f + t * 0.3f, 0.1f};
                p.maxLifetime = 0.4f + t * 0.2f;
                p.scale = 0.08f + t * 0.06f;
                p.endScale = 0.2f + t * 0.12f;
                p.alpha = 0.85f;
                p.replayType = "rocket_trail";
                EffectPartSystem::instance().spawn(p);
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

            float splashR = def.customParams.count("splashRadius")
                ? def.customParams.at("splashRadius") : 8.0f;
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

} // namespace WeaponRocketLauncher

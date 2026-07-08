#include "weapon-hafs.h"
#include "weapon-types.h"
#include "weapon-audio.h"
#include "camera.h"
#include "audio/audio.h"
#include "debug/debug-log.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "physics/physics-types.h"

bool gHafsDebug = false;

#include <algorithm>
#include <cmath>
#include <cstdio>

// ── Helpers ─────────────────────────────────────────────────

static float cp(const WeaponDefinition& def, const char* key, float fallback) {
    auto it = def.customParams.find(key);
    return (it != def.customParams.end()) ? it->second : fallback;
}

static glm::vec3 getSwordTip(const Player& owner) {
    // Sword extends from the weapon collision capsule's B end (muzzle/tip)
    return owner.weaponCollisionCapsule.b;
}

static glm::vec3 getSwordGrip(const Player& owner) {
    return owner.weaponCollisionCapsule.a;
}

// Swept sphere vs target sphere (reused from godball pattern)
static bool sweptSphereOverlap(
    glm::vec3 prevCenter, glm::vec3 currCenter, float radius,
    glm::vec3 targetCenter, float targetRadius,
    float& outDist)
{
    glm::vec3 seg = currCenter - prevCenter;
    float segLen = glm::length(seg);
    if (segLen < 0.0001f) {
        // Static overlap test
        float d = glm::length(currCenter - targetCenter);
        float overlap = radius + targetRadius - d;
        if (overlap > 0.0f) {
            outDist = d;
            return true;
        }
        return false;
    }
    glm::vec3 segDir = seg / segLen;
    glm::vec3 toTarget = targetCenter - prevCenter;
    float t = glm::clamp(glm::dot(toTarget, segDir), 0.0f, segLen);
    glm::vec3 closest = prevCenter + segDir * t;
    float d = glm::length(closest - targetCenter);
    float overlap = radius + targetRadius - d;
    if (overlap > 0.0f) {
        outDist = d;
        return true;
    }
    return false;
}

// ── Attack triggers ─────────────────────────────────────────

void WeaponHafs::startSlash(HafsState& state, const WeaponDefinition& def, Player& owner) {
    state.currentAttack = HafsState::AttackType::Slash;
    state.attackTimer = 0.0f;
    float poseTime = cp(def, "slashPoseTime", 0.22f);
    float recoverTime = cp(def, "slashRecoverTime", 0.10f);
    state.attackDuration = poseTime + recoverTime;
    state.attackForward = owner.aimDirection;
    if (glm::length(state.attackForward) < 0.001f)
        state.attackForward = {1.0f, 0.0f, 0.0f};
    state.attackForward.z = 0.0f;
    state.attackForward = glm::normalize(state.attackForward);
    state.hitCooldowns.clear();
    WeaponAudio::playShootSound(def, owner.pos);
    Debug::log(Debug::Category::Weapons, "[HAFS] slash duration=%.2f dir=(%.2f %.2f)\n",
               state.attackDuration, state.attackForward.x, state.attackForward.y);
}

void WeaponHafs::startLunge(HafsState& state, const WeaponDefinition& def, Player& owner) {
    state.currentAttack = HafsState::AttackType::Lunge;
    state.attackTimer = 0.0f;
    float poseTime = cp(def, "lungePoseTime", 0.28f);
    float recoverTime = cp(def, "lungeRecoverTime", 0.12f);
    state.attackDuration = poseTime + recoverTime;
    state.attackForward = owner.aimDirection;
    if (glm::length(state.attackForward) < 0.001f)
        state.attackForward = {1.0f, 0.0f, 0.0f};
    state.attackForward.z = 0.0f;
    state.attackForward = glm::normalize(state.attackForward);
    state.hitCooldowns.clear();
    float lungeImpulse = cp(def, "lungeForwardImpulse", 12.0f);
    owner.vel += state.attackForward * lungeImpulse + glm::vec3(0, 0, 5.0f);
    WeaponAudio::playShootSound(def, owner.pos);
    Debug::log(Debug::Category::Weapons, "[HAFS] lunge duration=%.2f impulse=%.1f dir=(%.2f %.2f)\n",
               state.attackDuration, lungeImpulse, state.attackForward.x, state.attackForward.y);
}

// ── Bullet blocked ──────────────────────────────────────────

void WeaponHafs::onBulletBlocked(HafsState& state, Player& owner) {
    state.blockedBulletsThisFrame++;
    playWorldSound("weapon/hafs/hafsWorldHit", owner.pos, 0.6f, 1.0f, 20.0f);
    Debug::log(Debug::Category::Weapons, "[HAFS] bullet blocked (total=%d)\n",
               state.blockedBulletsThisFrame);
}

// ── Per-frame update ────────────────────────────────────────

void WeaponHafs::update(HafsState& state, const WeaponDefinition& def,
                        WeaponRuntime& runtime, Player& owner,
                        NpcSystem& npcs, const Camera& camera,
                        const World& world, float dt)
{
    // ── Sword velocity from tip movement ─────────────────
    glm::vec3 tip = getSwordTip(owner);
    glm::vec3 grip = getSwordGrip(owner);
    if (glm::length(state.prevSwordTip) > 0.001f) {
        state.swordVelocity = (tip - state.prevSwordTip) / std::max(dt, 0.0001f);
        state.swordSpeed = glm::length(state.swordVelocity);

        // Angular velocity: angle change of the blade direction
        glm::vec3 bladeDir = tip - grip;
        glm::vec3 prevBladeDir = state.prevSwordTip - state.prevSwordGrip;
        float bladeLen = glm::length(bladeDir);
        float prevLen = glm::length(prevBladeDir);
        if (bladeLen > 0.001f && prevLen > 0.001f) {
            float angleDelta = glm::degrees(std::acos(std::clamp(
                glm::dot(bladeDir / bladeLen, prevBladeDir / prevLen), -1.0f, 1.0f)));
            state.swordAngularVelocity = angleDelta / std::max(dt, 0.0001f);
        }
    }
    state.prevSwordTip = tip;
    state.prevSwordGrip = grip;

    // ── Attack timer ─────────────────────────────────────
    if (state.currentAttack != HafsState::AttackType::None) {
        state.attackTimer += dt;
        if (state.attackTimer >= state.attackDuration) {
            state.currentAttack = HafsState::AttackType::None;
        }
    }

    // ── Continuous collision damage ──────────────────────
    // Generate spheres along the blade and test vs NPC body parts
    state.collisionCount = 0;
    state.lastDamage = 0.0f;
    state.impactForce = 0.0f;

    // Blade spheres: 10 points along the capsule
    constexpr int BLADE_SPHERES = 10;
    std::vector<glm::vec3> bladePoints;
    bladePoints.reserve(BLADE_SPHERES);
    for (int i = 0; i < BLADE_SPHERES; i++) {
        float t = (float)i / (float)(BLADE_SPHERES - 1);
        bladePoints.push_back(glm::mix(grip, tip, t));
    }

    // Previous frame positions for swept test
    std::vector<glm::vec3> prevBladePoints;
    if (glm::length(state.prevSwordGrip) > 0.001f) {
        prevBladePoints.reserve(BLADE_SPHERES);
        for (int i = 0; i < BLADE_SPHERES; i++) {
            float t = (float)i / (float)(BLADE_SPHERES - 1);
            prevBladePoints.push_back(glm::mix(state.prevSwordGrip, state.prevSwordTip, t));
        }
    }

    float sphereRadius = 0.25f;

    for (Npc& npc : npcs.all()) {
        if (npc.body.dead) continue;

        // Hit cooldown
        auto& cd = state.hitCooldowns[npc.id];
        if (cd > 0.0f) { cd -= dt; continue; }

        for (const auto& part : npc.body.physicalBody.parts) {
            // Compute body part center from world transform
            glm::vec3 partCenter = glm::vec3(part.worldTransform[3]);
            float partRadius = 0.3f;

            for (int bi = 0; bi < BLADE_SPHERES; bi++) {
                bool hit = false;
                float hitDist = 0.0f;

                if (!prevBladePoints.empty()) {
                    hit = sweptSphereOverlap(prevBladePoints[bi], bladePoints[bi], sphereRadius,
                                             partCenter, partRadius, hitDist);
                } else {
                    // Static test
                    float d = glm::length(bladePoints[bi] - partCenter);
                    hit = (d < sphereRadius + partRadius);
                    hitDist = d;
                }

                if (hit) {
                    state.collisionCount++;

                    // Read damage params from config
                    float baseDamagePerTick = cp(def, "baseDamagePerTick", 12.0f);
                    float speedDmgFactor = cp(def, "speedDamageFactor", 22.0f);
                    float angleDmgFactor = cp(def, "angleDamageFactor", 2.0f);
                    float faceDmgFactor = cp(def, "faceNormalDamageFactor", 2.0f);
                    float maxDmgCap = cp(def, "maxDamageCap", 999999.0f);

                    float speedFactor = state.swordSpeed * speedDmgFactor * 0.01f;
                    float angleBonus = 0.0f;
                    glm::vec3 bladeDirVec = (tip - grip);
                    float bLen = glm::length(bladeDirVec);
                    if (bLen > 0.001f) {
                        glm::vec3 toVictim = npc.body.pos - tip;
                        float toLen = glm::length(toVictim);
                        if (toLen > 0.001f) {
                            float dotVal = glm::dot(bladeDirVec / bLen, toVictim / toLen);
                            angleBonus = std::max(0.0f, dotVal) * angleDmgFactor;
                        }
                    }

                    float damage = baseDamagePerTick + speedFactor + angleBonus;
                    if (state.currentAttack == HafsState::AttackType::Lunge)
                        damage *= cp(def, "lungeDamageMultiplier", 1.4f);
                    else
                        damage *= cp(def, "slashDamageMultiplier", 1.0f);
                    damage = std::min(damage, maxDmgCap);
                    state.lastDamage = damage;

                    // Knockback from config
                    glm::vec3 kbDir = glm::normalize(npc.body.pos - tip);
                    if (glm::length(kbDir) < 0.001f) kbDir = {0.0f, 1.0f, 0.0f};
                    float baseKb = cp(def, "baseKnockback", 35.0f);
                    float speedKbFactor = cp(def, "speedKnockbackFactor", 4.0f);
                    float angleKbFactor = cp(def, "angleKnockbackFactor", 2.0f);
                    float kbForce = baseKb + state.swordSpeed * speedKbFactor + angleBonus * angleKbFactor;
                    if (state.currentAttack == HafsState::AttackType::Lunge)
                        kbForce *= cp(def, "lungeKnockbackMultiplier", 1.6f);
                    else
                        kbForce *= cp(def, "slashKnockbackMultiplier", 1.0f);
                    state.lastKnockback = kbForce;
                    state.impactForce = kbForce;

                    // Apply damage
                    npc.body.takeDamage(damage, kbDir, kbForce);
                    Debug::log(Debug::Category::Weapons,
                        "[HAFS] hit npc=%u damage=%.1f kb=%.1f speed=%.1f\n",
                        npc.id, damage, kbForce, state.swordSpeed);

                    // Hit sound
                    WeaponAudio::playHitSound(def, partCenter);

                    // Knockback sound for strong hits
                    float kbSoundMin = cp(def, "knockbackSoundMinForce", 40.0f);
                    if (kbForce > kbSoundMin)
                        playWorldSound("weapon/hafs/hafsknockback", partCenter, 0.8f, 1.0f, 30.0f);

                    // Cooldown from config
                    float tickInterval = cp(def, "damageTickInterval", 0.05f);
                    cd = tickInterval;
                    break; // one hit per NPC per frame
                }
            }
            if (cd > 0.0f) break; // already hit this NPC
        }
    }

    // ── Player balance effects (from config) ──────────────
    if (cp(def, "heavyWeaponEnabled", 1.0f) > 0.0f) {
        float turnResist = cp(def, "heavyTurnResistance", 0.10f);
        if (state.swordAngularVelocity > 180.0f && turnResist > 0.0f) {
            float resistance = glm::clamp((state.swordAngularVelocity - 180.0f) / 720.0f, 0.0f, turnResist);
            owner.vel *= (1.0f - resistance * dt * 5.0f);
        }

        float moveDrag = cp(def, "heavyMovementDrag", 0.04f);
        if (moveDrag > 0.0f) {
            owner.vel *= (1.0f - moveDrag * dt * 60.0f);
        }

        float airPenalty = cp(def, "heavyAirControlPenalty", 0.06f);
        if (!owner.ground.onGround && airPenalty > 0.0f) {
            owner.vel *= (1.0f - airPenalty * dt * 60.0f);
        }
    }

    // ── Debug ─────────────────────────────────────────────
    if (gHafsDebug) {
        static float logTimer = 0.0f;
        logTimer += dt;
        if (logTimer >= 1.0f) {
            logTimer = 0.0f;
            printf("[HAFS DEBUG] attack=%d speed=%.1f angVel=%.1f collisions=%d damage=%.1f kb=%.1f blocked=%d\n",
                   (int)state.currentAttack, state.swordSpeed, state.swordAngularVelocity,
                   state.collisionCount, state.lastDamage, state.lastKnockback,
                   state.blockedBulletsThisFrame);
            state.blockedBulletsThisFrame = 0;
        }
    }
}

// 08 25 2026, 00 00
/* purpose
* Implements QuickHit client-side state machine: instant punch pose, capsule, sound restart.
* Owns input detection, attack lifecycle, smooth visual return, and local NPC hit detection.
* Does NOT own server-authoritative damage, swept collision validation, or episode batching.
* Does NOT send network packets or replicate effects to remote players.
* Does NOT render the glowing capsule for remote players (that is a separate render path).
*/

#include "weapon-quick-hit.h"
#include "weapon-types.h"
#include "weapon-audio.h"
#include "weapon-execution.h"
#include "weapon-registry.h"
#include "camera.h"
#include "audio/audio.h"
#include "debug/debug-log.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "physics/physics-types.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

static constexpr unsigned int QUICKHIT_SOUND_OWNER_ID = 0xFFFF0001;

static float qhCp(const WeaponDefinition& def, const char* key, float fallback) {
    auto it = def.customParams.find(key);
    return (it != def.customParams.end()) ? it->second : fallback;
}

// ── Capsule from right arm world transform ────────────────────

Capsule WeaponQuickHit::computeArmCapsule(const Player& owner,
                                           const WeaponDefinition& def)
{
    const float capsuleRadius = qhCp(def, "hitboxRadius", 0.22f);
    const float capsuleLength = qhCp(def, "hitboxLength", 0.85f);

    // Find rightArm body part
    glm::vec3 armCenter(0.0f);
    bool foundArm = false;
    for (const PhysicalBodyPart& part : owner.physicalBody.parts) {
        if (part.name == "rightArm") {
            armCenter = glm::vec3(part.worldTransform[3]);
            foundArm = true;
            break;
        }
    }

    if (!foundArm) {
        // Fallback: approximate from position + forward
        glm::vec3 forward = owner.aimDirection;
        if (glm::length(forward) < 0.001f) forward = glm::vec3(0.0f, 1.0f, 0.0f);
        forward = glm::normalize(forward);
        armCenter = owner.pos + glm::vec3(0.0f, 0.0f, 1.2f) + forward * 0.6f;
    }

    // Compute forward direction for capsule extension
    glm::vec3 forward = owner.aimDirection;
    if (glm::length(forward) < 0.001f) forward = glm::vec3(0.0f, 1.0f, 0.0f);
    forward = glm::normalize(forward);

    // Capsule: A is at the arm center (shoulder area), B extends forward along the arm
    Capsule cap;
    cap.a = armCenter;
    cap.b = armCenter + forward * capsuleLength;
    cap.r = capsuleRadius;
    return cap;
}

// ── Attack start ─────────────────────────────────────────────

void WeaponQuickHit::startAttack(QuickHitState& state, const WeaponDefinition& def,
                                  Player& owner, Camera& camera)
{
    // Store previous capsule for swept test
    if (state.active) {
        state.previousArmCapsule = state.currentArmCapsule;
    } else {
        state.previousArmCapsule = computeArmCapsule(owner, def);
    }

    // Advance sequence
    state.attackSequenceId++;
    if (state.attackSequenceId == 0) state.attackSequenceId = 1;

    // Reset attack timers
    state.activeTicksRemaining = (uint32_t)qhCp(def, "activeHitboxTicks", 30.0f);
    state.visualReturnTicksRemaining = (uint32_t)qhCp(def, "visualReturnTicks", 60.0f);

    // Compute forward from camera
    state.attackForward = camera.front;
    if (glm::length(state.attackForward) < 0.001f)
        state.attackForward = glm::vec3(0.0f, 1.0f, 0.0f);
    state.attackForward.z = 0.0f;
    if (glm::length(state.attackForward) > 0.001f)
        state.attackForward = glm::normalize(state.attackForward);
    else
        state.attackForward = glm::vec3(0.0f, 1.0f, 0.0f);

    state.active = true;
    state.hitCooldowns.clear();

    // Compute current capsule
    state.currentArmCapsule = computeArmCapsule(owner, def);
    state.hasPreviousCapsule = true;

    // Cut previous sound and play new one
    AudioManager::instance().stopOwner(QUICKHIT_SOUND_OWNER_ID);
    {
        AudioEvent attackSound;
        attackSound.name = def.soundShoot;
        attackSound.category = AudioCategory::Impacts;
        attackSound.world = true;
        attackSound.position = owner.pos;
        attackSound.volume = 1.0f;
        attackSound.pitch = 1.0f;
        attackSound.maxDistance = 40.0f;
        attackSound.ownerId = QUICKHIT_SOUND_OWNER_ID;
        AudioManager::instance().play(attackSound);
    }

    // Set shoot effect timer for pose system
    WeaponRuntime* rt = nullptr;
    auto it = owner.weaponRuntimes.find(def.id);
    if (it != owner.weaponRuntimes.end()) {
        rt = &it->second;
        rt->shootEffectTimer = std::max(0.1f,
            (float)state.activeTicksRemaining / 60.0f);
        rt->customFloats["swordPoseState"] = 3.0f;
    }

    Debug::log(Debug::Category::Weapons,
        "[QUICK HIT] start seq=%u ticks=%u visualReturn=%u dir=(%.2f %.2f %.2f)\n",
        state.attackSequenceId, state.activeTicksRemaining,
        state.visualReturnTicksRemaining,
        state.attackForward.x, state.attackForward.y, state.attackForward.z);
}

// ── Per-frame update ─────────────────────────────────────────

void WeaponQuickHit::update(QuickHitState& state, const WeaponDefinition& def,
                             WeaponRuntime& runtime, Player& owner,
                             NpcSystem& npcs, const Camera& camera,
                             const World& world, float dt)
{
    (void)world;

    if (!state.active) return;

    dt = std::min(dt, 0.05f);

    const float tickDt = 1.0f / 60.0f;
    const uint32_t ticksThisFrame = std::max(1u, (uint32_t)std::round(dt / tickDt));

    // Save previous capsule for swept test
    state.previousArmCapsule = state.currentArmCapsule;

    // Update current capsule from right arm
    state.currentArmCapsule = computeArmCapsule(owner, def);

    // Keep shoot effect timer alive while active
    runtime.shootEffectTimer = std::max(runtime.shootEffectTimer,
        (float)state.activeTicksRemaining / 60.0f);
    runtime.customFloats["swordPoseState"] = 3.0f;

    // Sweep capsule against NPCs every tick within this frame
    if (state.activeTicksRemaining > 0) {
        for (uint32_t t = 0; t < ticksThisFrame && state.activeTicksRemaining > 0; t++) {
            state.activeTicksRemaining--;

            // Sweep collision against NPCs
            for (Npc& npc : npcs.all()) {
                if (npc.body.dead) continue;

                auto cdIt = state.hitCooldowns.find(npc.id);
                if (cdIt != state.hitCooldowns.end() && cdIt->second > 0.0f)
                    continue;

                for (const auto& part : npc.body.physicalBody.parts) {
                    glm::vec3 partCenter = glm::vec3(part.worldTransform[3]);
                    float partRadius = 0.3f;

                    // Test capsule vs sphere
                    glm::vec3 seg = state.currentArmCapsule.b - state.currentArmCapsule.a;
                    float segLen = glm::length(seg);
                    if (segLen < 0.001f) continue;
                    glm::vec3 segDir = seg / segLen;
                    glm::vec3 toTarget = partCenter - state.currentArmCapsule.a;
                    float tProj = glm::clamp(glm::dot(toTarget, segDir), 0.0f, segLen);
                    glm::vec3 closest = state.currentArmCapsule.a + segDir * tProj;
                    float dist = glm::length(closest - partCenter);
                    float sumRadius = state.currentArmCapsule.r + partRadius;

                    if (dist < sumRadius) {
                        // Hit! Compute force-based damage
                        glm::vec3 capsuleVel = (state.currentArmCapsule.b - state.previousArmCapsule.b) /
                                               std::max(tickDt, 0.0001f);
                        float impactSpeed = glm::length(capsuleVel);

                        glm::vec3 contactNormal = dist > 0.001f
                            ? glm::normalize(partCenter - closest)
                            : glm::vec3(0.0f, 0.0f, 1.0f);
                        float directness = std::max(0.0f,
                            glm::dot(glm::normalize(capsuleVel + glm::vec3(0.001f)),
                                     -contactNormal));

                        float rawForce = impactSpeed * directness;
                        float forceDmgScale = qhCp(def, "forceDamageScale", 1.0f);
                        float forceDmgExp = qhCp(def, "forceDamageExponent", 1.35f);
                        float minDmg = qhCp(def, "minDamage", 1.0f);
                        float maxDmg = qhCp(def, "maxDamage", 100.0f);
                        float damage = minDmg + std::pow(rawForce * forceDmgScale, forceDmgExp);
                        damage = std::clamp(damage, minDmg, maxDmg);

                        float forceKbScale = qhCp(def, "forceKnockbackScale", 1.0f);
                        float maxKb = qhCp(def, "maxKnockback", 100.0f);
                        float minKb = qhCp(def, "minKnockback", 0.0f);
                        float knockback = rawForce * forceKbScale;
                        knockback = std::clamp(knockback, minKb, maxKb);

                        glm::vec3 kbDir = glm::length(partCenter - closest) > 0.001f
                            ? glm::normalize(partCenter - closest)
                            : glm::vec3(0.0f, 0.0f, 1.0f);
                        kbDir.z = std::max(kbDir.z, 0.15f);
                        kbDir = glm::normalize(kbDir);

                        npc.body.takeDamage(damage, kbDir, knockback);

                        Debug::log(Debug::Category::Weapons,
                            "[QUICK HIT] hit npc=%u damage=%.1f kb=%.1f speed=%.1f directness=%.2f\n",
                            npc.id, damage, knockback, impactSpeed, directness);

                        WeaponAudio::playHitSound(def, partCenter);

                        state.hitCooldowns[npc.id] = qhCp(def, "damageTickInterval", 0.05f);
                        break;
                    }
                }
            }

            // Decay hit cooldowns
            for (auto it = state.hitCooldowns.begin(); it != state.hitCooldowns.end(); ) {
                it->second -= tickDt;
                if (it->second <= 0.0f)
                    it = state.hitCooldowns.erase(it);
                else
                    ++it;
            }
        }
    }

    // Advance visual return
    if (state.visualReturnTicksRemaining > 0) {
        uint32_t returnTicks = std::min(ticksThisFrame, state.visualReturnTicksRemaining);
        state.visualReturnTicksRemaining -= returnTicks;
    }

    // End attack when both counters are done
    if (state.activeTicksRemaining == 0 && state.visualReturnTicksRemaining == 0) {
        state.active = false;
        state.hasPreviousCapsule = false;
        runtime.shootEffectTimer = 0.0f;
        runtime.customFloats["swordPoseState"] = 0.0f;
        Debug::log(Debug::Category::Weapons, "[QUICK HIT] attack ended\n");
    }
}

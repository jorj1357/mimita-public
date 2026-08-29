// 08 29 2026, 00 00
/* purpose
* Implements SpyKnife client-side state machine: swing animation, swept blade collision, backstab.
* Owns input detection, attack lifecycle, backstab geometry, NPC hit detection, and local damage.
* Stores pending remote player hits for the network layer to send as hit claims.
* Does NOT send network packets directly.
* Does NOT independently simulate knife collision on the server.
*/

#include "weapon-spyknife.h"
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
#include "effects/hit-effects.h"
#include "ui/hitmarker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

static constexpr unsigned int SPYKNIFE_SOUND_OWNER_ID = 0xFFFF0002;

static float skCp(const WeaponDefinition& def, const char* key, float fallback) {
    auto it = def.customParams.find(key);
    return (it != def.customParams.end()) ? it->second : fallback;
}

Capsule WeaponSpyKnife::computeKnifeCapsule(const Player& owner,
                                              const WeaponDefinition& def)
{
    const float bladeLength = skCp(def, "bladeLength", 0.6f);
    const float bladeRadius = skCp(def, "bladeRadius", 0.05f);

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
        glm::vec3 forward = owner.aimDirection;
        if (glm::length(forward) < 0.001f) forward = glm::vec3(0.0f, 1.0f, 0.0f);
        forward = glm::normalize(forward);
        armCenter = owner.pos + glm::vec3(0.0f, 0.0f, 1.2f) + forward * 0.6f;
    }

    glm::vec3 forward = owner.aimDirection;
    if (glm::length(forward) < 0.001f) forward = glm::vec3(0.0f, 1.0f, 0.0f);
    forward = glm::normalize(forward);

    Capsule cap;
    cap.a = armCenter;
    cap.b = armCenter + forward * bladeLength;
    cap.r = bladeRadius;
    return cap;
}

bool WeaponSpyKnife::isBackstabGeometry(const Player& attacker, const Player& victim,
                                          const WeaponDefinition& def)
{
    const float backstabDist = skCp(def, "backstabDistance", 0.5f);
    const float backstabConeDeg = skCp(def, "backstabConeDegrees", 150.0f);

    float dist = glm::length(attacker.pos - victim.pos);
    if (dist > backstabDist)
        return false;

    glm::vec2 victimFwd2(victim.aimDirection.x, victim.aimDirection.y);
    float fwdLen = glm::length(victimFwd2);
    if (fwdLen < 0.001f)
        return false;
    victimFwd2 /= fwdLen;

    glm::vec2 attackerDir2(attacker.pos.x - victim.pos.x, attacker.pos.y - victim.pos.y);
    float dirLen = glm::length(attackerDir2);
    if (dirLen < 0.001f)
        return false;
    attackerDir2 /= dirLen;

    float cosAngle = glm::dot(victimFwd2, attackerDir2);
    float halfConeCos = std::cos(backstabConeDeg * 0.5f * 3.14159265f / 180.0f);

    return cosAngle <= halfConeCos;
}

std::vector<SpyKnifeHitResult> WeaponSpyKnife::collectRemoteHits(SpyKnifeState& state)
{
    std::vector<SpyKnifeHitResult> result;
    result.swap(state.pendingRemoteHits);
    return result;
}

static bool sweptBladeHitPart(const Capsule& prev, const Capsule& curr,
                               const glm::vec3& partCenter, float partRadius)
{
    for (int si = 0; si <= 4; si++) {
        float t = (float)si / 4.0f;
        glm::vec3 prevPt = prev.a + (prev.b - prev.a) * t;
        glm::vec3 currPt = curr.a + (curr.b - curr.a) * t;

        glm::vec3 seg = currPt - prevPt;
        float segLen = glm::length(seg);
        if (segLen < 0.001f) {
            if (glm::length(currPt - partCenter) < curr.r + partRadius)
                return true;
            continue;
        }
        glm::vec3 segDir = seg / segLen;
        glm::vec3 toTarget = partCenter - prevPt;
        float tProj = glm::clamp(glm::dot(toTarget, segDir), 0.0f, segLen);
        glm::vec3 closest = prevPt + segDir * tProj;
        float dist = glm::length(closest - partCenter);
        if (dist < curr.r + partRadius)
            return true;
    }
    return false;
}

void WeaponSpyKnife::startSwing(SpyKnifeState& state, const WeaponDefinition& def,
                                  Player& owner, Camera& camera)
{
    if (state.active) {
        state.previousKnifeCapsule = state.currentKnifeCapsule;
    } else {
        state.previousKnifeCapsule = computeKnifeCapsule(owner, def);
    }

    state.attackSequenceId++;
    if (state.attackSequenceId == 0) state.attackSequenceId = 1;

    state.swingTick = 0;
    state.animState = SpyKnifeAnimState::Swinging;
    state.active = true;
    state.hitCooldowns.clear();
    state.backstabSoundPlayed.clear();
    state.pendingRemoteHits.clear();

    state.currentKnifeCapsule = computeKnifeCapsule(owner, def);
    state.hasPreviousCapsule = true;

    AudioManager::instance().stopOwner(SPYKNIFE_SOUND_OWNER_ID);
    {
        AudioEvent attackSound;
        attackSound.name = def.soundShoot;
        attackSound.category = AudioCategory::Impacts;
        attackSound.world = true;
        attackSound.position = owner.pos;
        attackSound.volume = 1.0f;
        attackSound.pitch = 1.0f;
        attackSound.maxDistance = 40.0f;
        attackSound.ownerId = SPYKNIFE_SOUND_OWNER_ID;
        AudioManager::instance().play(attackSound);
    }

    WeaponRuntime* rt = nullptr;
    auto it = owner.weaponRuntimes.find(def.id);
    if (it != owner.weaponRuntimes.end()) {
        rt = &it->second;
        rt->shootEffectTimer = std::max(0.1f,
            (float)skCp(def, "swingDurationTicks", 120.0f) / 60.0f);
        rt->customFloats["knifeAnimState"] = 2.0f;
    }

    Debug::log(Debug::Category::Weapons,
        "[SPY KNIFE] swing start seq=%u\n",
        state.attackSequenceId);
}

static void applySpyKnifeNpcHit(SpyKnifeState& state, const WeaponDefinition& def,
                                 Player& owner, Npc& npc, bool isBackstab)
{
    float damage = isBackstab
        ? skCp(def, "backstabDamagePerTick", 999.0f)
        : skCp(def, "normalDamagePerTick", 1.0f);
    float kbStrength = isBackstab
        ? skCp(def, "backstabKnockback", 20.0f)
        : skCp(def, "frontstabKnockback", 100.0f);

    if (isBackstab && !state.backstabSoundPlayed[npc.id]) {
        state.backstabSoundPlayed[npc.id] = true;
        AudioEvent bsSound;
        bsSound.name = "spyknifebackstab";
        bsSound.category = AudioCategory::Impacts;
        bsSound.world = true;
        bsSound.position = npc.body.pos;
        bsSound.volume = 1.0f;
        bsSound.pitch = 1.0f;
        bsSound.maxDistance = 50.0f;
        AudioManager::instance().play(bsSound);
    }

    glm::vec3 kbDir = glm::length(npc.body.pos - owner.pos) > 0.001f
        ? glm::normalize(npc.body.pos - owner.pos)
        : glm::vec3(0.0f, 0.0f, 1.0f);
    float vertFrac = isBackstab ? 0.15f : skCp(def, "frontstabVerticalKnockback", 0.3f);
    kbDir.z = std::max(kbDir.z, vertFrac);
    kbDir = glm::normalize(kbDir);

    npc.body.takeDamage(damage, kbDir, kbStrength);

    Debug::log(Debug::Category::Weapons,
        "[SPY KNIFE] hit npc=%u damage=%.1f kb=%.1f backstab=%d\n",
        npc.id, damage, kbStrength, (int)isBackstab);

    WeaponAudio::playHitSound(def, npc.body.pos);
}

void WeaponSpyKnife::update(SpyKnifeState& state, const WeaponDefinition& def,
                              WeaponRuntime& runtime, Player& owner,
                              NpcSystem& npcs, const Camera& camera,
                              const World& world, float dt)
{
    (void)world;

    dt = std::min(dt, 0.05f);
    const float tickDt = 1.0f / 60.0f;
    const uint32_t ticksThisFrame = std::max(1u, (uint32_t)std::round(dt / tickDt));

    const uint32_t swingDurationTicks = (uint32_t)skCp(def, "swingDurationTicks", 120.0f);
    const uint32_t swingForwardTicks = (uint32_t)skCp(def, "swingForwardTicks", 60.0f);

    state.previousKnifeCapsule = state.currentKnifeCapsule;
    state.currentKnifeCapsule = computeKnifeCapsule(owner, def);

    if (state.active) {
        for (uint32_t t = 0; t < ticksThisFrame; t++) {
            state.swingTick++;

            runtime.shootEffectTimer = std::max(runtime.shootEffectTimer,
                (float)(swingDurationTicks - state.swingTick) / 60.0f);

            for (Npc& npc : npcs.all()) {
                if (npc.body.dead) continue;

                auto cdIt = state.hitCooldowns.find(npc.id);
                if (cdIt != state.hitCooldowns.end() && cdIt->second > 0.0f)
                    continue;

                bool hitFound = false;
                for (const auto& part : npc.body.physicalBody.parts) {
                    glm::vec3 partCenter = glm::vec3(part.worldTransform[3]);
                    float partRadius = 0.3f;

                    if (sweptBladeHitPart(state.previousKnifeCapsule,
                                          state.currentKnifeCapsule,
                                          partCenter, partRadius)) {
                        hitFound = true;
                        break;
                    }
                }

                if (!hitFound) continue;

                bool isBs = isBackstabGeometry(owner, npc.body, def);
                applySpyKnifeNpcHit(state, def, owner, npc, isBs);
                state.hitCooldowns[npc.id] = skCp(def, "damageTickInterval", 0.0f);
            }

            for (auto cdIt = state.hitCooldowns.begin(); cdIt != state.hitCooldowns.end(); ) {
                cdIt->second -= tickDt;
                if (cdIt->second <= 0.0f)
                    cdIt = state.hitCooldowns.erase(cdIt);
                else
                    ++cdIt;
            }
        }

        if (state.swingTick >= swingDurationTicks) {
            state.active = false;
            state.hasPreviousCapsule = false;
            runtime.shootEffectTimer = 0.0f;

            bool foundReady = false;
            for (const Npc& npc : npcs.all()) {
                if (npc.body.dead) continue;
                if (isBackstabGeometry(owner, npc.body, def)) {
                    foundReady = true;
                    break;
                }
            }
            state.animState = foundReady ? SpyKnifeAnimState::Ready : SpyKnifeAnimState::Idle;
            runtime.customFloats["knifeAnimState"] = (float)state.animState;
        } else if (state.swingTick >= swingForwardTicks) {
            state.animState = SpyKnifeAnimState::Returning;
            runtime.customFloats["knifeAnimState"] = 3.0f;
        }
    }

    if (!state.active) {
        bool foundReady = false;
        for (const Npc& npc : npcs.all()) {
            if (npc.body.dead) continue;
            if (isBackstabGeometry(owner, npc.body, def)) {
                foundReady = true;
                break;
            }
        }

        SpyKnifeAnimState newState = foundReady ? SpyKnifeAnimState::Ready : SpyKnifeAnimState::Idle;
        if (newState != state.animState) {
            state.animState = newState;
            runtime.customFloats["knifeAnimState"] = (float)newState;
        }
    }
}

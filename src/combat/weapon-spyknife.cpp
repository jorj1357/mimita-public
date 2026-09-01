// 09 01 2026, 00 00
/* purpose
* Implements SpyKnife client-side state machine with swept-OBB blade hitbox.
* Oriented box collision runs at 60Hz tick rate, not per-frame.
* Force-based damage: speed + angle + directness determine damage.
* Box is welded to the knife model orientation via weapon capsule axes.
* Applies remote NPC damage directly on the attacking client for now.
* Does NOT render the hitbox outside debug visualization mode.
*/

#include "weapon-spyknife.h"
#include "weapon-types.h"
#include "weapon-audio.h"
#include "weapon-execution.h"
#include "camera.h"
#include "audio/audio.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "physics/physics-types.h"
#include "effects/hit-effects.h"
#include "ui/hitmarker.h"
#include "combat/death-system.h"
#include "devtools/terminal.h"
#include "network/multiplayer-context.h"

extern MimitaNet::MultiplayerContext* gpMpContext;

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <chrono>

static constexpr unsigned int SPYKNIFE_SOUND_OWNER_ID = 0xFFFF0002;

static float skCp(const WeaponDefinition& def, const char* key, float fallback) {
    auto it = def.customParams.find(key);
    return (it != def.customParams.end()) ? it->second : fallback;
}

static glm::vec3 skCpVec3(const WeaponDefinition& def, const char* prefix, glm::vec3 fallback) {
    char bufX[64], bufY[64], bufZ[64];
    snprintf(bufX, sizeof(bufX), "%sX", prefix);
    snprintf(bufY, sizeof(bufY), "%sY", prefix);
    snprintf(bufZ, sizeof(bufZ), "%sZ", prefix);
    return glm::vec3(
        skCp(def, bufX, fallback.x),
        skCp(def, bufY, fallback.y),
        skCp(def, bufZ, fallback.z)
    );
}

// ── Debug logging ─────────────────────────────────────────────

static FILE* gSpyknifeLogFile = nullptr;

static void openSpyknifeLog()
{
    if (gSpyknifeLogFile) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char dateDir[64];
    snprintf(dateDir, sizeof(dateDir), "logs/%02d-%02d-%04d", st.wMonth, st.wDay, st.wYear);
    CreateDirectoryA("logs", NULL);
    CreateDirectoryA(dateDir, NULL);

    char path[256];
    snprintf(path, sizeof(path), "%s/SpyKnife_log_%02d%02d%02d.txt",
             dateDir, st.wHour, st.wMinute, st.wSecond);
    gSpyknifeLogFile = fopen(path, "w");
    if (gSpyknifeLogFile)
        Debug::warn(Debug::Category::Weapons, "[SPYKNIFE_DBG] Log opened: %s\n", path);
}

static void spyknifeLog(const char* fmt, ...)
{
    openSpyknifeLog();
    Debug::warn(Debug::Category::Weapons, "[SPYKNIFE_DBG] ");
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Debug::warn(Debug::Category::Weapons, "%s\n", buf);
    if (gSpyknifeLogFile) {
        fprintf(gSpyknifeLogFile, "[SPYKNIFE_DBG] %s\n", buf);
        fflush(gSpyknifeLogFile);
    }
}

// ── Blade OBB position and orientation (from weapon model transform) ──

static BladeOBB computeBladeBox(const Player& owner, const WeaponDefinition& def)
{
    BladeOBB box;
    box.halfExtents = skCpVec3(def, "hitboxHalf", glm::vec3(0.5f));

    glm::vec3 offset = skCpVec3(def, "hitboxOffset", glm::vec3(0.0f));

    // Extract the 3 axes directly from the weapon model's world transform.
    // Column 0 = right (local X), Column 1 = up (local Y), Column 2 = forward (local Z)
    glm::mat4 modelMat = owner.weaponModelTransform;
    glm::vec3 right   = glm::normalize(glm::vec3(modelMat[0]));
    glm::vec3 up      = glm::normalize(glm::vec3(modelMat[1]));
    glm::vec3 forward = glm::normalize(glm::vec3(modelMat[2]));

    // Apply local hitbox rotation (euler degrees) on top of the model basis
    float rx = skCp(def, "hitboxRotX", 0.0f);
    float ry = skCp(def, "hitboxRotY", 0.0f);
    float rz = skCp(def, "hitboxRotZ", 0.0f);
    if (std::abs(rx) > 0.001f || std::abs(ry) > 0.001f || std::abs(rz) > 0.001f) {
        glm::mat4 rotMat(1.0f);
        rotMat = glm::rotate(rotMat, glm::radians(rx), glm::vec3(1,0,0));
        rotMat = glm::rotate(rotMat, glm::radians(ry), glm::vec3(0,1,0));
        rotMat = glm::rotate(rotMat, glm::radians(rz), glm::vec3(0,0,1));
        right   = glm::vec3(rotMat * glm::vec4(right, 0.0f));
        up      = glm::vec3(rotMat * glm::vec4(up, 0.0f));
        forward = glm::vec3(rotMat * glm::vec4(forward, 0.0f));
    }

    box.axes[0] = right;
    box.axes[1] = up;
    box.axes[2] = forward;

    // Center at blade tip + offset rotated into world space
    glm::vec3 tip;
    if (owner.weaponCollisionCapsule.r > 0.001f) {
        tip = owner.weaponCollisionCapsule.b;
    } else {
        tip = owner.pos + glm::vec3(0.0f, 0.0f, 1.2f) + owner.aimDirection * 0.8f;
    }
    box.center = tip + offset;

    return box;
}

// ── OBB vs sphere overlap test ──

static bool obbSphereOverlap(const BladeOBB& box, const glm::vec3& sphereCenter, float sphereRadius)
{
    glm::vec3 d = sphereCenter - box.center;
    float localX = glm::dot(d, box.axes[0]);
    float localY = glm::dot(d, box.axes[1]);
    float localZ = glm::dot(d, box.axes[2]);

    float cx = std::clamp(localX, -box.halfExtents.x, box.halfExtents.x);
    float cy = std::clamp(localY, -box.halfExtents.y, box.halfExtents.y);
    float cz = std::clamp(localZ, -box.halfExtents.z, box.halfExtents.z);

    glm::vec3 closest = box.center + box.axes[0] * cx + box.axes[1] * cy + box.axes[2] * cz;
    float dist = glm::length(sphereCenter - closest);
    return dist < sphereRadius;
}

// ── Swept OBB vs sphere overlap (discretized samples along sweep) ──

static bool sweptOBBOverlap(const BladeOBB& prevBox, const BladeOBB& currBox,
                             const glm::vec3& sphereCenter, float sphereRadius,
                             int substeps, glm::vec3& hitPoint, glm::vec3& hitNormal)
{
    for (int s = 0; s < substeps; s++) {
        float t0 = (float)s / substeps;
        float t1 = (float)(s + 1) / substeps;

        BladeOBB sample;
        sample.center = prevBox.center + (currBox.center - prevBox.center) * ((t0 + t1) * 0.5f);
        sample.halfExtents = currBox.halfExtents;
        for (int i = 0; i < 3; i++)
            sample.axes[i] = currBox.axes[i];

        if (obbSphereOverlap(sample, sphereCenter, sphereRadius)) {
            hitPoint = sample.center;
            glm::vec3 diff = sphereCenter - sample.center;
            float dist = glm::length(diff);
            hitNormal = dist > 0.001f ? diff / dist : glm::vec3(0.0f, 0.0f, 1.0f);
            return true;
        }
    }
    return false;
}

// ── Backstab geometry ─────────────────────────────────────────

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
    if (fwdLen < 0.001f) return false;
    victimFwd2 /= fwdLen;

    glm::vec2 attackerDir2(attacker.pos.x - victim.pos.x, attacker.pos.y - victim.pos.y);
    float dirLen = glm::length(attackerDir2);
    if (dirLen < 0.001f) return false;
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

// ── Swing start ───────────────────────────────────────────────

void WeaponSpyKnife::startSwing(SpyKnifeState& state, const WeaponDefinition& def,
                                  Player& owner, Camera& camera)
{
    state.attackSequenceId++;
    if (state.attackSequenceId == 0) state.attackSequenceId = 1;

    state.swingTick = 0;
    state.animState = SpyKnifeAnimState::Swinging;
    state.active = true;
    state.hitCooldowns.clear();
    state.backstabSoundPlayed.clear();
    state.pendingRemoteHits.clear();

    state.previousBladeCapsule = owner.weaponCollisionCapsule;
    state.hasPreviousBladeCapsule = true;

    BladeOBB box = computeBladeBox(owner, def);
    state.prevBladeOBB = box;
    state.hasPrevBladeOBB = true;

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
        rt->customFloats["swordPoseState"] = 1.0f;
    }

    spyknifeLog("SWING_START seq=%u pos=(%.2f,%.2f,%.2f) center=(%.2f,%.2f,%.2f) half=(%.2f,%.2f,%.2f)",
                state.attackSequenceId,
                owner.pos.x, owner.pos.y, owner.pos.z,
                box.center.x, box.center.y, box.center.z,
                box.halfExtents.x, box.halfExtents.y, box.halfExtents.z);
}

// ── Apply remote hit (godball-style) ──────────────────────────

static int applySpyKnifeRemoteHit(SpyKnifeState& state, const WeaponDefinition& def,
                                     Player& owner, uint32_t targetId, Player& target,
                                     bool isBackstab, const glm::vec3& hitPoint)
{
    glm::vec3 bladeDir = glm::length(state.prevBladeOBB.center - hitPoint) > 0.001f
        ? glm::normalize(hitPoint - state.prevBladeOBB.center)
        : owner.aimDirection;
    float bladeSpeed = glm::length(state.prevBladeOBB.center - hitPoint) * 60.0f;
    glm::vec3 toTarget = target.pos - hitPoint;
    float toLen = glm::length(toTarget);

    float directness = 0.0f;
    float angleBonus = 0.0f;
    if (toLen > 0.001f && bladeSpeed > 0.1f) {
        directness = std::max(0.0f, glm::dot(bladeDir, toTarget / toLen));
        angleBonus = directness * skCp(def, "angleDamageFactor", 10.0f);
    }

    float damage, kbForce;

    if (isBackstab) {
        damage = skCp(def, "backstabDamagePerTick", 999.0f);
        kbForce = skCp(def, "backstabKnockback", 20.0f);
    } else {
        float baseDmg = skCp(def, "baseDamage", 15.0f);
        float speedDmg = bladeSpeed * skCp(def, "speedDamageFactor", 20.0f) * 0.01f;
        damage = baseDmg + speedDmg + angleBonus;
        damage = std::clamp(damage, 1.0f, skCp(def, "maxDamage", 100.0f));

        float baseKb = skCp(def, "baseKnockback", 30.0f);
        float speedKb = bladeSpeed * skCp(def, "speedKnockbackFactor", 4.0f);
        float angleKb = angleBonus * skCp(def, "angleKnockbackFactor", 2.0f);
        kbForce = baseKb + speedKb + angleKb;
        kbForce = std::clamp(kbForce, 0.0f, skCp(def, "maxKnockback", 200.0f));
    }

    int roundedDamage = std::max(1, (int)std::round(damage));

    if (isBackstab && !state.backstabSoundPlayed[targetId]) {
        state.backstabSoundPlayed[targetId] = true;
        AudioEvent bsSound;
        bsSound.name = "spyknifebackstab";
        bsSound.category = AudioCategory::Impacts;
        bsSound.world = true;
        bsSound.position = target.pos;
        bsSound.volume = 1.0f;
        bsSound.pitch = 1.0f;
        bsSound.maxDistance = 50.0f;
        AudioManager::instance().play(bsSound);
    }

    glm::vec3 kbDir = toLen > 0.001f ? toTarget / toLen : glm::vec3(0.0f, 0.0f, 1.0f);
    kbDir.z = std::max(kbDir.z, 0.15f);
    kbDir = glm::normalize(kbDir);

    int hpBefore = target.currentHp;

    // Temporary client-authoritative NPC damage: the hit the attacker sees is
    // the damage that is applied. Do not route this through the prediction
    // overlay or send a server claim, because either path can overwrite the
    // local NPC health after the feedback has already been shown.
    target.currentHp = std::max(0, target.currentHp - roundedDamage);
    if (gpMpContext && gpMpContext->connected && gpMpContext->localPlayerId)
        MimitaNet::mpApplyPredictedDamage(*gpMpContext, targetId, roundedDamage, true);
    target.currentHp = std::max(0, target.currentHp);
    target.dead = target.currentHp <= 0;
    Debug::log(Debug::Category::NpcCombat,
        "[SPYKNIFE CLIENT NPC DAMAGE] attacker=%s npc=%u damage=%d hpBefore=%d hpAfter=%d",
        owner.username.c_str(), targetId, roundedDamage, hpBefore, target.currentHp);

    target.externalImpulse += kbDir * kbForce + glm::vec3(0, 0, kbForce * 0.3f);

    if (target.currentHp <= 0) {
        std::string line = owner.username + " killed " + target.username + " with Spy Knife";
        Terminal::instance().addLog(line);
    }

    {
        HitEvent ev;
        ev.position = target.pos + glm::vec3(0, 0, 0.8f);
        ev.normal = kbDir;
        ev.direction = bladeDir;
        ev.hitEntity = true;
        ev.damage = roundedDamage;
        ev.attacker = owner.username;
        ev.victim = target.username;
        ev.weaponSource = "spyknife";
        HitEffects::onHit(ev);
    }

    hitmarker(roundedDamage);

    {
        float severity = std::clamp(damage / 100.0f, 0.0f, 1.0f);
        WeaponAudio::playGodballImpact(target.pos, severity);
    }

    spyknifeLog("HIT id=%u name=%s damage=%d backstab=%d speed=%.1f directness=%.2f hpBefore=%d hpAfter=%d",
                targetId, target.username.c_str(), roundedDamage, (int)isBackstab,
                bladeSpeed, directness, hpBefore, target.currentHp);

    SpyKnifeHitResult hitResult;
    hitResult.targetId = targetId;
    hitResult.isBackstab = isBackstab;
    hitResult.hitPosition = hitPoint;
    hitResult.victimPosition = target.pos;
    state.pendingRemoteHits.push_back(hitResult);

    return roundedDamage;
}

// ── Send hit claim packet to server ───────────────────────────

static void sendSpyKnifeHitClaim(const Player& owner, uint32_t targetId,
                                   bool isBackstab, float damage,
                                   const glm::vec3& hitPos)
{
    if (!gpMpContext || !gpMpContext->connected || !gpMpContext->localPlayerId)
        return;

    MimitaNet::SpyKnifeHitClaimPacket claim{};
    claim.header.type = MimitaNet::PACKET_SPYKNIFE_HIT_CLAIM;
    claim.header.tick = gpMpContext->tick;
    claim.header.playerId = gpMpContext->localPlayerId;
    claim.attackerId = gpMpContext->localPlayerId;
    claim.targetId = targetId;
    claim.isBackstab = isBackstab ? 1 : 0;
    claim.damage = damage;
    claim.hitX = hitPos.x;
    claim.hitY = hitPos.y;
    claim.hitZ = hitPos.z;
    claim.attackerX = owner.pos.x;
    claim.attackerY = owner.pos.y;
    claim.attackerZ = owner.pos.z;
    claim.attackerYaw = 0.0f;
    claim.victimX = hitPos.x;
    claim.victimY = hitPos.y;
    claim.victimZ = hitPos.z;
    claim.attackSerial = (uint16_t)(0);

    MimitaNet::mpSendPacket(*gpMpContext, &claim, sizeof(claim));
    spyknifeLog("CLAIM_SENT targetId=%u backstab=%d damage=%.1f hit=(%.2f,%.2f,%.2f)",
                targetId, (int)isBackstab, damage, hitPos.x, hitPos.y, hitPos.z);
}

// ── Per-tick update (60Hz) ────────────────────────────────────

void WeaponSpyKnife::update(SpyKnifeState& state, const WeaponDefinition& def,
                              WeaponRuntime& runtime, Player& owner,
                              std::unordered_map<uint32_t, Player>* remoteNpcs,
                              const Camera& camera,
                              const World& world, float dt)
{
    (void)world;

    dt = std::min(dt, 0.05f);
    const float tickDt = 1.0f / 60.0f;
    const uint32_t ticksThisFrame = std::max(1u, (uint32_t)std::round(dt / tickDt));

    const uint32_t swingDurationTicks = (uint32_t)skCp(def, "swingDurationTicks", 120.0f);
    const uint32_t swingForwardTicks = (uint32_t)skCp(def, "swingForwardTicks", 60.0f);

    const bool logVerbose = DebugConfig::DEBUG_SPYKNIFE;

    BladeOBB currBox = computeBladeBox(owner, def);
    // Spy Knife is continuously live while equipped. The attack input still
    // starts the animation/sound, but the collision volume is re-armed here
    // so the weapon does not wait for another click after its swing timer.
    if (!state.active) {
        state.active = true;
        state.swingTick = 0;
        state.animState = SpyKnifeAnimState::Swinging;
        state.hitCooldowns.clear();
        state.backstabSoundPlayed.clear();
        state.prevBladeOBB = currBox;
        state.hasPrevBladeOBB = true;
        runtime.shootEffectTimer = (float)swingDurationTicks / 60.0f;
        runtime.customFloats["swordPoseState"] = 1.0f;
    }
    BladeOBB prevBox = state.hasPrevBladeOBB ? state.prevBladeOBB : currBox;

    // ── Swing tick advancement ──
    if (state.active) {
        for (uint32_t t = 0; t < ticksThisFrame; t++) {
            state.swingTick++;
            runtime.shootEffectTimer = std::max(runtime.shootEffectTimer,
                (float)(swingDurationTicks - state.swingTick) / 60.0f);
        }

        if (state.swingTick >= swingDurationTicks) {
            state.active = false;
            state.hasPreviousBladeCapsule = false;
            state.hasPrevBladeOBB = false;
            runtime.shootEffectTimer = 0.0f;
            runtime.customFloats["swordPoseState"] = 0.0f;
            spyknifeLog("SWING_END seq=%u totalTicks=%u", state.attackSequenceId, state.swingTick);
            state.animState = SpyKnifeAnimState::Idle;
        } else if (state.swingTick >= swingForwardTicks) {
            state.animState = SpyKnifeAnimState::Returning;
            runtime.customFloats["swordPoseState"] = 0.0f;
        }
    }

    // ── Remote NPC status log ──
    size_t remoteNpcCount = remoteNpcs ? remoteNpcs->size() : 0;
    spyknifeLog("REMOTE_NPC_STATUS remoteNpcCount=%zu active=%d", remoteNpcCount, (int)state.active);

    // ── Swept OBB collision against remote NPCs ──
    {
        float boxSpeed = glm::length(currBox.center - prevBox.center) * 60.0f;
        spyknifeLog("TICK center=(%.2f,%.2f,%.2f) prevCenter=(%.2f,%.2f,%.2f) speed=%.1f",
                    currBox.center.x, currBox.center.y, currBox.center.z,
                    prevBox.center.x, prevBox.center.y, prevBox.center.z, boxSpeed);

        if (remoteNpcs) {
            const float sweepThreshold = 20.0f;
            const int maxSubsteps = 8;
            float maxDim = std::max({currBox.halfExtents.x, currBox.halfExtents.y, currBox.halfExtents.z});
            const float combinedEstimate = maxDim * 2.0f + 0.5f;
            const float maxStepDist = combinedEstimate * 0.8f;
            const float moveDist = boxSpeed * dt;
            const int substeps = (moveDist > sweepThreshold && maxStepDist > 0.001f)
                ? std::clamp((int)std::ceil(moveDist / maxStepDist), 1, maxSubsteps)
                : 1;

            for (auto& entry : *remoteNpcs) {
                uint32_t npcId = entry.first;
                Player& remote = entry.second;

                if (remote.dead || remote.currentHp <= 0) continue;
                if (remote.physicalBody.parts.empty()) continue;

                auto cdIt = state.hitCooldowns.find(npcId);
                if (cdIt != state.hitCooldowns.end() && cdIt->second > 0.0f) {
                    if (logVerbose)
                        spyknifeLog("  SKIP id=%u name=%s reason=cooldown cd=%.3f",
                                    npcId, remote.username.c_str(), cdIt->second);
                    continue;
                }

                remote.updateModelWorldTransforms();

                if (logVerbose) {
                    spyknifeLog("REMOTE_NPC id=%u name=%s hp=%d pos=(%.2f,%.2f,%.2f) parts=%zu",
                                npcId, remote.username.c_str(), remote.currentHp,
                                remote.pos.x, remote.pos.y, remote.pos.z,
                                remote.physicalBody.parts.size());
                }

                bool hitFound = false;
                glm::vec3 hitPt, hitNm;

                for (const auto& part : remote.physicalBody.parts) {
                    glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                    glm::vec3 worldCenter = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                    glm::vec3 halfSize = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                    float partRadius = glm::length(halfSize) * 1.25f;

                    if (sweptOBBOverlap(prevBox, currBox, worldCenter, partRadius,
                                         substeps, hitPt, hitNm)) {
                        hitFound = true;
                        if (logVerbose)
                            spyknifeLog("  HIT_PART id=%u name=%s", npcId, part.name.c_str());
                        break;
                    }
                    if (hitFound) break;
                }

                if (!hitFound) {
                    if (logVerbose) {
                        float dist = glm::length(remote.pos - currBox.center);
                        spyknifeLog("  MISS id=%u name=%s dist=%.3f", npcId, remote.username.c_str(), dist);
                    }
                    continue;
                }

                bool isBs = isBackstabGeometry(owner, remote, def);
                spyknifeLog("BACKSTAB_CHECK id=%u name=%s backstab=%d",
                            npcId, remote.username.c_str(), (int)isBs);

                int hitDamage = applySpyKnifeRemoteHit(state, def, owner, npcId, remote, isBs, hitPt);
                (void)hitDamage;
                state.hitCooldowns[npcId] = 1.0f / 60.0f;
            }
        }

        // Decay cooldowns
        for (auto cdIt = state.hitCooldowns.begin(); cdIt != state.hitCooldowns.end(); ) {
            cdIt->second -= dt;
            if (cdIt->second <= 0.0f)
                cdIt = state.hitCooldowns.erase(cdIt);
            else
                ++cdIt;
        }
    }

    // Store current OBB for next frame's sweep
    state.prevBladeOBB = currBox;
    state.hasPrevBladeOBB = true;
    state.previousBladeCapsule = owner.weaponCollisionCapsule;
    state.hasPreviousBladeCapsule = true;

    // ── Ready pose detection ──
    if (!state.active && remoteNpcs) {
        bool foundReady = false;
        for (auto& entry : *remoteNpcs) {
            if (entry.second.dead || entry.second.currentHp <= 0) continue;
            if (isBackstabGeometry(owner, entry.second, def)) {
                foundReady = true;
                break;
            }
        }
        SpyKnifeAnimState newState = foundReady ? SpyKnifeAnimState::Ready : SpyKnifeAnimState::Idle;
        if (newState != state.animState) {
            state.animState = newState;
        }
    }

    // ── Debug OBB rendering ──
    if (skCp(def, "hitboxVisible", 0.0f) > 0.5f) {
        float alpha = skCp(def, "hitboxAlpha", 0.5f);

        // Build 8 corners of the oriented box
        glm::vec3 corners[8];
        for (int i = 0; i < 8; i++) {
            glm::vec3 local(
                (i & 1) ? currBox.halfExtents.x : -currBox.halfExtents.x,
                (i & 2) ? currBox.halfExtents.y : -currBox.halfExtents.y,
                (i & 4) ? currBox.halfExtents.z : -currBox.halfExtents.z
            );
            corners[i] = currBox.localToWorld(local);
        }

        // Draw 12 edges of the box
        static const int edges[12][2] = {
            {0,1},{2,3},{4,5},{6,7},
            {0,2},{1,3},{4,6},{5,7},
            {0,4},{1,5},{2,6},{3,7}
        };
        glm::vec4 wireColor(1.0f, 0.2f, 0.2f, 0.9f);
        for (int e = 0; e < 12; e++) {
            DebugVis::drawLine(camera, corners[edges[e][0]], corners[edges[e][1]], wireColor);
        }

        // Draw filled faces with transparency using corners
        glm::vec4 fillColor(1.0f, 0.2f, 0.2f, alpha * 0.3f);
        static const int faceTris[36] = {
            0,2,1, 0,3,2, 4,5,6, 4,6,7,
            0,1,5, 0,5,4, 1,2,6, 1,6,5,
            2,3,7, 2,7,6, 3,0,4, 3,4,7
        };
        for (int fi = 0; fi < 36; fi += 3) {
            DebugVis::drawLine(camera, corners[faceTris[fi]],   corners[faceTris[fi+1]], fillColor);
            DebugVis::drawLine(camera, corners[faceTris[fi+1]], corners[faceTris[fi+2]], fillColor);
            DebugVis::drawLine(camera, corners[faceTris[fi+2]], corners[faceTris[fi]],   fillColor);
        }

        if (logVerbose) {
            spyknifeLog("OBB_RENDER center=(%.2f,%.2f,%.2f) half=(%.2f,%.2f,%.2f) alpha=%.2f",
                        currBox.center.x, currBox.center.y, currBox.center.z,
                        currBox.halfExtents.x, currBox.halfExtents.y, currBox.halfExtents.z, alpha);
        }
    }

    // ── Rate-limited summary ──
    if (DebugConfig::DEBUG_SPYKNIFE) {
        static float summaryTimer = 0.0f;
        summaryTimer += dt;
        if (summaryTimer >= 1.0f) {
            summaryTimer = 0.0f;
            float closestDist = 999.0f;
            if (remoteNpcs) {
                for (auto& entry : *remoteNpcs) {
                    if (entry.second.dead || entry.second.currentHp <= 0) continue;
                    float d = glm::length(entry.second.pos - currBox.center);
                    if (d < closestDist) closestDist = d;
                }
            }
            float swordPose = 0.0f;
            auto spIt = runtime.customFloats.find("swordPoseState");
            if (spIt != runtime.customFloats.end()) swordPose = spIt->second;
            spyknifeLog("SUMMARY active=%d swingTick=%u animState=%d remoteNpcCount=%zu",
                        (int)state.active, state.swingTick, (int)state.animState, remoteNpcCount);
            spyknifeLog("  center=(%.2f,%.2f,%.2f) half=(%.2f,%.2f,%.2f)",
                        currBox.center.x, currBox.center.y, currBox.center.z,
                        currBox.halfExtents.x, currBox.halfExtents.y, currBox.halfExtents.z);
            spyknifeLog("  closestNpcDist=%.3f swordPoseState=%.1f", closestDist, swordPose);
        }
    }
}

// 08 30 2026, 12 00
/* purpose
* Implements SpyKnife client-side state machine with swept-sphere blade hitbox.
* Sphere collision runs at 60Hz tick rate, not per-frame.
* Force-based damage: speed + angle + directness determine damage amount.
* Uses godball-style swept sphere overlap for NPC hit detection.
* Sends SpyKnifeHitClaimPacket to server on remote NPC hits.
* Does NOT render the hitbox sphere (debug visualization only).
*/

#include "weapon-spyknife.h"
#include "weapon-types.h"
#include "weapon-audio.h"
#include "weapon-execution.h"
#include "weapon-registry.h"
#include "weapon-godball.h"
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

// ── Blade tip sphere position (from weapon collision capsule) ──

struct BladeSphere {
    glm::vec3 tip;
    float radius;
};

static BladeSphere computeBladeSphere(const Player& owner, const WeaponDefinition& def)
{
    BladeSphere s;
    s.radius = skCp(def, "bladeSphereRadius", 1.0f);

    glm::vec3 tipOffset = skCpVec3(def, "bladeSphereOffset", glm::vec3(0.0f));

    if (owner.weaponCollisionCapsule.r > 0.001f) {
        s.tip = owner.weaponCollisionCapsule.b;
    } else {
        s.tip = owner.pos + glm::vec3(0.0f, 0.0f, 1.2f) + owner.aimDirection * 0.8f;
    }
    s.tip += tipOffset;
    return s;
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

    BladeSphere bs = computeBladeSphere(owner, def);
    state.prevBladeTip = bs.tip;
    state.hasPrevBladeTip = true;

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

    spyknifeLog("SWING_START seq=%u pos=(%.2f,%.2f,%.2f) tip=(%.2f,%.2f,%.2f) radius=%.2f",
                state.attackSequenceId,
                owner.pos.x, owner.pos.y, owner.pos.z,
                bs.tip.x, bs.tip.y, bs.tip.z, bs.radius);
}

// ── Apply remote hit (godball-style) ──────────────────────────

static int applySpyKnifeRemoteHit(SpyKnifeState& state, const WeaponDefinition& def,
                                     Player& owner, uint32_t targetId, Player& target,
                                     bool isBackstab, const glm::vec3& hitPoint)
{
    glm::vec3 bladeDir = glm::length(state.prevBladeTip - hitPoint) > 0.001f
        ? glm::normalize(hitPoint - state.prevBladeTip)
        : owner.aimDirection;
    float bladeSpeed = glm::length(state.prevBladeTip - hitPoint) * 60.0f;
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

    // Client-side prediction (godball pattern)
    if (gpMpContext && gpMpContext->connected && gpMpContext->localPlayerId) {
        const uint64_t nowMsVal = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        gpMpContext->predictedNpcHitMs[targetId] = nowMsVal;
        gpMpContext->predictedNpcDamage[targetId] = roundedDamage;
        MimitaNet::mpApplyPredictedDamage(*gpMpContext, targetId, roundedDamage, true);
    } else {
        target.currentHp = std::max(0, target.currentHp - roundedDamage);
    }

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

    BladeSphere bs = computeBladeSphere(owner, def);
    glm::vec3 prevTip = state.hasPrevBladeTip ? state.prevBladeTip : bs.tip;

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
            state.hasPrevBladeTip = false;
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

    // ── Swept sphere collision against remote NPCs (godball-style) ──
    {
        spyknifeLog("TICK tip=(%.2f,%.2f,%.2f) prevTip=(%.2f,%.2f,%.2f) radius=%.2f",
                    bs.tip.x, bs.tip.y, bs.tip.z,
                    prevTip.x, prevTip.y, prevTip.z, bs.radius);

        if (remoteNpcs) {
            const float damageRadius = bs.radius;
            glm::vec3 currPos = bs.tip;
            glm::vec3 sweepPrev = prevTip;

            float bladeSpeed = glm::length(currPos - sweepPrev) * 60.0f;
            const float sweepThreshold = 20.0f;
            const int maxSubsteps = 8;
            const float combinedRadius = damageRadius + 0.5f;
            const float maxStepDist = combinedRadius * 0.8f;
            const float moveDist = bladeSpeed * dt;
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

                    for (int s = 0; s < substeps; s++) {
                        float t0 = (float)s / substeps;
                        float t1 = (float)(s + 1) / substeps;
                        glm::vec3 subPrev = sweepPrev + (currPos - sweepPrev) * t0;
                        glm::vec3 subCurr = sweepPrev + (currPos - sweepPrev) * t1;

                        if (WeaponGodball::sweptSphereOverlap(subPrev, subCurr, damageRadius,
                                                              worldCenter, partRadius,
                                                              hitPt, hitNm)) {
                            hitFound = true;
                            if (logVerbose)
                                spyknifeLog("  HIT_PART id=%u name=%s", npcId, part.name.c_str());
                            break;
                        }
                    }
                    if (hitFound) break;
                }

                if (!hitFound) {
                    if (logVerbose) {
                        float dist = glm::length(remote.pos - bs.tip);
                        spyknifeLog("  MISS id=%u name=%s dist=%.3f", npcId, remote.username.c_str(), dist);
                    }
                    continue;
                }

                bool isBs = isBackstabGeometry(owner, remote, def);
                spyknifeLog("BACKSTAB_CHECK id=%u name=%s backstab=%d",
                            npcId, remote.username.c_str(), (int)isBs);

                int hitDamage = applySpyKnifeRemoteHit(state, def, owner, npcId, remote, isBs, hitPt);
                sendSpyKnifeHitClaim(owner, npcId, isBs, (float)hitDamage, hitPt);
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

    // Store current tip for next frame's sweep
    state.prevBladeTip = bs.tip;
    state.hasPrevBladeTip = true;
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

    // ── Debug sphere rendering ──
    if (skCp(def, "hitboxVisible", 0.0f) > 0.5f) {
        float alpha = skCp(def, "hitboxAlpha", 0.5f);

        DebugVis::drawFilledSphere(camera, bs.tip, bs.radius,
                                    {1.0f, 0.2f, 0.2f, alpha});
        DebugVis::drawWireSphere(camera, bs.tip, bs.radius,
                                  {1.0f, 0.2f, 0.2f, 0.9f});

        if (logVerbose) {
            spyknifeLog("SPHERE_RENDER tip=(%.2f,%.2f,%.2f) radius=%.2f alpha=%.2f",
                        bs.tip.x, bs.tip.y, bs.tip.z, bs.radius, alpha);
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
                    float d = glm::length(entry.second.pos - bs.tip);
                    if (d < closestDist) closestDist = d;
                }
            }
            float swordPose = 0.0f;
            auto spIt = runtime.customFloats.find("swordPoseState");
            if (spIt != runtime.customFloats.end()) swordPose = spIt->second;
            spyknifeLog("SUMMARY active=%d swingTick=%u animState=%d remoteNpcCount=%zu",
                        (int)state.active, state.swingTick, (int)state.animState, remoteNpcCount);
            spyknifeLog("  tip=(%.2f,%.2f,%.2f) radius=%.2f",
                        bs.tip.x, bs.tip.y, bs.tip.z, bs.radius);
            spyknifeLog("  closestNpcDist=%.3f swordPoseState=%.1f", closestDist, swordPose);
        }
    }
}

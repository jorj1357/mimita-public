// 08 29 2026, 00 00
/* purpose
* Implements SpyKnife client-side state machine with configurable box hitbox.
* Box collision runs at 60Hz tick rate, not per-frame.
* Force-based damage: speed + angle + directness determine damage amount.
* Uses godball-style client-authoritative damage on remote NPCs.
* Visual box renders at configurable alpha for debugging.
*/

#include "weapon-spyknife.h"
#include "weapon-types.h"
#include "weapon-audio.h"
#include "weapon-execution.h"
#include "weapon-registry.h"
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

static constexpr unsigned int SPYKNIFE_SOUND_OWNER_ID = 0xFFFF0002;

static float skCp(const WeaponDefinition& def, const char* key, float fallback) {
    auto it = def.customParams.find(key);
    return (it != def.customParams.end()) ? it->second : fallback;
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

// ── Box hitbox computation (uses arm orientation) ─────────────

struct BoxHitbox {
    glm::vec3 center;
    glm::vec3 halfSize;
    glm::vec3 rotation;
    glm::quat orientation;
};

static BoxHitbox computeBoxHitbox(const Player& owner, const WeaponDefinition& def)
{
    BoxHitbox box;
    box.halfSize = glm::vec3(
        skCp(def, "hitboxHalfX", 0.15f),
        skCp(def, "hitboxHalfY", 0.05f),
        skCp(def, "hitboxHalfZ", 0.4f)
    );
    glm::vec3 offset(
        skCp(def, "hitboxOffsetX", 0.0f),
        skCp(def, "hitboxOffsetY", 0.0f),
        skCp(def, "hitboxOffsetZ", 0.3f)
    );
    box.rotation = glm::vec3(
        skCp(def, "hitboxRotX", 0.0f),
        skCp(def, "hitboxRotY", 0.0f),
        skCp(def, "hitboxRotZ", 0.0f)
    );

    glm::vec3 armCenter(0.0f);
    glm::quat armRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    bool foundArm = false;
    for (const PhysicalBodyPart& part : owner.physicalBody.parts) {
        if (part.name == "rightArm") {
            armCenter = glm::vec3(part.worldTransform[3]);
            armRot = glm::quat_cast(glm::mat3(part.worldTransform));
            foundArm = true;
            break;
        }
    }
    if (!foundArm) {
        armCenter = owner.pos + glm::vec3(0.0f, 0.0f, 1.2f) + owner.aimDirection * 0.6f;
    }

    float rx = glm::radians(box.rotation.x);
    float ry = glm::radians(box.rotation.y);
    float rz = glm::radians(box.rotation.z);
    glm::mat4 weaponRot = glm::mat4(1.0f);
    weaponRot = glm::rotate(weaponRot, rx, glm::vec3(1, 0, 0));
    weaponRot = glm::rotate(weaponRot, ry, glm::vec3(0, 1, 0));
    weaponRot = glm::rotate(weaponRot, rz, glm::vec3(0, 0, 1));

    box.orientation = armRot * glm::quat_cast(weaponRot);
    glm::vec3 rotatedOffset = box.orientation * offset;
    box.center = armCenter + rotatedOffset;
    return box;
}

// ── Box vs point test ─────────────────────────────────────────

static bool pointInsideBox(const glm::vec3& point, const BoxHitbox& box)
{
    glm::quat invRot = glm::conjugate(box.orientation);
    glm::vec3 local = invRot * (point - box.center);

    return std::abs(local.x) < box.halfSize.x &&
           std::abs(local.y) < box.halfSize.y &&
           std::abs(local.z) < box.halfSize.z;
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

    BoxHitbox box = computeBoxHitbox(owner, def);
    spyknifeLog("SWING_START seq=%u pos=(%.2f,%.2f,%.2f) box=(%.2f,%.2f,%.2f) half=(%.2f,%.2f,%.2f)",
                state.attackSequenceId,
                owner.pos.x, owner.pos.y, owner.pos.z,
                box.center.x, box.center.y, box.center.z,
                box.halfSize.x, box.halfSize.y, box.halfSize.z);
}

// ── Apply remote hit (godball-style) ──────────────────────────

static void applySpyKnifeRemoteHit(SpyKnifeState& state, const WeaponDefinition& def,
                                     Player& owner, uint32_t targetId, Player& target,
                                     bool isBackstab,
                                     const BoxHitbox& box, const BoxHitbox& prevBox)
{
    float boxSpeed = glm::length(box.center - prevBox.center) * 60.0f;
    glm::vec3 boxDir = glm::length(box.center - prevBox.center) > 0.001f
        ? glm::normalize(box.center - prevBox.center)
        : owner.aimDirection;
    glm::vec3 toTarget = target.pos - box.center;
    float toLen = glm::length(toTarget);

    float directness = 0.0f;
    float angleBonus = 0.0f;
    if (toLen > 0.001f && boxSpeed > 0.1f) {
        directness = std::max(0.0f, glm::dot(boxDir, toTarget / toLen));
        angleBonus = directness * skCp(def, "angleDamageFactor", 10.0f);
    }

    float damage, kbForce;

    if (isBackstab) {
        damage = skCp(def, "backstabDamagePerTick", 999.0f);
        kbForce = skCp(def, "backstabKnockback", 20.0f);
    } else {
        float baseDmg = skCp(def, "baseDamage", 15.0f);
        float speedDmg = boxSpeed * skCp(def, "speedDamageFactor", 20.0f) * 0.01f;
        damage = baseDmg + speedDmg + angleBonus;
        damage = std::clamp(damage, 1.0f, skCp(def, "maxDamage", 100.0f));

        float baseKb = skCp(def, "baseKnockback", 30.0f);
        float speedKb = boxSpeed * skCp(def, "speedKnockbackFactor", 4.0f);
        float angleKb = angleBonus * skCp(def, "angleKnockbackFactor", 2.0f);
        kbForce = baseKb + speedKb + angleKb;
        kbForce = std::clamp(kbForce, 0.0f, skCp(def, "maxKnockback", 200.0f));
    }

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

    // Apply damage locally (godball-style)
    int hpBefore = target.currentHp;
    target.currentHp = std::max(0, target.currentHp - (int)std::round(damage));
    target.externalImpulse += kbDir * kbForce + glm::vec3(0, 0, kbForce * 0.3f);

    // Death check (godball-style)
    if (target.currentHp <= 0) {
        std::string line = owner.username + " killed " + target.username + " with Spy Knife";
        Terminal::instance().addLog(line);
    }

    // Hit effects (godball-style)
    {
        HitEvent ev;
        ev.position = target.pos + glm::vec3(0, 0, 0.8f);
        ev.normal = kbDir;
        ev.direction = boxDir;
        ev.hitEntity = true;
        ev.damage = (int)std::round(damage);
        ev.attacker = owner.username;
        ev.victim = target.username;
        ev.weaponSource = "spyknife";
        HitEffects::onHit(ev);
    }

    hitmarker((int)std::round(damage));

    {
        float severity = std::clamp(damage / 100.0f, 0.0f, 1.0f);
        WeaponAudio::playGodballImpact(target.pos, severity);
    }

    spyknifeLog("HIT id=%u name=%s damage=%.1f backstab=%d speed=%.1f directness=%.2f hpBefore=%d hpAfter=%d",
                targetId, target.username.c_str(), damage, (int)isBackstab,
                boxSpeed, directness, hpBefore, target.currentHp);

    // Store pending hit for network layer
    SpyKnifeHitResult hitResult;
    hitResult.targetId = targetId;
    hitResult.isBackstab = isBackstab;
    hitResult.hitPosition = target.pos;
    hitResult.victimPosition = target.pos;
    state.pendingRemoteHits.push_back(hitResult);
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

    BoxHitbox prevBox = computeBoxHitbox(owner, def);

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

    // ── Box collision against remote NPCs (godball-style) ──
    {
        BoxHitbox box = prevBox;

        spyknifeLog("TICK box=(%.2f,%.2f,%.2f) half=(%.2f,%.2f,%.2f) rot=(%.1f,%.1f,%.1f)",
                    box.center.x, box.center.y, box.center.z,
                    box.halfSize.x, box.halfSize.y, box.halfSize.z,
                    box.rotation.x, box.rotation.y, box.rotation.z);

        if (remoteNpcs) {
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
                for (const auto& part : remote.physicalBody.parts) {
                    glm::vec3 partCenter = glm::vec3(part.worldTransform[3]);

                    if (logVerbose) {
                        spyknifeLog("  PART id=%u name=%s center=(%.2f,%.2f,%.2f)",
                                    npcId, part.name.c_str(),
                                    partCenter.x, partCenter.y, partCenter.z);
                    }

                    if (pointInsideBox(partCenter, box)) {
                        hitFound = true;
                        if (logVerbose)
                            spyknifeLog("  HIT_PART id=%u name=%s", npcId, part.name.c_str());
                        break;
                    }
                }

                if (!hitFound) {
                    if (logVerbose) {
                        float dist = glm::length(remote.pos - box.center);
                        spyknifeLog("  MISS id=%u name=%s dist=%.3f", npcId, remote.username.c_str(), dist);
                    }
                    continue;
                }

                bool isBs = isBackstabGeometry(owner, remote, def);
                spyknifeLog("BACKSTAB_CHECK id=%u name=%s backstab=%d",
                            npcId, remote.username.c_str(), (int)isBs);

                applySpyKnifeRemoteHit(state, def, owner, npcId, remote, isBs, box, prevBox);
                state.hitCooldowns[npcId] = skCp(def, "damageTickInterval", 0.166f);
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

    // ── Debug box rendering ──
    if (skCp(def, "hitboxVisible", 0.0f) > 0.5f) {
        BoxHitbox box = prevBox;
        float alpha = skCp(def, "hitboxAlpha", 0.5f);

        DebugVis::drawFilledBox(camera, box.center, box.halfSize,
                                {1.0f, 0.2f, 0.2f, alpha}, box.rotation);
        DebugVis::drawWireBox(camera, box.center, box.halfSize,
                              {1.0f, 0.2f, 0.2f, 0.9f});

        if (logVerbose) {
            spyknifeLog("BOX_RENDER center=(%.2f,%.2f,%.2f) half=(%.2f,%.2f,%.2f) rot=(%.1f,%.1f,%.1f) alpha=%.2f",
                        box.center.x, box.center.y, box.center.z,
                        box.halfSize.x, box.halfSize.y, box.halfSize.z,
                        box.rotation.x, box.rotation.y, box.rotation.z, alpha);
        }
    }

    // ── Rate-limited summary ──
    if (DebugConfig::DEBUG_SPYKNIFE) {
        static float summaryTimer = 0.0f;
        summaryTimer += dt;
        if (summaryTimer >= 1.0f) {
            summaryTimer = 0.0f;
            BoxHitbox box = prevBox;
            float closestDist = 999.0f;
            if (remoteNpcs) {
                for (auto& entry : *remoteNpcs) {
                    if (entry.second.dead || entry.second.currentHp <= 0) continue;
                    float d = glm::length(entry.second.pos - box.center);
                    if (d < closestDist) closestDist = d;
                }
            }
            float swordPose = 0.0f;
            auto spIt = runtime.customFloats.find("swordPoseState");
            if (spIt != runtime.customFloats.end()) swordPose = spIt->second;
            spyknifeLog("SUMMARY active=%d swingTick=%u animState=%d remoteNpcCount=%zu",
                        (int)state.active, state.swingTick, (int)state.animState, remoteNpcCount);
            spyknifeLog("  box=(%.2f,%.2f,%.2f) half=(%.2f,%.2f,%.2f)",
                        box.center.x, box.center.y, box.center.z,
                        box.halfSize.x, box.halfSize.y, box.halfSize.z);
            spyknifeLog("  closestNpcDist=%.3f swordPoseState=%.1f", closestDist, swordPose);
        }
    }
}

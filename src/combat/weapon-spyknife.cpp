// 08 29 2026, 00 00
/* purpose
* Implements SpyKnife client-side state machine: swept blade collision, backstab, sounds.
* Uses the engine's weaponCollisionCapsule for collision (same as swordsword).
* Owns swing lifecycle, per-tick blade collision, backstab cone check, and local NPC damage.
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
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "physics/physics-types.h"
#include "effects/hit-effects.h"
#include "ui/hitmarker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>

static constexpr unsigned int SPYKNIFE_SOUND_OWNER_ID = 0xFFFF0002;

static float skCp(const WeaponDefinition& def, const char* key, float fallback) {
    auto it = def.customParams.find(key);
    return (it != def.customParams.end()) ? it->second : fallback;
}

// ── Debug logging (godball-style) ─────────────────────────────

static FILE* gSpyknifeLogFile = nullptr;

static void openSpyknifeLog()
{
    if (gSpyknifeLogFile) return;
    // Create logs directory if needed
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

// ── Swept blade collision ─────────────────────────────────────

static bool sweptBladeHitPart(const Capsule& prev, const Capsule& curr,
                               const glm::vec3& partCenter, float partRadius,
                               bool logDetails, const char* npcName)
{
    for (int si = 0; si <= 4; si++) {
        float t = (float)si / 4.0f;
        glm::vec3 prevPt = prev.a + (prev.b - prev.a) * t;
        glm::vec3 currPt = curr.a + (curr.b - curr.a) * t;

        glm::vec3 seg = currPt - prevPt;
        float segLen = glm::length(seg);
        if (segLen < 0.001f) {
            float d = glm::length(currPt - partCenter);
            if (d < curr.r + partRadius) {
                if (logDetails)
                    spyknifeLog("  SWEEP hit(sample=%d t=%.2f) dist=%.3f combinedR=%.3f part=%s",
                                si, t, d, curr.r + partRadius, npcName);
                return true;
            }
            continue;
        }
        glm::vec3 segDir = seg / segLen;
        glm::vec3 toTarget = partCenter - prevPt;
        float tProj = glm::clamp(glm::dot(toTarget, segDir), 0.0f, segLen);
        glm::vec3 closest = prevPt + segDir * tProj;
        float dist = glm::length(closest - partCenter);
        if (dist < curr.r + partRadius) {
            if (logDetails)
                spyknifeLog("  SWEEP hit(sample=%d t=%.2f) dist=%.3f combinedR=%.3f part=%s",
                            si, t, dist, curr.r + partRadius, npcName);
            return true;
        }
    }
    return false;
}

// ── Swing start ───────────────────────────────────────────────

void WeaponSpyKnife::startSwing(SpyKnifeState& state, const WeaponDefinition& def,
                                  Player& owner, Camera& camera)
{
    if (!state.hasPreviousBladeCapsule) {
        state.previousBladeCapsule = owner.weaponCollisionCapsule;
        state.hasPreviousBladeCapsule = true;
    }

    state.attackSequenceId++;
    if (state.attackSequenceId == 0) state.attackSequenceId = 1;

    state.swingTick = 0;
    state.animState = SpyKnifeAnimState::Swinging;
    state.active = true;
    state.hitCooldowns.clear();
    state.backstabSoundPlayed.clear();
    state.pendingRemoteHits.clear();

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

    // Always log swing start
    spyknifeLog("SWING_START seq=%u pos=(%.2f,%.2f,%.2f) aim=(%.2f,%.2f,%.2f)",
                state.attackSequenceId,
                owner.pos.x, owner.pos.y, owner.pos.z,
                owner.aimDirection.x, owner.aimDirection.y, owner.aimDirection.z);

    const Capsule& ec = owner.weaponCollisionCapsule;
    bool ecValid = (glm::length(ec.b - ec.a) > 0.001f && ec.r > 0.001f);
    spyknifeLog("  engineCapsule: valid=%d A=(%.2f,%.2f,%.2f) B=(%.2f,%.2f,%.2f) R=%.3f",
                (int)ecValid,
                ec.a.x, ec.a.y, ec.a.z,
                ec.b.x, ec.b.y, ec.b.z,
                ec.r);
    spyknifeLog("  bladeCollisionRadius=%.3f swingDurationTicks=%u swingForwardTicks=%u",
                skCp(def, "bladeCollisionRadius", 0.25f),
                (uint32_t)skCp(def, "swingDurationTicks", 120.0f),
                (uint32_t)skCp(def, "swingForwardTicks", 60.0f));
}

// ── Apply NPC damage ──────────────────────────────────────────

static void applySpyKnifeNpcHit(SpyKnifeState& state, const WeaponDefinition& def,
                                 Player& owner, Npc& npc, bool isBackstab,
                                 uint32_t swingTick, size_t totalHits)
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

    // Always log hits
    spyknifeLog("HIT id=%u name=%s damage=%.1f backstab=%d",
                npc.id, npc.body.username.c_str(), damage, (int)isBackstab);
    spyknifeLog("  myPos=(%.2f,%.2f,%.2f) npcPos=(%.2f,%.2f,%.2f) dist=%.3f",
                owner.pos.x, owner.pos.y, owner.pos.z,
                npc.body.pos.x, npc.body.pos.y, npc.body.pos.z,
                glm::length(npc.body.pos - owner.pos));
    spyknifeLog("  kbDir=(%.2f,%.2f,%.2f) kbStrength=%.1f swingTick=%u totalHits=%zu",
                kbDir.x, kbDir.y, kbDir.z, kbStrength, swingTick, totalHits);

    WeaponAudio::playHitSound(def, npc.body.pos);
}

// ── Per-frame update ──────────────────────────────────────────

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
    const float collisionRadius = skCp(def, "bladeCollisionRadius", 0.25f);

    // Use engine's weaponCollisionCapsule (follows actual weapon model position)
    Capsule currentBlade = owner.weaponCollisionCapsule;
    currentBlade.r = collisionRadius;

    const bool logVerbose = DebugConfig::DEBUG_SPYKNIFE;

    // ── Per-frame capsule state log ──
    if (logVerbose) {
        const Capsule& ec = owner.weaponCollisionCapsule;
        bool ecValid = (glm::length(ec.b - ec.a) > 0.001f && ec.r > 0.001f);
        spyknifeLog("CAPSULE tick=%u active=%d animState=%d",
                    state.swingTick, (int)state.active, (int)state.animState);
        spyknifeLog("  myPos=(%.2f,%.2f,%.2f) myAim=(%.2f,%.2f,%.2f)",
                    owner.pos.x, owner.pos.y, owner.pos.z,
                    owner.aimDirection.x, owner.aimDirection.y, owner.aimDirection.z);
        spyknifeLog("  engineCapsule: valid=%d A=(%.2f,%.2f,%.2f) B=(%.2f,%.2f,%.2f) R=%.3f",
                    (int)ecValid,
                    ec.a.x, ec.a.y, ec.a.z,
                    ec.b.x, ec.b.y, ec.b.z,
                    ec.r);
        spyknifeLog("  prevBlade: A=(%.2f,%.2f,%.2f) B=(%.2f,%.2f,%.2f) R=%.3f",
                    state.previousBladeCapsule.a.x, state.previousBladeCapsule.a.y, state.previousBladeCapsule.a.z,
                    state.previousBladeCapsule.b.x, state.previousBladeCapsule.b.y, state.previousBladeCapsule.b.z,
                    state.previousBladeCapsule.r);
        spyknifeLog("  currentBlade: A=(%.2f,%.2f,%.2f) B=(%.2f,%.2f,%.2f) R=%.3f",
                    currentBlade.a.x, currentBlade.a.y, currentBlade.a.z,
                    currentBlade.b.x, currentBlade.b.y, currentBlade.b.z,
                    currentBlade.r);
        float swordPose = 0.0f;
        auto spIt = runtime.customFloats.find("swordPoseState");
        if (spIt != runtime.customFloats.end()) swordPose = spIt->second;
        spyknifeLog("  swordPoseState=%.1f shootEffectTimer=%.3f npcCount=%zu",
                    swordPose, runtime.shootEffectTimer, npcs.all().size());
    }

    // Always log NPC count (even when 0)
    spyknifeLog("NPC_STATUS npcCount=%zu active=%d", npcs.all().size(), (int)state.active);

    // ── ALWAYS: NPC collision (knife damages on contact, not just during swing) ──
    {
        for (Npc& npc : npcs.all()) {
            if (npc.body.dead) continue;

            auto cdIt = state.hitCooldowns.find(npc.id);
            if (cdIt != state.hitCooldowns.end() && cdIt->second > 0.0f) {
                if (logVerbose)
                    spyknifeLog("  SKIP id=%u name=%s reason=cooldown cd=%.3f",
                                npc.id, npc.body.username.c_str(), cdIt->second);
                continue;
            }

            if (logVerbose) {
                spyknifeLog("NPC_CHECK id=%u name=%s hp=%d dead=%d active=%d",
                            npc.id, npc.body.username.c_str(), npc.body.currentHp, (int)npc.body.dead, (int)state.active);
                spyknifeLog("  npcPos=(%.2f,%.2f,%.2f) myPos=(%.2f,%.2f,%.2f) dist=%.3f",
                            npc.body.pos.x, npc.body.pos.y, npc.body.pos.z,
                            owner.pos.x, owner.pos.y, owner.pos.z,
                            glm::length(npc.body.pos - owner.pos));
                spyknifeLog("  npcPartCount=%zu", npc.body.physicalBody.parts.size());
            }

            bool hitFound = false;
            for (const auto& part : npc.body.physicalBody.parts) {
                glm::vec3 partCenter = glm::vec3(part.worldTransform[3]);
                float partRadius = 0.3f;

                if (logVerbose) {
                    spyknifeLog("  COLLISION part=%s center=(%.2f,%.2f,%.2f) radius=%.3f bladeR=%.3f combinedR=%.3f",
                                part.name.c_str(),
                                partCenter.x, partCenter.y, partCenter.z,
                                partRadius, currentBlade.r, currentBlade.r + partRadius);
                }

                if (sweptBladeHitPart(state.previousBladeCapsule,
                                      currentBlade,
                                      partCenter, partRadius,
                                      logVerbose, part.name.c_str())) {
                    hitFound = true;
                    break;
                }
            }

            if (!hitFound) {
                if (logVerbose) {
                    float dist = glm::length(npc.body.pos - owner.pos);
                    spyknifeLog("  MISS id=%u name=%s dist=%.3f", npc.id, npc.body.username.c_str(), dist);
                }
                continue;
            }

            // Backstab check with logging
            bool isBs = isBackstabGeometry(owner, npc.body, def);
            if (logVerbose) {
                const float backstabDist = skCp(def, "backstabDistance", 0.5f);
                const float backstabConeDeg = skCp(def, "backstabConeDegrees", 150.0f);
                float dist = glm::length(owner.pos - npc.body.pos);

                glm::vec2 victimFwd2(npc.body.aimDirection.x, npc.body.aimDirection.y);
                float fwdLen = glm::length(victimFwd2);
                if (fwdLen > 0.001f) victimFwd2 /= fwdLen;

                glm::vec2 attackerDir2(owner.pos.x - npc.body.pos.x, owner.pos.y - npc.body.pos.y);
                float dirLen = glm::length(attackerDir2);
                if (dirLen > 0.001f) attackerDir2 /= dirLen;

                float cosAngle = glm::dot(victimFwd2, attackerDir2);
                float halfConeCos = std::cos(backstabConeDeg * 0.5f * 3.14159265f / 180.0f);

                spyknifeLog("BACKSTAB_CHECK id=%u name=%s", npc.id, npc.body.username.c_str());
                spyknifeLog("  dist=%.3f backstabDist=%.3f", dist, backstabDist);
                spyknifeLog("  victimFwd=(%.2f,%.2f) attackerDir=(%.2f,%.2f)",
                            victimFwd2.x, victimFwd2.y, attackerDir2.x, attackerDir2.y);
                spyknifeLog("  cosAngle=%.3f halfConeCos=%.3f result=%d",
                            cosAngle, halfConeCos, (int)isBs);
            }

            applySpyKnifeNpcHit(state, def, owner, npc, isBs,
                                state.swingTick, 0);
            state.hitCooldowns[npc.id] = skCp(def, "damageTickInterval", 0.0f);
        }

        // Decay hit cooldowns
        for (auto cdIt = state.hitCooldowns.begin(); cdIt != state.hitCooldowns.end(); ) {
            cdIt->second -= dt;
            if (cdIt->second <= 0.0f)
                cdIt = state.hitCooldowns.erase(cdIt);
            else
                ++cdIt;
        }
    }

    // ── Swing animation (only when actively swinging) ──
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

            bool foundReady = false;
            for (const Npc& npc : npcs.all()) {
                if (npc.body.dead) continue;
                if (isBackstabGeometry(owner, npc.body, def)) {
                    foundReady = true;
                    break;
                }
            }
            state.animState = foundReady ? SpyKnifeAnimState::Ready : SpyKnifeAnimState::Idle;
        } else if (state.swingTick >= swingForwardTicks) {
            state.animState = SpyKnifeAnimState::Returning;
            runtime.customFloats["swordPoseState"] = 0.0f;
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
        }
    }

    // Save for next frame's swept test
    state.previousBladeCapsule = currentBlade;
    state.hasPreviousBladeCapsule = true;

    // Debug capsule rendering (always on when DEBUG_SPYKNIFE, or via config)
    bool showCapsule = DebugConfig::DEBUG_SPYKNIFE || (skCp(def, "showCollisionCapsule", 0.0f) > 0.5f);
    if (showCapsule) {
        float alpha = DebugConfig::DEBUG_SPYKNIFE ? 0.8f : skCp(def, "collisionAlpha", 0.5f);
        if (state.active) {
            DebugVis::drawWeaponCapsuleWire(camera, currentBlade, {1.0f, 0.2f, 0.2f, alpha});
            if (logVerbose)
                spyknifeLog("CAPSULE_RENDER active alpha=%.2f A=(%.2f,%.2f,%.2f) B=(%.2f,%.2f,%.2f) R=%.3f",
                            alpha,
                            currentBlade.a.x, currentBlade.a.y, currentBlade.a.z,
                            currentBlade.b.x, currentBlade.b.y, currentBlade.b.z,
                            currentBlade.r);
        }
        if (state.animState == SpyKnifeAnimState::Ready) {
            DebugVis::drawWeaponCapsuleWire(camera, currentBlade, {1.0f, 0.8f, 0.0f, alpha});
            if (logVerbose)
                spyknifeLog("CAPSULE_RENDER ready alpha=%.2f A=(%.2f,%.2f,%.2f) B=(%.2f,%.2f,%.2f) R=%.3f",
                            alpha,
                            currentBlade.a.x, currentBlade.a.y, currentBlade.a.z,
                            currentBlade.b.x, currentBlade.b.y, currentBlade.b.z,
                            currentBlade.r);
        }
    }

    // Rate-limited summary (once per second, always on)
    if (DebugConfig::DEBUG_SPYKNIFE) {
        static float summaryTimer = 0.0f;
        summaryTimer += dt;
        if (summaryTimer >= 1.0f) {
            summaryTimer = 0.0f;
            const Capsule& ec = owner.weaponCollisionCapsule;
            bool ecValid = (glm::length(ec.b - ec.a) > 0.001f && ec.r > 0.001f);
            float closestDist = 999.0f;
            for (const Npc& npc : npcs.all()) {
                if (npc.body.dead) continue;
                float d = glm::length(npc.body.pos - owner.pos);
                if (d < closestDist) closestDist = d;
            }
            float swordPose = 0.0f;
            auto spIt = runtime.customFloats.find("swordPoseState");
            if (spIt != runtime.customFloats.end()) swordPose = spIt->second;
            spyknifeLog("SUMMARY active=%d swingTick=%u animState=%d",
                        (int)state.active, state.swingTick, (int)state.animState);
            spyknifeLog("  capsuleValid=%d A=(%.2f,%.2f,%.2f) B=(%.2f,%.2f,%.2f) R=%.3f",
                        (int)ecValid,
                        ec.a.x, ec.a.y, ec.a.z,
                        ec.b.x, ec.b.y, ec.b.z,
                        ec.r);
            spyknifeLog("  npcCount=%zu closestNpcDist=%.3f swordPoseState=%.1f",
                        npcs.all().size(), closestDist, swordPose);
        }
    }
}

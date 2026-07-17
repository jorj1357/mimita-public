#include "network/server.h"
#include "network/network-weapons.h"
#include "combat/weapon-registry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace MimitaNet {

static float swordCp(const WeaponDefinition* def, const char* key, float fallback) {
    if (!def) return fallback;
    auto it = def->customParams.find(key);
    return (it != def->customParams.end()) ? it->second : fallback;
}

// Swept sphere vs sphere (same algorithm as weapon-swordsword.cpp)
static bool sweptSphereOverlap(
    glm::vec3 prevCenter, glm::vec3 currCenter, float radius,
    glm::vec3 targetCenter, float targetRadius,
    float& outDist)
{
    glm::vec3 seg = currCenter - prevCenter;
    float segLen = glm::length(seg);
    if (segLen < 0.0001f) {
        float d = glm::length(currCenter - targetCenter);
        if (d < radius + targetRadius) {
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
    if (d < radius + targetRadius) {
        outDist = d;
        return true;
    }
    return false;
}

// Compute a simple blade capsule from server player state.
// Grip near the player's hand, tip extends in aim direction.
static void computeBladeCapsule(const ServerPlayer& attacker,
                                 glm::vec3& outGrip, glm::vec3& outTip, float& outRadius)
{
    const float bladeLength = 4.0f;
    const float bladeRadius = 0.35f;

    glm::vec3 forward = glm::length(attacker.input.camForward) > 0.001f
        ? glm::normalize(attacker.input.camForward)
        : glm::vec3(1.0f, 0.0f, 0.0f);

    glm::vec3 handOffset(0.0f, 0.0f, 0.8f);
    glm::vec3 grip = attacker.pos + handOffset - forward * 0.5f;
    glm::vec3 tip = grip + forward * bladeLength;

    outGrip = grip;
    outTip = tip;
    outRadius = bladeRadius;
}

static float computeHitDamage(const WeaponDefinition* def, float swordSpeed,
                                const glm::vec3& bladeDir, const glm::vec3& toTarget,
                                bool isLunge, float& outKnockback)
{
    float baseDamage = isLunge
        ? swordCp(def, "lungeBaseDamage", 18.0f)
        : swordCp(def, "slashBaseDamage", 10.0f);
    float speedFactor = isLunge
        ? swordCp(def, "lungeSpeedDamageFactor", 28.0f)
        : swordCp(def, "slashSpeedDamageFactor", 18.0f);
    float maxCap = isLunge
        ? swordCp(def, "lungeMaxDamage", 999999.0f)
        : swordCp(def, "slashMaxDamage", 999999.0f);
    float baseKb = isLunge
        ? swordCp(def, "lungeBaseKnockback", 50.0f)
        : swordCp(def, "slashBaseKnockback", 25.0f);
    float speedKbFactor = isLunge
        ? swordCp(def, "lungeSpeedKnockbackFactor", 6.0f)
        : swordCp(def, "slashSpeedKnockbackFactor", 3.0f);
    float maxKb = isLunge
        ? swordCp(def, "lungeMaxKnockback", 120.0f)
        : swordCp(def, "slashMaxKnockback", 60.0f);
    float forceMul = isLunge
        ? swordCp(def, "lungeForceMultiplier", 1.1f)
        : swordCp(def, "slashForceMultiplier", 0.5f);

    float speedContrib = swordSpeed * speedFactor * 0.01f;
    float totalDmg = (baseDamage + speedContrib) * forceMul;

    float kb = (baseKb + swordSpeed * speedKbFactor) * forceMul;

    float globalDmgMul = swordCp(def, "globalDamageMultiplier", 1.0f);
    float globalKbMul = swordCp(def, "globalKnockbackMultiplier", 1.0f);

    totalDmg = std::clamp(totalDmg * globalDmgMul, 0.0f, maxCap * globalDmgMul);
    outKnockback = std::clamp(kb * globalKbMul, 0.0f, maxKb * globalKbMul);

    return totalDmg;
}

// ── Attack-start command handler ─────────────────────────────────────
// The client no longer sends "I hit player X for Y damage."
// Instead it sends "I started a sword attack (slash/lunge) with serial S."
// The server simulates the full attack and determines hits authoritatively.
void handleMeleeHitRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           uint32_t tick, uint64_t& totalPacketsOut)
{
    if (bytes < (int)sizeof(MeleeHitRequestPacket))
        return;
    const MeleeHitRequestPacket* request =
        reinterpret_cast<const MeleeHitRequestPacket*>(buffer);

    auto attackerIt = players.find(request->header.playerId);
    const bool ownsAttacker =
        attackerIt != players.end() &&
        sameAddress(attackerIt->second.addr, from);
    if (!ownsAttacker)
    {
        printf("%s [SWORD ATTACK REJECT] attackerId=%u reason=sender-address-mismatch\n",
               serverTimestamp(), request->header.playerId);
        return;
    }

    ServerPlayer& attacker = attackerIt->second;

    // ── Validate attack type ──
    bool validType = (request->attackType == 1 || request->attackType == 2);

    const uint8_t equippedWeapon = networkWeaponTypeForSlot(attacker.equippedSlot);
    const bool stateValid =
        !attacker.dead &&
        request->weapon == NETWORK_WEAPON_SWORDSWORD &&
        equippedWeapon == NETWORK_WEAPON_SWORDSWORD &&
        validType;
    const bool serialValid =
        request->attackSerial != 0 &&
        (attacker.lastMeleeAttackSerial == 0 ||
         (int32_t)(request->attackSerial - attacker.lastMeleeAttackSerial) > 0);
    const bool cooldownValid = attacker.meleeCooldownTimer <= 0.0f;
    const bool accepted = stateValid && serialValid && cooldownValid;

    printf("%s [SWORD ATTACK] attackerId=%u attackSerial=%u attackType=%u "
           "accepted=%d reason=%s\n",
           serverTimestamp(), attacker.id, request->attackSerial,
           request->attackType, (int)accepted,
           accepted ? "started" :
           !stateValid ? "invalid-state-or-weapon" :
           !serialValid ? "duplicate-or-stale-serial" :
           "cooldown");

    if (!accepted)
        return;

    attacker.lastMeleeAttackSerial = request->attackSerial;

    // Broadcast attack-started event to ALL players (for remote animation)
    {
        MeleeHitEventPacket animEvent{};
        animEvent.header.type = PACKET_MELEE_HIT_EVENT;
        animEvent.header.tick = tick;
        animEvent.header.playerId = attacker.id;
        animEvent.attackSerial = request->attackSerial;
        animEvent.attackerPlayerId = attacker.id;
        animEvent.targetPlayerId = 0;        // 0 = no hit, animation only
        animEvent.damage = 0;
        animEvent.weapon = NETWORK_WEAPON_SWORDSWORD;
        animEvent.attackType = request->attackType;
        animEvent.killed = 0;
        animEvent.damageConfirmed = 0;
        for (const auto& pe : players)
        {
            if (pe.second.transport)
                pe.second.transport->send(&animEvent, sizeof(animEvent));
            else
                sendto(sock, (const char*)&animEvent, sizeof(animEvent), 0,
                       (sockaddr*)&pe.second.addr, sizeof(pe.second.addr));
            ++totalPacketsOut;
        }
        printf("%s [SWORD ATTACK ANIM] attackerId=%u serial=%u type=%u broadcast to %zu players\n",
               serverTimestamp(), attacker.id, request->attackSerial,
               request->attackType, players.size());
    }

    // ── Start server-side sword attack state ──
    const WeaponDefinition* def = WeaponRegistry::instance().get("swordsword");
    float slashWindup  = swordCp(def, "slashWindupTime", 0.08f);
    float slashActive  = swordCp(def, "slashActiveTime", 0.15f);
    float slashRecover = swordCp(def, "slashRecoverTime", 0.10f);
    float lungeWindup  = swordCp(def, "lungeWindupTime", 0.10f);
    float lungeActive  = swordCp(def, "lungeActiveTime", 0.20f);
    float lungeRecover = swordCp(def, "lungeRecoverTime", 0.12f);

    SwordswordState& ss = attacker.swordswordState;
    ss = SwordswordState{};
    if (request->attackType == 1) {
        ss.state = SwordswordState::AttackState::SlashWindup;
        ss.stateTimer = 0.0f;
        ss.animTimer = 0.0f;
        float totalTime = slashWindup + slashActive + slashRecover;
        attacker.meleeCooldownTimer = totalTime + 0.05f;
        printf("%s [SWORD ATTACK START] attackerId=%u serial=%u type=slash "
               "windup=%.2f active=%.2f recover=%.2f\n",
               serverTimestamp(), attacker.id, request->attackSerial,
               slashWindup, slashActive, slashRecover);
    } else if (request->attackType == 2) {
        ss.state = SwordswordState::AttackState::LungeWindup;
        ss.stateTimer = 0.0f;
        ss.animTimer = 0.0f;
        float totalTime = lungeWindup + lungeActive + lungeRecover;
        attacker.meleeCooldownTimer = totalTime + 0.05f;
        printf("%s [SWORD ATTACK START] attackerId=%u serial=%u type=lunge "
               "windup=%.2f active=%.2f recover=%.2f\n",
               serverTimestamp(), attacker.id, request->attackSerial,
               lungeWindup, lungeActive, lungeRecover);
    }
}

// ── Per-tick sword combat simulation ─────────────────────────────────
// Called every server tick. Advances active sword states, performs blade
// collision against all other players, and broadcasts hit events.
void tickServerSwordCombat(SOCKET sock,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           const HeadlessWorld& world,
                           float dt, uint32_t tick, uint64_t& totalPacketsOut)
{
    (void)world;
    const WeaponDefinition* def = WeaponRegistry::instance().get("swordsword");
    if (!def) return;

    float slashWindup  = swordCp(def, "slashWindupTime", 0.08f);
    float slashActive  = swordCp(def, "slashActiveTime", 0.15f);
    float slashRecover = swordCp(def, "slashRecoverTime", 0.10f);
    float lungeWindup  = swordCp(def, "lungeWindupTime", 0.10f);
    float lungeActive  = swordCp(def, "lungeActiveTime", 0.20f);
    float lungeRecover = swordCp(def, "lungeRecoverTime", 0.12f);
    float tickInterval = swordCp(def, "damageTickInterval", 0.05f);

    for (auto& kv : players)
    {
        ServerPlayer& attacker = kv.second;
        if (attacker.dead) {
            if (attacker.swordswordState.state != SwordswordState::AttackState::Idle) {
                printf("%s [SWORD ATTACK END] attackerId=%u reason=dead\n",
                       serverTimestamp(), attacker.id);
                attacker.swordswordState = SwordswordState{};
            }
            continue;
        }

        if (attacker.meleeCooldownTimer > 0.0f)
            attacker.meleeCooldownTimer -= dt;

        SwordswordState& ss = attacker.swordswordState;
        if (ss.state == SwordswordState::AttackState::Idle)
            continue;

        // Save previous blade for swept test
        glm::vec3 prevGrip, prevTip;
        float prevRadius;
        computeBladeCapsule(attacker, prevGrip, prevTip, prevRadius);

        // ── Advance state machine ──
        enum class Phase { Windup, Active, Recover, Idle };
        Phase nextPhase = Phase::Windup;

        if (ss.state == SwordswordState::AttackState::SlashWindup) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / slashWindup;
            if (ss.stateTimer >= slashWindup) {
                ss.state = SwordswordState::AttackState::SlashActive;
                ss.stateTimer = 0.0f;
                printf("%s [SWORD PHASE] attackerId=%u phase=slash-active\n",
                       serverTimestamp(), attacker.id);
            }
        } else if (ss.state == SwordswordState::AttackState::SlashActive) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / slashActive;
            if (ss.stateTimer >= slashActive) {
                ss.state = SwordswordState::AttackState::SlashRecover;
                ss.stateTimer = 0.0f;
                printf("%s [SWORD PHASE] attackerId=%u phase=slash-recover\n",
                       serverTimestamp(), attacker.id);
            }
        } else if (ss.state == SwordswordState::AttackState::SlashRecover) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / slashRecover;
            if (ss.stateTimer >= slashRecover) {
                ss.state = SwordswordState::AttackState::Idle;
                attacker.meleeCooldownTimer = 0.0f;
                continue;
            }
        } else if (ss.state == SwordswordState::AttackState::LungeWindup) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / lungeWindup;
            if (ss.stateTimer >= lungeWindup) {
                ss.state = SwordswordState::AttackState::LungeActive;
                ss.stateTimer = 0.0f;
                printf("%s [SWORD PHASE] attackerId=%u phase=lunge-active\n",
                       serverTimestamp(), attacker.id);
            }
        } else if (ss.state == SwordswordState::AttackState::LungeActive) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / lungeActive;
            if (ss.stateTimer >= lungeActive) {
                ss.state = SwordswordState::AttackState::LungeRecover;
                ss.stateTimer = 0.0f;
                printf("%s [SWORD PHASE] attackerId=%u phase=lunge-recover\n",
                       serverTimestamp(), attacker.id);
            }
        } else if (ss.state == SwordswordState::AttackState::LungeRecover) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / lungeRecover;
            if (ss.stateTimer >= lungeRecover) {
                ss.state = SwordswordState::AttackState::Idle;
                attacker.meleeCooldownTimer = 0.0f;
                continue;
            }
        }

        // Only check collision during active phases
        bool isActive = (ss.state == SwordswordState::AttackState::SlashActive ||
                         ss.state == SwordswordState::AttackState::LungeActive);
        if (!isActive)
            continue;

        // ── Compute current blade capsule ──
        glm::vec3 curGrip, curTip;
        float curRadius;
        computeBladeCapsule(attacker, curGrip, curTip, curRadius);

        glm::vec3 bladeDir = curTip - curGrip;
        float bladeLen = glm::length(bladeDir);
        if (bladeLen < 0.01f) continue;
        bladeDir /= bladeLen;

        bool isLunge = (ss.state == SwordswordState::AttackState::LungeActive);

        // ── Sweep blade against all other players ──
        for (auto& targetKv : players)
        {
            ServerPlayer& target = targetKv.second;
            if (target.id == attacker.id || target.dead || target.health <= 0)
                continue;

            // Per-target hit cooldown
            auto cdIt = ss.hitCooldowns.find(target.id);
            if (cdIt != ss.hitCooldowns.end() && cdIt->second > 0.0f)
                continue;

            // Model target as a sphere
            float targetRadius = 1.5f;

            // Swept blade test: sample at 3 points along blade
            bool hit = false;
            glm::vec3 hitPoint;
            for (float t = 0.0f; t <= 1.0f; t += 0.5f) {
                glm::vec3 prevPos = prevGrip + (prevTip - prevGrip) * t;
                glm::vec3 curPos = curGrip + (curTip - curGrip) * t;
                float d;
                if (sweptSphereOverlap(prevPos, curPos, curRadius,
                                       target.pos, targetRadius, d)) {
                    hit = true;
                    hitPoint = curPos;
                    break;
                }
            }

            if (!hit) continue;

            // Compute sword speed from capsule movement
            glm::vec3 prevCenter = (prevGrip + prevTip) * 0.5f;
            glm::vec3 curCenter = (curGrip + curTip) * 0.5f;
            float swordSpeed = glm::length(curCenter - prevCenter) / std::max(dt, 0.0001f);

            // Lunge force spike
            if (isLunge) {
                float spikeMul = swordCp(def, "lungeForceSpikeMultiplier", 5.0f);
                float spikeCenter = swordCp(def, "lungeForceSpikeCenter", 0.5f);
                float spikeWidth = swordCp(def, "lungeForceSpikeWidth", 0.15f);
                float distFromCenter = std::fabs(ss.animTimer - spikeCenter);
                float spikeFactor = 1.0f;
                if (distFromCenter < spikeWidth) {
                    float st = 1.0f - (distFromCenter / spikeWidth);
                    spikeFactor = 1.0f + (spikeMul - 1.0f) * st;
                }
                swordSpeed *= spikeFactor;
            }

            glm::vec3 toTarget = target.pos - curGrip;
            float knockback = 0.0f;
            float damage = computeHitDamage(def, swordSpeed, bladeDir, toTarget,
                                             isLunge, knockback);
            if (damage <= 0.0f) continue;

            glm::vec3 kbDir = glm::normalize(target.pos - curGrip);
            kbDir.z = std::max(kbDir.z, 0.15f);
            kbDir = glm::normalize(kbDir);

            float kbH = swordCp(def, "knockbackHorizontalMultiplier", 1.0f);
            float kbV = swordCp(def, "knockbackVerticalMultiplier", 1.0f);
            glm::vec3 kbVec(kbDir.x * knockback * kbH,
                            kbDir.y * knockback * kbH,
                            kbDir.z * knockback * kbV);

            // Apply server-authoritative damage
            ServerDamageResult dmgResult = applyServerDamage(
                players, target, attacker.id, (int)damage, kbVec,
                ServerDamageSource::Melee);

            ss.hitCooldowns[target.id] = tickInterval;

            printf("%s [SWORD HIT] attackerId=%u targetId=%u "
                   "attackSerial=%u type=%s damage=%d healthAfter=%d "
                   "knockback=(%.1f,%.1f,%.1f) killed=%d\n",
                   serverTimestamp(), attacker.id, target.id,
                   attacker.lastMeleeAttackSerial,
                   isLunge ? "lunge" : "slash",
                   (int)damage, dmgResult.healthAfter,
                   kbVec.x, kbVec.y, kbVec.z, (int)dmgResult.killed);

            // Broadcast hit event
            MeleeHitEventPacket event{};
            event.header.type = PACKET_MELEE_HIT_EVENT;
            event.header.tick = tick;
            event.header.playerId = attacker.id;
            event.attackSerial = attacker.lastMeleeAttackSerial;
            event.attackerPlayerId = attacker.id;
            event.targetPlayerId = target.id;
            event.damage = (int)damage;
            event.targetHealth = dmgResult.healthAfter;
            event.weapon = NETWORK_WEAPON_SWORDSWORD;
            event.attackType = isLunge ? 2 : 1;
            event.killed = dmgResult.killed ? 1 : 0;
            event.damageConfirmed = dmgResult.applied ? 1 : 0;
            event.hitX = hitPoint.x; event.hitY = hitPoint.y; event.hitZ = hitPoint.z;
            event.normalX = kbDir.x; event.normalY = kbDir.y; event.normalZ = kbDir.z;
            event.knockX = kbVec.x; event.knockY = kbVec.y; event.knockZ = kbVec.z;

            for (const auto& pe : players)
            {
                if (pe.second.transport)
                    pe.second.transport->send(&event, sizeof(event));
                else
                    sendto(sock, (const char*)&event, sizeof(event), 0,
                           (sockaddr*)&pe.second.addr,
                           sizeof(pe.second.addr));
                ++totalPacketsOut;
            }
        }

        // Decay hit cooldowns
        for (auto it = ss.hitCooldowns.begin(); it != ss.hitCooldowns.end(); ) {
            it->second -= dt;
            if (it->second <= 0.0f) it = ss.hitCooldowns.erase(it); else ++it;
        }
    }
}

} // namespace MimitaNet

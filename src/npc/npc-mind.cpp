// 09 11 2026
/* purpose
* Implements temporary NPC runtime emotion, bounded memory, event hooks, and
* the effective combat/decision helpers fed into the existing math.
* Emotion updates are O(1) and dt-based; memory is a fixed set of fields.
* Does NOT own simulation, combat, navigation, or configuration loading.
*/

#include "npc/npc-mind.h"

#include <algorithm>
#include <cmath>

#include "npc/npc.h"
#include "npc/npc-combat-log.h"
#include "world/world.h"
#include "physics/movement/physics-collision.h"

namespace {

// Shared engine defaults for values the behavior profile does not override.
constexpr float kDamageStressSensitivity = 0.8f;
constexpr float kDamageConfidenceDrop = 0.3f;
constexpr float kKillConfidenceGain = 0.15f;
constexpr float kKillStressRelief = 0.1f;
constexpr float kAllyDeathFear = 0.25f;
constexpr float kAllyDeathStress = 0.20f;
constexpr float kAllyDeathPanic = 0.15f;
constexpr float kEnemyDeathConfidence = 0.05f;

constexpr float kDefaultPanicDecay = 0.7f;
constexpr float kDefaultStressDecay = 0.4f;
constexpr float kFearDecayPerSecond = 0.15f;
constexpr float kConfidenceDecayPerSecond = 0.20f;

constexpr float kStressAimPenalty = 6.0f;       // degrees per unit stress
constexpr float kFearAttackPenalty = 0.5f;
constexpr float kPanicRetreatWeight = 0.5f;
constexpr float kPanicStickinessPenalty = 0.6f;
constexpr float kPanicReactionPenalty = 0.20f;
constexpr float kStressReactionPenalty = 0.10f;

constexpr float kPerceiveRange = 25.0f;

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

float approach(float value, float target, float rate, float dt)
{
    if (value > target) return std::max(target, value - rate * dt);
    return std::min(target, value + rate * dt);
}

} // anonymous namespace

void npcMindReset(Npc& npc)
{
    npc.emotion = NpcRuntimeEmotion{};
    const float baseFear = npc.behavior.active ? npc.behavior.baseFear : 0.0f;
    const float baseConfidence = npc.behavior.active ? npc.behavior.baseConfidence : 0.5f;
    npc.emotion.fear = clamp01(baseFear);
    npc.emotion.confidence = clamp01(baseConfidence);
    npc.memory = NpcMemory{};
}

void npcMindUpdate(Npc& npc, float dt)
{
    const float baseFear = clamp01(npc.behavior.active ? npc.behavior.baseFear : 0.0f);
    const float baseConfidence = clamp01(npc.behavior.active ? npc.behavior.baseConfidence : 0.5f);
    const float panicDecay = npc.behavior.active ? std::max(0.0f, npc.behavior.panicDecayPerSecond)
                                                 : kDefaultPanicDecay;
    const float stressDecay = npc.behavior.active ? std::max(0.0f, npc.behavior.stressDecayPerSecond)
                                                  : kDefaultStressDecay;

    npc.emotion.panic = clamp01(approach(npc.emotion.panic, 0.0f, panicDecay, dt));
    npc.emotion.stress = clamp01(approach(npc.emotion.stress, 0.0f, stressDecay, dt));
    npc.emotion.fear = clamp01(approach(npc.emotion.fear, baseFear, kFearDecayPerSecond, dt));
    npc.emotion.confidence =
        clamp01(approach(npc.emotion.confidence, baseConfidence, kConfidenceDecayPerSecond, dt));

    npc.memory.lastAttackerAge += dt;
    npc.memory.recentDangerAge += dt;
    npc.memory.lastWitnessedDeathAge += dt;
}

void npcMindOnDamaged(Npc& npc, uint32_t attackerId, const glm::vec3& attackerPos,
                      int damage, int maxHealth)
{
    if (damage <= 0)
        return;

    const float frac = maxHealth > 0
        ? clamp01((float)damage / (float)maxHealth)
        : 0.2f;
    const float panicSens = npc.behavior.active ? npc.behavior.damagePanicSensitivity : 1.0f;
    const float fearSens = npc.behavior.active ? npc.behavior.damageFearSensitivity : 1.0f;

    npc.emotion.panic = clamp01(npc.emotion.panic + frac * std::max(0.0f, panicSens));
    npc.emotion.fear = clamp01(npc.emotion.fear + frac * std::max(0.0f, fearSens));
    npc.emotion.stress = clamp01(npc.emotion.stress + frac * kDamageStressSensitivity);
    npc.emotion.confidence = clamp01(npc.emotion.confidence - frac * kDamageConfidenceDrop);

    npc.memory.lastAttackerId = attackerId;
    npc.memory.lastAttackerPos = attackerPos;
    npc.memory.lastAttackerAge = 0.0f;
    npc.memory.recentDangerPos = attackerPos;
    npc.memory.recentDangerAge = 0.0f;

    npcLog("[NPC EMOTION] actor=%u event=damage dmg=%d panic=%.2f fear=%.2f stress=%.2f confidence=%.2f attacker=%u",
           npc.id, damage, npc.emotion.panic, npc.emotion.fear,
           npc.emotion.stress, npc.emotion.confidence, attackerId);
    npcLog("[NPC MEMORY] actor=%u attacker=%u pos=(%.1f,%.1f,%.1f)",
           npc.id, attackerId, attackerPos.x, attackerPos.y, attackerPos.z);
}

void npcMindOnKill(Npc& npc)
{
    npc.emotion.confidence = clamp01(npc.emotion.confidence + kKillConfidenceGain);
    npc.emotion.panic = clamp01(npc.emotion.panic - kKillStressRelief);
    npc.emotion.stress = clamp01(npc.emotion.stress - kKillStressRelief);
    npcLog("[NPC EMOTION] actor=%u event=kill panic=%.2f fear=%.2f stress=%.2f confidence=%.2f",
           npc.id, npc.emotion.panic, npc.emotion.fear,
           npc.emotion.stress, npc.emotion.confidence);
}

void npcMindOnWitnessedDeath(Npc& npc, uint32_t deadActorId, const glm::vec3& pos, bool ally)
{
    npc.memory.lastWitnessedDeathActorId = deadActorId;
    npc.memory.lastWitnessedDeathPos = pos;
    npc.memory.lastWitnessedDeathAge = 0.0f;

    if (ally) {
        npc.emotion.fear = clamp01(npc.emotion.fear + kAllyDeathFear);
        npc.emotion.stress = clamp01(npc.emotion.stress + kAllyDeathStress);
        npc.emotion.panic = clamp01(npc.emotion.panic + kAllyDeathPanic);
        npcLog("[NPC EMOTION] actor=%u event=ally_death dead=%u fear=%.2f stress=%.2f panic=%.2f",
               npc.id, deadActorId, npc.emotion.fear, npc.emotion.stress, npc.emotion.panic);
    } else {
        npc.emotion.confidence = clamp01(npc.emotion.confidence + kEnemyDeathConfidence);
        npcLog("[NPC EMOTION] actor=%u event=enemy_death dead=%u confidence=%.2f",
               npc.id, deadActorId, npc.emotion.confidence);
    }
}

bool npcMindCanPerceive(const Npc& npc, const World& world, const glm::vec3& pos)
{
    const glm::vec3 eye = npc.body.pos + glm::vec3(0.0f, 0.0f, 0.8f);
    const glm::vec3 target = pos + glm::vec3(0.0f, 0.0f, 0.8f);
    glm::vec3 dir = target - eye;
    const float dist = glm::length(dir);
    if (dist < 0.001f)
        return true;
    if (dist > kPerceiveRange)
        return false;
    dir /= dist;
    float hit = dist;
    if (rayTraverseGridCells(world, eye, dir, dist, hit, nullptr))
        return hit > dist - 0.5f;  // a hit at the endpoint is the target's surface
    return true;
}

float npcMindAimErrorBonus(const Npc& npc)
{
    const float panicPenalty = npc.behavior.active ? npc.behavior.panicAimPenalty : 0.0f;
    return npc.emotion.panic * std::max(0.0f, panicPenalty)
         + npc.emotion.stress * kStressAimPenalty;
}

float npcMindEffectiveAggression(const Npc& npc, float baseAggression)
{
    const float confWeight = npc.behavior.active ? npc.behavior.confidenceAttackWeight : 0.0f;
    return clamp01(baseAggression
                 + npc.emotion.confidence * std::max(0.0f, confWeight)
                 - npc.emotion.fear * kFearAttackPenalty);
}

float npcMindRetreatBonus(const Npc& npc)
{
    const float fearWeight = npc.behavior.active ? npc.behavior.fearRetreatWeight : 0.0f;
    return npc.emotion.fear * std::max(0.0f, fearWeight)
         + npc.emotion.panic * kPanicRetreatWeight;
}

float npcMindStickiness(const Npc& npc)
{
    const float base = npc.behavior.targetStickiness;
    return std::max(0.0f, base - npc.emotion.panic * kPanicStickinessPenalty);
}

float npcMindReactionDelay(const Npc& npc)
{
    return std::max(0.0f, npc.behavior.reactionDelay
                 + npc.emotion.panic * kPanicReactionPenalty
                 + npc.emotion.stress * kStressReactionPenalty);
}

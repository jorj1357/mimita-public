// 09 11 2026
/* purpose
* Temporary per-NPC runtime emotion and small bounded combat/social memory.
* Emotion is changing state caused by match events; the behavior profile is the
* stable personality baseline. Memory is a few recent facts, not an event log.
* Provides event hooks and effective combat/target helpers that feed the
* existing combat, state-scoring, and target-selection math.
* Does NOT own navigation, physics, combat, or the behavior profile config.
* Does NOT replicate state; it is server-side AI state only.
*/
#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "npc/npc-behavior.h"

class Npc;
struct World;

// Bounded 0..1 runtime state for one life.
struct NpcRuntimeEmotion
{
    float panic = 0.0f;
    float fear = 0.0f;
    float confidence = 0.5f;
    float stress = 0.0f;
};

// Small bounded recent history (ages in seconds; large age = expired).
// lastSeenTarget memory reuses NpcStateMachine::lastKnownTarget/lastKnownAge.
struct NpcMemory
{
    uint32_t lastAttackerId = 0;
    glm::vec3 lastAttackerPos{0.0f};
    float lastAttackerAge = 1e9f;

    glm::vec3 recentDangerPos{0.0f};
    float recentDangerAge = 1e9f;

    uint32_t lastWitnessedDeathActorId = 0;
    glm::vec3 lastWitnessedDeathPos{0.0f};
    float lastWitnessedDeathAge = 1e9f;
};

// Initial state for a new life, derived from the resolved behavior profile.
void npcMindReset(Npc& npc);
// Time-based decay toward personality baseline; also ages memory. O(1).
void npcMindUpdate(Npc& npc, float dt);

// Event hooks (call from the authoritative damage/kill owners).
void npcMindOnDamaged(Npc& npc, uint32_t attackerId, const glm::vec3& attackerPos,
                      int damage, int maxHealth);
void npcMindOnKill(Npc& npc);
// `ally` = dead actor shared the witness's team.
void npcMindOnWitnessedDeath(Npc& npc, uint32_t deadActorId, const glm::vec3& pos,
                             bool ally);
// Returns true when the NPC could perceive `pos` (range + line of sight).
bool npcMindCanPerceive(const Npc& npc, const World& world, const glm::vec3& pos);

// Effective values fed into existing combat/decision math.
float npcMindAimErrorBonus(const Npc& npc);          // degrees, additive
float npcMindEffectiveAggression(const Npc& npc, float baseAggression);
float npcMindRetreatBonus(const Npc& npc);
float npcMindStickiness(const Npc& npc);             // base stickiness - panic penalty
float npcMindReactionDelay(const Npc& npc);          // base delay + panic/stress penalty

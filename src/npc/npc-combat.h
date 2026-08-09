#pragma once

#include <glm/glm.hpp>

struct World;
class Player;
class Npc;
class NpcSystem;
class Camera;

namespace NpcCombat {

// Effective-unlimited firing range: NPCs never idle purely because a target is
// far away. The weapon hitscan/projectile still has its own range, so damage
// only lands within reach while the NPC always attempts to fire.
constexpr float kNpcFiringRangeCap = 999999.0f;
inline bool npcFiringRangeBlocked(float targetDistance)
{
    return targetDistance > kNpcFiringRangeCap;
}

// Single source of truth for the NPC eye/muzzle height above the body origin.
// LOS rays, hitscan origins, and broadcast fire origins all use this offset so
// "the NPC fires from where it looks" stays consistent everywhere.
inline glm::vec3 npcMuzzleOffset()
{
    return glm::vec3(0.0f, 0.0f, 0.8f);
}

bool tryFire(Npc& npc, const World& world, Player& player, float dt);

glm::vec3 aimAtTarget(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos);

float aimErrorDegrees(float difficulty);
float maxAngularErrorForAccuracy(float acc);

bool rayCapsule(const glm::vec3& origin, const glm::vec3& dir,
                const glm::vec3& a, const glm::vec3& b, float radius,
                float& outDist, glm::vec3& outNormal);

// Update NPC-launched projectiles (rockets, grenades, etc.)
void updateNpcProjectiles(const World& world, NpcSystem& npcSystem,
                          const Camera& camera, float dt);

} // namespace NpcCombat

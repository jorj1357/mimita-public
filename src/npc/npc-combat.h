#pragma once

#include <glm/glm.hpp>

struct World;
class Player;
class Npc;

extern bool gNpcForceHit;
extern float gNpcMaxInaccuracyDegrees;

namespace NpcCombat {

bool tryFire(Npc& npc, const World& world, Player& player, float dt);

glm::vec3 aimAtTarget(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos, glm::vec3 targetVel);

float aimErrorDegrees(float difficulty);

bool rayCapsule(const glm::vec3& origin, const glm::vec3& dir,
                const glm::vec3& a, const glm::vec3& b, float radius,
                float& outDist, glm::vec3& outNormal);

} // namespace NpcCombat

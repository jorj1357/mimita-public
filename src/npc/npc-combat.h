#pragma once

#include <glm/glm.hpp>

struct World;
class Player;
class Npc;
class NpcSystem;
class Camera;

extern bool gNpcForceHit;
extern float gNpcAimAccuracy;

namespace NpcCombat {

bool tryFire(Npc& npc, const World& world, Player& player, float dt);

glm::vec3 aimAtTarget(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos, glm::vec3 targetVel);

float aimErrorDegrees(float difficulty);
float maxAngularErrorForAccuracy(float acc);

bool rayCapsule(const glm::vec3& origin, const glm::vec3& dir,
                const glm::vec3& a, const glm::vec3& b, float radius,
                float& outDist, glm::vec3& outNormal);

// Update NPC-launched projectiles (rockets, grenades, etc.)
void updateNpcProjectiles(const World& world, NpcSystem& npcSystem,
                          const Camera& camera, float dt);

} // namespace NpcCombat

#pragma once

#include <glm/glm.hpp>

struct World;
class Npc;

namespace NpcNavigation {

glm::vec3 wallAvoidDirection(const Npc& npc, glm::vec3 desiredDir, const World& world);

bool isStuck(const Npc& npc);

glm::vec3 unstuckDirection(const Npc& npc, unsigned int& rng, const World& world);

// Returns true if there is a climbable wall in front of the NPC.
// If so, outWallNormal is set to the wall direction the NPC should face.
bool isClimbableWall(const Npc& npc, glm::vec3 moveDir, const World& world, glm::vec3& outWallNormal);

// Returns true if there's an obstacle (wall) in the movement direction within checkDist.
bool obstacleInDirection(const Npc& npc, glm::vec3 dir, float checkDist, const World& world);

} // namespace NpcNavigation

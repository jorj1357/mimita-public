#pragma once

#include <vector>
#include <glm/glm.hpp>

struct World;
class Npc;

namespace NpcNavigation {

glm::vec3 wallAvoidDirection(const Npc& npc, glm::vec3 desiredDir, const World& world);

glm::vec3 wallAvoidDirection(const Npc& npc, glm::vec3 desiredDir, const World& world, const std::vector<int>& candidates);

bool isStuck(const Npc& npc);

glm::vec3 unstuckDirection(const Npc& npc, unsigned int& rng, const World& world);

glm::vec3 unstuckDirection(const Npc& npc, unsigned int& rng, const World& world, const std::vector<int>& candidates);

// Returns true if there is a climbable wall in front of the NPC.
// If so, outWallNormal is set to the wall direction the NPC should face.
bool isClimbableWall(const Npc& npc, glm::vec3 moveDir, const World& world, glm::vec3& outWallNormal);

bool isClimbableWall(const Npc& npc, glm::vec3 moveDir, const World& world, glm::vec3& outWallNormal, const std::vector<int>& candidates);

// Returns true if there's an obstacle (wall) in the movement direction within checkDist.
bool obstacleInDirection(const Npc& npc, glm::vec3 dir, float checkDist, const World& world);

bool obstacleInDirection(const Npc& npc, glm::vec3 dir, float checkDist, const World& world, const std::vector<int>& candidates);

// Returns a direction toward nearby cover (a position where LOS to threatPos is blocked).
// Returns zero vector if no nearby cover found.
glm::vec3 findCoverDirection(const Npc& npc, glm::vec3 threatPos, const World& world);

// Height of the highest solid floor triangle below `pos` (same x/y, casting
// down up to searchDist, sampling within a horizontal radius). Returns the
// floor's world z, or -1e6f if no floor is found within searchDist. Used by
// the server to pin NPCs onto the ground when the headless collision world
// lets them fall through the floor.
float groundHeightAt(const World& world, const glm::vec3& pos, float searchDist, float radius);

} // namespace NpcNavigation

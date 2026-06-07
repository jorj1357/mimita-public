#pragma once

#include <glm/glm.hpp>

struct World;
class Npc;

namespace NpcNavigation {

// Check if moving in the given direction would hit a wall.
// Returns adjusted direction that slides along walls.
glm::vec3 wallAvoidDirection(const Npc& npc, glm::vec3 desiredDir, const World& world);

// Returns true if the NPC is stuck (trying to move but not making progress).
bool isStuck(const Npc& npc);

// Returns a random direction away from nearby walls for un-sticking.
glm::vec3 unstuckDirection(const Npc& npc, unsigned int& rng, const World& world);

} // namespace NpcNavigation

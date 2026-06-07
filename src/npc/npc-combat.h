#pragma once

#include <glm/glm.hpp>

struct World;
class Player;
class Npc;

namespace NpcCombat {

// Try to fire at the player. Returns true if a shot was fired.
bool tryFire(Npc& npc, const World& world, Player& player, float dt);

// Compute aim direction toward predicted target with difficulty-based error.
glm::vec3 aimAtTarget(const Npc& npc, glm::vec3 npcPos, glm::vec3 targetPos, glm::vec3 targetVel);

} // namespace NpcCombat

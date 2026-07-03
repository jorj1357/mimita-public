#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct WeaponDefinition;
struct WeaponRuntime;
class Player;
class Camera;
class NpcSystem;
struct World;
class Npc;

namespace WeaponGrenadeLauncher {

void fire(const WeaponDefinition& def, WeaponRuntime& runtime,
           Player& owner, const glm::vec3& muzzlePos, const glm::vec3& aimDir);

void update(const WeaponDefinition& def, WeaponRuntime& runtime,
            Player& owner, NpcSystem& npcs, const World& world,
            Camera& camera, float dt);

} // namespace WeaponGrenadeLauncher

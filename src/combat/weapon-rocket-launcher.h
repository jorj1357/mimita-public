#pragma once

#include <glm/glm.hpp>
#include <vector>

class Camera;
class Player;
class NpcSystem;
struct World;
struct WeaponDefinition;
struct WeaponRuntime;

struct RocketLauncherState {
    struct Rocket {
        glm::vec3 position{0.0f};
        glm::vec3 prevPosition{0.0f};
        glm::vec3 velocity{0.0f};
        glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
        float lifetime = 5.0f;
        float distanceTraveled = 0.0f;
        bool exploded = false;
        uint32_t ownerId = 0;
        float spawnTime = 0.0f;
        float lastInAirSoundTime = 0.0f;
        float smokeAccumulator = 0.0f;
    };
    std::vector<Rocket> activeRockets;
    float gameTime = 0.0f;
};

namespace WeaponRocketLauncher {

void fire(
    RocketLauncherState& state,
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& owner,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir);

void update(
    RocketLauncherState& state,
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& owner,
    NpcSystem& npcs,
    const World& world,
    Camera& camera,
    float dt);

void clear(RocketLauncherState& state);

} // namespace WeaponRocketLauncher

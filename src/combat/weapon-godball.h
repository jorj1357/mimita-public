#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

class Camera;
class Player;
class NpcSystem;
struct World;
struct WeaponDefinition;
struct WeaponRuntime;

struct GodballPhysics {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    float radius = 0.5f;
    float mass = 0.1f;
    bool active = false;

    float ropeLength = 2.5f;
    float ropeStiffness = 50.0f;
    float ropeDamping = 2.0f;
    float linearDamping = 0.005f;

    // Hand tracking for energy transfer
    glm::vec3 prevHandPos{0.0f};
    bool hasPrevHandPos = false;

    // Debug info
    float ropeTension = 0.0f;
    float constraintDist = 0.0f;
    float handedEnergy = 0.0f;
    float radialVel = 0.0f;
    float tangentialSpeed = 0.0f;
};

namespace WeaponGodball {

void spawnBall(GodballPhysics& phys, const WeaponDefinition& def, const Player& owner);
void despawnBall(GodballPhysics& phys);

void updatePhysics(
    GodballPhysics& phys,
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& owner,
    const Camera& camera,
    float dt
);

void checkOverlaps(
    GodballPhysics& phys,
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& owner,
    NpcSystem& npcs,
    const Camera& camera,
    float dt
);

float computeDamage(
    const GodballPhysics& phys,
    const WeaponDefinition& def,
    const Player& owner,
    const Player& target,
    const glm::vec3& overlapPoint
);

void render(
    const Camera& camera,
    const GodballPhysics& phys,
    const glm::vec3& handPos
);

void renderDebug(
    const Camera& camera,
    const GodballPhysics& phys,
    const WeaponRuntime& runtime,
    const glm::vec3& handPos
);

glm::vec3 getHandPosition(const Player& player);

} // namespace WeaponGodball

#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>

class Camera;
class Player;
class NpcSystem;
struct World;
struct WeaponDefinition;
struct WeaponRuntime;

struct GodballPhysics {
    glm::vec3 position{0.0f};
    glm::vec3 prevPosition{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 angularVelocity{0.0f};
    float radius = 0.5f;
    float mass = 0.1f;
    bool active = false;

    float ropeLength = 2.5f;
    float ropeStiffness = 50.0f;
    float ropeDamping = 2.0f;
    float linearDamping = 0.005f;

    glm::vec3 prevHandPos{0.0f};
    bool hasPrevHandPos = false;

    bool lastFrameHit = false;
    glm::vec3 lastHitNormal{0.0f};

    float ropeTension = 0.0f;
    float constraintDist = 0.0f;
    float handedEnergy = 0.0f;
    float radialVel = 0.0f;
    float tangentialSpeed = 0.0f;

    // === Debug state (populated every frame in checkOverlaps) ===

    // Per-NPC collision attempt results
    struct NpcCollisionDebug {
        uint32_t npcId = 0;
        bool overlapCheck = false;
        bool sweptHit = false;
        bool rejected = false;
        std::string rejectReason;
        float distanceToTarget = 0.0f;
        float overlapAmount = 0.0f;
        float angleDot = 0.0f;
        float ballSpeed = 0.0f;
        float computedDamage = 0.0f;
        bool cooldownActive = false;
        glm::vec3 npcPos{0.0f};
        glm::vec3 hitPoint{0.0f};
        glm::vec3 hitNormal{0.0f};
        glm::vec3 sweepClosest{0.0f};
    };
    std::vector<NpcCollisionDebug> npcCollisions;

    // Impact event (for flash/stop rendering)
    struct ImpactEvent {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f};
        float damage = 0.0f;
        float age = 0.0f;
        float velocity = 0.0f;
    };
    std::vector<ImpactEvent> impactEvents;

    // Hitstop timer (for godball_hitstop_debug)
    float hitstopTimer = 0.0f;
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

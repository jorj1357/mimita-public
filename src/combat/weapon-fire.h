#pragma once

#include <unordered_map>

#include "weapon-types.h"
#include "physics/physics-types.h"

class Camera;
class Player;
class Npc;
class NpcSystem;
struct World;

struct DamageContext;
struct WeaponRuntime;

namespace WeaponFire {

struct AimTarget {
    glm::vec3 worldPoint;
    float cameraDistance;
};

AimTarget computeAimTarget(
    const Camera& camera,
    const World& world,
    NpcSystem& npcs,
    const std::unordered_map<uint32_t, Player>* remotePlayers
);

RevolverShotResult tryFireHitscan(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    const Camera& camera,
    Player& shooter,
    NpcSystem& npcs,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir,
    const std::unordered_map<uint32_t, Player>* remotePlayers = nullptr
);

void fireMultiPellet(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    const Camera& camera,
    Player& shooter,
    NpcSystem& npcs,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir,
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    RevolverShotResult& outResult
);

void applyRecoil(
    Player& shooter,
    const WeaponDefinition& def,
    const glm::vec3& shotDirection,
    float& inOutRecoil,
    float dt
);

glm::vec3 computeSpreadDirection(
    const glm::vec3& baseDir,
    float spreadDegrees,
    unsigned int& rngState
);

int applyDamageToEntity(
    const DamageContext& ctx,
    Npc& victim,
    const WeaponDefinition& def,
    Player& shooter,
    NpcSystem& npcs,
    const glm::vec3& muzzlePos,
    const glm::vec3& shotDirection
);

bool rayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                 const CollisionTriangle& tri, float& distance);

bool rayAabb(const glm::vec3& origin, const glm::vec3& direction,
             const glm::vec3& mn, const glm::vec3& mx,
             float& distance, glm::vec3& normal);

} // namespace WeaponFire

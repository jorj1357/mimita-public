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

enum class AimHitKind {
    None,
    World,
    Npc,
    RemotePlayer
};

const char* aimHitKindName(AimHitKind kind);

struct AimTarget {
    glm::vec3 worldPoint;
    float cameraDistance;
    AimHitKind hitKind = AimHitKind::None;
};

struct AimSolution {
    glm::vec3 origin;
    glm::vec3 aimPoint;
    glm::vec3 direction;
    float cameraDistance;
    const char* modeName;
    AimHitKind cameraHitKind = AimHitKind::None;
    bool usesCameraTarget = false;
};

AimTarget computeAimTarget(
    const Camera& camera,
    const World& world,
    NpcSystem& npcs,
    const std::unordered_map<uint32_t, Player>* remotePlayers
);

AimSolution computeAim(
    const Camera& camera,
    const World& world,
    NpcSystem& npcs,
    const glm::vec3& muzzlePos,
    const std::unordered_map<uint32_t, Player>* remotePlayers
);

void logAimDebug(const char* label, const Camera& camera, const AimSolution& aim);

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

// Direction-based fire: NPCs provide aim direction directly instead of using a camera.
// Uses the same hit detection, damage, blood, debris, knockback pipeline.
// Scans world triangles + the target player (if provided).
RevolverShotResult tryFireHitscanDir(
    const WeaponDefinition& def,
    WeaponRuntime& runtime,
    Player& shooter,
    const World& world,
    const glm::vec3& muzzlePos,
    const glm::vec3& aimDir,
    const Player* targetPlayer = nullptr
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

struct BeamCollisionResult {
    float nearest;
    bool hitWorld;
    glm::vec3 worldNormal;
    Npc* victim;
    std::string hitPart;
    glm::vec3 hitNormal;
    float localHeight;
    uint32_t remoteTargetId;
    const Player* remoteVictim;

    // Exact surface contact point (on the triangle/entity surface)
    glm::vec3 hitPosition{0.0f};
    // Center of the swept sphere at first contact
    glm::vec3 sweepCenterPosition{0.0f};
};

BeamCollisionResult collideBeam(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    float beamThickness,
    const World& world,
    NpcSystem* npcs,
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    const Player* targetPlayer
);

bool rayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                 const CollisionTriangle& tri, float& distance);

bool rayAabb(const glm::vec3& origin, const glm::vec3& direction,
             const glm::vec3& mn, const glm::vec3& mx,
             float& distance, glm::vec3& normal);

void processNpcHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    Npc& victim,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& shotDirection,
    float nearest,
    Player& shooter,
    NpcSystem& npcs,
    const glm::vec3& muzzlePos,
    const glm::vec3& muzzleDir);

void processRemotePlayerHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& shotDirection,
    float nearest,
    Player& shooter,
    uint32_t remoteTargetId,
    const Player* remoteVictim);

void processPlayerHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& shotDirection,
    float nearest,
    Player& shooter,
    Player* targetPlayer);

void processWorldHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const glm::vec3& hitEnd,
    const glm::vec3& worldNormal,
    const glm::vec3& shotDirection,
    const std::string& shooterName);

float computeFalloffDamage(
    const WeaponDefinition& def,
    const std::string& hitPart,
    float nearest,
    int& outDamage);

void processMultiPelletNpcHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    Npc& victim,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& pelletDir,
    float pelletNearest,
    Player& shooter,
    NpcSystem& npcs,
    const glm::vec3& muzzlePos,
    float& accumulatedDamage,
    bool& anyHitEntity,
    uint32_t& lastTargetId,
    glm::vec3& accumulatedKnockback,
    float& nearestPelletDist,
    glm::vec3& lastPelletEnd,
    glm::vec3& lastHitNormal);

void processMultiPelletRemoteHit(
    RevolverShotResult& result,
    const WeaponDefinition& def,
    const std::string& hitPart,
    const glm::vec3& hitNormal,
    const glm::vec3& hitEnd,
    const glm::vec3& pelletDir,
    float pelletNearest,
    Player& shooter,
    uint32_t pelletRemoteTargetId,
    float& accumulatedDamage,
    bool& anyHitEntity,
    uint32_t& lastTargetId,
    glm::vec3& accumulatedKnockback,
    float& nearestPelletDist,
    glm::vec3& lastPelletEnd,
    glm::vec3& lastHitNormal,
    const std::string& victimName);

void processMultiPelletWorldHit(
    const WeaponDefinition& def,
    const glm::vec3& hitEnd,
    const glm::vec3& worldNml,
    const glm::vec3& pelletDir,
    float pelletNearest,
    Player& shooter,
    bool& anyHitWorld,
    float& nearestPelletDist,
    glm::vec3& lastPelletEnd,
    glm::vec3& lastHitNormal);

void finalizeMultiPelletResult(
    RevolverShotResult& outResult,
    const glm::vec3& muzzlePos,
    const glm::vec3& lastPelletEnd,
    const glm::vec3& lastHitNormal,
    float accumulatedDamage,
    bool anyHitEntity,
    bool anyHitWorld,
    uint32_t lastTargetId,
    const glm::vec3& accumulatedKnockback,
    int totalPellets,
    const WeaponDefinition& def,
    Player& shooter);

void setWeaponDebug(bool enabled);
bool weaponDebugEnabled();

} // namespace WeaponFire

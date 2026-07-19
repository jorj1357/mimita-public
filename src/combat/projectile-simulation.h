// 07 19 2026, 09 29
/* purpose
* Shared deterministic projectile physics declarations.
* Defines projectile state, config, collision results, and world query interface.
* Used by server authority, client prediction, and deterministic tests.
* Does NOT send packets, apply damage, spawn effects, or own weapon policy.
* Does NOT define map geometry ownership or networking delivery.
* Does NOT decide whether an impact becomes an explosion.
*/

#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "physics/physics-types.h"

// ── Shared projectile simulation ─────────────────────────────────────
// Pure physics: gravity, drag, angular velocity, collision, bounce.
// No game logic (ammo, damage, effects, packets, weapon names).
// Called by both server (authoritative) and client (predicted).

struct ProjectilePhysicsState
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity{0.0f};
    float age = 0.0f;
    int bounceCount = 0;
    bool exploded = false;
    bool sleeping = false;
};

struct ProjectilePhysicsConfig
{
    float speed = 0.0f;
    float gravity = 0.0f;
    float drag = 0.0f;
    float angularDrag = 0.0f;
    float angSpeed = 6.0f;
    float radius = 0.3f;
    float lifetime = 5.0f;
    float restitution = 0.0f;
    float friction = 0.0f;
    float armingDistance = 0.3f;
    int maxBounceCount = 0;
    float minBounceSpeed = 0.0f;
    bool bounceEnabled = false;
};

struct SweptPlayerCapsule
{
    uint32_t playerId = 0;
    uint32_t spawnGeneration = 0;
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
    float radius = 0.0f;
};

enum class ProjectileCollisionType : uint8_t
{
    None,
    WorldBounce,
    WorldImpact,
    PlayerImpact,
    LifetimeExpired
};

struct ProjectileStepResult
{
    ProjectileCollisionType type = ProjectileCollisionType::None;
    glm::vec3 hitPosition{0.0f};
    glm::vec3 hitNormal{0.0f, 0.0f, 1.0f};
    float impactSpeed = 0.0f;
    float travelDistance = 0.0f;
    uint32_t hitPlayerId = 0;
    uint32_t hitPlayerSpawnGeneration = 0;
};

struct CollisionWorldView
{
    virtual ~CollisionWorldView() = default;
    virtual void queryTrianglesSwept(
        const glm::vec3& from, const glm::vec3& to, float radius,
        std::vector<int>& outIndices) const = 0;
    virtual const CollisionTriangle& triangleAt(int index) const = 0;
    virtual int triangleCount() const = 0;
    virtual void queryPlayerCapsulesSwept(
        const glm::vec3& from, const glm::vec3& to, float radius,
        std::vector<SweptPlayerCapsule>& out) const = 0;
};

ProjectileStepResult simulateProjectileTick(
    ProjectilePhysicsState& state,
    const ProjectilePhysicsConfig& config,
    const CollisionWorldView& world,
    float fixedDt);

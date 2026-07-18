#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// ── Shared projectile simulation ─────────────────────────────────────
// Pure physics: gravity, drag, angular velocity, collision, bounce, fuse.
// No game logic (ammo, damage, effects, packets).
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
    float upBias = 0.0f;
    float armingDistance = 0.3f;
    int maxBounceCount = 0;
    float minBounceSpeed = 0.0f;
};

// Abstract collision world interface (server and client provide adapters)
struct CollisionTriangle
{
    glm::vec3 a{0.0f}, b{0.0f}, c{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
};

struct CollisionWorldView
{
    virtual ~CollisionWorldView() = default;
    // Gather candidate triangle indices near a sphere at `center` with `radius`
    virtual void querySphereTriangles(
        const glm::vec3& center, float radius,
        std::vector<int>& outIndices) const = 0;
    // Access triangle by index
    virtual const CollisionTriangle& triangleAt(int index) const = 0;
    // Total triangle count
    virtual int triangleCount() const = 0;
};

// Step one projectile forward by `fixedDt` seconds (typically 1/60).
// Returns true if the projectile is still alive after this step.
// Sets `state.exploded = true` when lifetime expires.
bool simulateProjectileTick(
    ProjectilePhysicsState& state,
    const ProjectilePhysicsConfig& config,
    const CollisionWorldView& world,
    float fixedDt);

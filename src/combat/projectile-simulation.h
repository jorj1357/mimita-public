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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
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
    uint32_t triangleQueryCount = 0;
    uint32_t triangleCandidateTotal = 0;
    uint32_t triangleCandidateMax = 0;
    uint32_t playerCapsuleCandidateTotal = 0;
    uint32_t playerCapsuleCandidateMax = 0;
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

// ── Shared splash line-of-sight helpers ──────────────────────────────
// Used identically by the server (authority) and the client (prediction) so the
// predicted blast feedback matches the authoritative splash verdict.
// Blocking rule: only non-floor surfaces (walls/columns/cover, |normal.z| <= 0.7)
// can occlude; floors/ceilings never block, so a grenade on the ground still
// splashes someone standing next to it.
// Targets are the victim's REAL body parts (head/arms/legs/torso boxes), never
// an invisible movement capsule — a hand poking around a corner can be hit.

// One body-part box in world space (an axis-aligned AABB).
struct SplashBodyPartBox
{
    glm::vec3 center{0.0f};
    glm::vec3 half{0.0f};
};

// Nearest world-space point on the victim's body to the blast: the closest point
// on the NEAREST body-part AABB. Returns true when a part was found and writes
// the ray target into outPoint (the point splash line-of-sight is tested to).
inline bool splashNearestBodyPartPoint(const glm::vec3& blast,
                                       const SplashBodyPartBox* parts,
                                       int count,
                                       glm::vec3& outPoint)
{
    bool found = false;
    float best = std::numeric_limits<float>::max();
    for (int i = 0; i < count; ++i)
    {
        const glm::vec3 mn = parts[i].center - parts[i].half;
        const glm::vec3 mx = parts[i].center + parts[i].half;
        glm::vec3 p;
        p.x = std::clamp(blast.x, mn.x, mx.x);
        p.y = std::clamp(blast.y, mn.y, mx.y);
        p.z = std::clamp(blast.z, mn.z, mx.z);
        const float d = glm::length(p - blast);
        if (d < best)
        {
            best = d;
            outPoint = p;
            found = true;
        }
    }
    return found;
}

// Möller–Trumbore ray test over candidate triangle indices. Returns true when a
// blocking surface lies strictly between origin and target (t in [0.15, maxDist-0.25]).
inline bool splashRayBlockedByWall(const glm::vec3& origin,
                                   const glm::vec3& dir,
                                   float maxDist,
                                   const std::vector<int>& candidates,
                                   const std::vector<CollisionTriangle>& triangles)
{
    for (int ti : candidates)
    {
        if (ti < 0 || ti >= (int)triangles.size())
            continue;
        const CollisionTriangle& tri = triangles[ti];
        if (std::fabs(tri.normal.z) > 0.7f)
            continue; // floors/ceilings never block splash
        const glm::vec3 e1 = tri.b - tri.a;
        const glm::vec3 e2 = tri.c - tri.a;
        const glm::vec3 p = glm::cross(dir, e2);
        const float det = glm::dot(e1, p);
        if (std::fabs(det) < 0.0001f)
            continue;
        const float invDet = 1.0f / det;
        const glm::vec3 tvec = origin - tri.a;
        const float u = glm::dot(tvec, p) * invDet;
        if (u < 0.0f || u > 1.0f)
            continue;
        const glm::vec3 q = glm::cross(tvec, e1);
        const float v = glm::dot(dir, q) * invDet;
        if (v < 0.0f || u + v > 1.0f)
            continue;
        const float t = glm::dot(e2, q) * invDet;
        if (t > 0.15f && t < maxDist - 0.25f)
            return true;
    }
    return false;
}

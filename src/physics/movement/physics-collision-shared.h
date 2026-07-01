#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "physics/physics-types.h"
#include "physics/config.h"
#include "entities/player.h"
#include "physics/movement/physics-collision.h"

class Player;
class CollisionTriangle;

// =====================================================
// Shared helper declarations extracted from
// physics-collision.cpp
// =====================================================

inline bool isFiniteVec3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline void projectVelocityAgainstNormal(Player& p, const glm::vec3& normal)
{
    glm::vec3* velocities[] =
    {
        &p.vel,
        &p.externalImpulse
    };

    for (glm::vec3* v : velocities)
    {
        float into = glm::dot(*v, normal);

        if (into >= 0.0f)
            continue;

        *v -= normal * into;
    }
}

inline void clampVelocityAgainstNormal(Player& p, const glm::vec3& normal)
{
    projectVelocityAgainstNormal(p, normal);
}

void applyCollisionContact(
    Player& p,
    bool& groundedThisFrame,
    const glm::vec3& normal,
    glm::vec3 point,
    float penetration,
    int triangleIndex,
    const char* label
);

void applyTouchResets(Player& p);

void recoverInvalidPlayerCollisionState(Player& p, const glm::vec3& frameStart, const char* phase);

bool rejectBelowBlockTopContact(
    const Capsule& cap,
    const AABB& block,
    const RecoveryContact& contact);

void appendUniqueTriangleIndices(std::vector<int>& dst, const std::vector<int>& src);

// =====================================================
// AABB helpers
// =====================================================

inline AABB makeSweptCapsuleAABB(const Capsule& cap, const glm::vec3& move)
{
    glm::vec3 mn = glm::min(glm::min(cap.a, cap.b), glm::min(cap.a + move, cap.b + move));
    glm::vec3 mx = glm::max(glm::max(cap.a, cap.b), glm::max(cap.a + move, cap.b + move));
    return {mn - glm::vec3(cap.r), mx + glm::vec3(cap.r)};
}

inline AABB makeTriangleAABB(const CollisionTriangle& tri)
{
    return {
        glm::min(glm::min(tri.a, tri.b), tri.c),
        glm::max(glm::max(tri.a, tri.b), tri.c)
    };
}

inline AABB makePlayerAABB(const Player& p)
{
    glm::vec3 half(
        PLAYER_WIDTH  * 0.5f,
        PLAYER_DEPTH  * 0.5f,
        PLAYER_HEIGHT * 0.5f
    );
    return { p.pos - half, p.pos + half };
}

inline bool overlaps(const AABB& a, const AABB& b)
{
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

inline glm::ivec3 collisionChunkCoord(const glm::vec3& p, float size)
{
    return glm::ivec3(
        (int)std::floor(p.x / size),
        (int)std::floor(p.y / size),
        (int)std::floor(p.z / size)
    );
}

// =====================================================
// Sphere-triangle helpers needed by body collision
// =====================================================

bool sweepSphereTriangle(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    const CollisionTriangle& tri,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
);

bool sphereTriangleContact(
    glm::vec3 center,
    float radius,
    const CollisionTriangle& tri,
    Contact& contact
);

// =====================================================
// Triangle gathering helpers needed by stress tests
// =====================================================

std::vector<int> gatherGLBTrianglesForSphere(
    const World& world,
    glm::vec3 center,
    float radius,
    const glm::vec3& move,
    const char* caller = nullptr
);

std::vector<int> gatherGLBTriangles(
    const World& world,
    const Capsule& cap,
    const glm::vec3& move,
    const char* caller = nullptr
);

std::vector<RecoveryContact> collectCapsuleRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<int>& candidates
);

// =====================================================
// Conversion helper for body collision
// =====================================================

inline bool rejectBelowTopFaceContact(
    const Capsule& cap,
    const CollisionTriangle& tri,
    const glm::vec3& normal,
    const glm::vec3& point,
    int triangleIndex,
    const char* phase)
{
    (void)cap; (void)tri; (void)normal; (void)point; (void)triangleIndex; (void)phase;
    return false;
}

// =====================================================
// Collision trace snapshot (used by stress tests + summary)
// =====================================================

struct CollisionTraceSnapshot
{
    glm::vec3 startPos{0.0f};
    glm::vec3 finalPos{0.0f};
    glm::vec3 inputMove{0.0f};
    int initialCandidates = 0;
    int maxCandidates = 0;
    int sweepIterations = 0;
    int sweepHits = 0;
    int maxSimultaneousTOI = 0;
    int maxSlideContacts = 0;
    int maxRecoveryContacts = 0;
    int finalContacts = 0;
    int finalSafetyContacts = 0;
    int resweepHits = 0;
    int faceHits = 0;
    int edgeHits = 0;
    int vertexHits = 0;
    float maxPenetration = 0.0f;
    bool emergencyEscaped = false;
};

extern CollisionTraceSnapshot gLastCollisionTrace;

// =====================================================
// Body / weapon capsule helpers
// =====================================================

struct BodyWeaponSphere {
    glm::vec3 center;
    float radius;
    const char* label;
    glm::vec3 sweepDelta;
};

void recomputeWeaponCapsule(Player& p);
void collectWeaponConfigSpheres(Player& p, std::vector<BodyWeaponSphere>& spheres);
std::vector<BodyWeaponSphere> collectBodyWeaponSpheres(Player& p);
std::vector<RecoveryContact> collectBodyWeaponContacts(
    const Player& p,
    const World& world,
    const std::vector<int>& candidates,
    const std::vector<BodyWeaponSphere>& spheres
);
std::vector<glm::vec3> collectPlayerBodyCollisionSamples(Player& p);

// =====================================================
// GLB collision pipeline
// =====================================================

void doGLBTriangleCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
);

std::vector<RecoveryContact> collectGLBRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<glm::vec3>& bodySamples,
    const std::vector<int>& candidates,
    float bodySampleRadius
);

// =====================================================
// Block collision functions
// =====================================================

bool capsuleSweep(
    const Capsule& cap,
    const glm::vec3& move,
    const AABB& block,
    float& hitTime,
    glm::vec3& hitNormal
);

bool capsuleVsBlock(
    const Capsule& cap,
    const AABB& block,
    glm::vec3& correction,
    bool& outGrounded
);

std::vector<RecoveryContact> collectBlockContactsForCapsule(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks
);

bool findBlockFallbackEscape(
    const Capsule& cap,
    const std::vector<Block*>& nearbyBlocks,
    const std::vector<RecoveryContact>& contacts,
    const glm::vec3& weightedNormal,
    glm::vec3& outCorrection
);

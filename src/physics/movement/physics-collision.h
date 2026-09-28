// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-collision.cpp
// feb 10 2026
// Purpose:
// - Handle ALL solid world collisions
// - No slope logic
// - No audio
// - No input handling
// - Pure positional correction + grounded detection
//
// Exposes:
//   doCollisions(...)
//   resolveCapsuleVsCapsule(...)

// purpose:
// declaration for solid world collision resolution
// implementation lives in physics-collision.cpp

#pragma once

#include <string>
#include <vector>

#include "physics/physics-types.h"
#include "physics/movement/movement-types.h"

class Block;
class Player;
class World;

struct RecoveryContact
{
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 point{0.0f};
    glm::vec3 sweepDelta{0.0f};
    float penetration = 0.0f;
    int triangleIndex = -1;
    const Block* block = nullptr;
    const char* label = "recovery";
    // Sweep parameter of the crossing for swept actor-mesh contacts in [0,1].
    // 0 for a current-pose overlap. Appended so existing aggregate initializers
    // keep working unchanged.
    float timeOfImpact = 0.0f;
    // Support entity for a moving-entity contact (0 = static world) and the
    // surface velocity of that entity. Appended; existing initializers omit them.
    uint32_t entityId = 0;
    glm::vec3 surfaceVelocity{0.0f};
    float surfaceMass = 0.0f;
    float surfaceRestitution = 0.0f;
    // Rounded response normal for actor-mesh feature contacts. `normal` stays
    // the exact world-triangle normal used for depenetration; this normal is
    // used only for movement response so an edge/vertex behaves like a rounded
    // feature instead of a zero-radius snag point.
    glm::vec3 responseNormal{0.0f, 0.0f, 1.0f};
};

// Fixed movement-geometry smoothness. This is deliberately code-owned: it
// rounds both actor and world triangle features and is not the configurable
// broadphase skin in collision.json.
inline constexpr float MOVEMENT_FEATURE_SMOOTHNESS = 0.1f;

// =====================================================
// Canonical contact adapters
// =====================================================
// MovementContact is the one canonical contact vocabulary (see movement-types.h).
// Producer-specific results (RecoveryContact, SweepHit, ActorWorldContact) are
// converted through these adapters instead of reconstructing contact facts
// independently at every call site. Adapters are additive; callers migrate
// gradually and unchanged callers keep their old behavior.

// Maps a producer label ("head", "leftArm", "weapon", "glb-recovery", ...) to
// the canonical actor subshape.
MovementSubshape movementSubshapeFromLabel(const char* label);

// Converts a RecoveryContact into the canonical contact. surfaceId is derived
// from triangleIndex, subshape from label, and sweepVelocity from sweepDelta.
MovementContact movementContactFromRecoveryContact(
    const RecoveryContact& recovery,
    MovementContactKind kind,
    MovementContactSource source,
    MovementShapeKind shapeKind,
    uint64_t simulationTick,
    MovementLifecycleIdentity targetLifecycle,
    uint32_t materialId = 0);

// Converts a SweepHit into the canonical contact. A sweep hit carries no
// penetration, so penetrationDepth is zero and timeOfImpact is unused here.
MovementContact movementContactFromSweepHit(
    const SweepHit& hit,
    MovementContactKind kind,
    MovementContactSource source,
    MovementShapeKind shapeKind,
    uint64_t simulationTick,
    MovementLifecycleIdentity targetLifecycle,
    const glm::vec3& sweepVelocity = glm::vec3(0.0f),
    const glm::vec3& surfaceVelocity = glm::vec3(0.0f),
    uint32_t materialId = 0);

// Resolves ALL solid block collisions (no slopes)
// - Mutates player position & velocity
// - Sets groundedThisFrame if standing on something
// - No input, no audio, no gravity
void doCollisions(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt
);

std::string collisionLastTraceSummary();
std::string collisionStateSummary(const class Player& p);
std::string collisionStressRun(const std::string& caseName);
bool collisionStressSelfTest(std::string* outSummary = nullptr);
bool collisionSubGridSelfTest(std::string* outSummary = nullptr);
// Deterministic check that the canonical contact adapters preserve producer
// metadata and that the new canonical fields do not change contact identity.
bool canonicalContactSelfTest(std::string* outSummary = nullptr);

// Resolve collision between two capsules (e.g., player vs NPC)
// - Mutates positions of both capsules
// - Returns true if collision was resolved
bool resolveCapsuleVsCapsule(
    Player& a,
    Player& b,
    bool& groundedA,
    bool& groundedB
);

// Gather candidate world triangles within an AABB using chunk spatial hashing.
// Used by root capsule collision and NPC line-of-sight / navigation.
void appendChunkTrianglesForAABB(
    const World& world,
    const AABB& queryBounds,
    float expansion,
    std::vector<int>& out,
    const char* caller = nullptr
);

// Thin-ray DDA: traverse grid cells along the ray in near-to-far order.
// Tests ALL triangles in each cell, tracks the closest hit.
// Stops when the next cell boundary is farther than the closest known hit.
// More efficient than querying the entire ray AABB — visits only cells
// actually touched by the ray.
bool rayTraverseGridCells(
    const World& world,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    float maxDist,
    float& hitDist,
    glm::vec3* outNormal = nullptr
);

// Thick-ray (swept-sphere) DDA: traverse grid cells along the centerline
// in near-to-far order, also visiting neighboring cells within the beam radius.
// Tests all unique triangles using sweptSphereTriangle.
// Stops when the next cell boundary is farther than the closest known hit.
// Preserves exact beam-thickness collision behavior.
bool sweptSphereTraverseGridCells(
    const World& world,
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    float radius,
    float& hitDistance,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
);

// Swept-sphere vs triangle test (needed by collision traversal and weapon code).
// Returns false if no intersection within maxDist.
bool sweptSphereTriangle(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float radius,
    const CollisionTriangle& tri,
    float maxDist,
    float& hitDist,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
);

// Batched multi-contact depenetration solver.
// Collects all penetrating contacts and solves them simultaneously using
// Gauss-Seidel iteration. Returns a single correction vector that satisfies
// all contact constraints.
glm::vec3 solveBatchedCorrection(
    const std::vector<RecoveryContact>& contacts,
    float slop,
    float* outMaxPenetration = nullptr,
    glm::vec3* outWeightedNormal = nullptr,
    glm::vec3 intendedMove = glm::vec3(0.0f),
    glm::vec3 debugPosition = glm::vec3(0.0f)
);



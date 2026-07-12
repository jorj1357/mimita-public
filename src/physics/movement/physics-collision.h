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

class Block;
class Player;
class World;

struct RecoveryContact
{
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 point{0.0f};
    float penetration = 0.0f;
    int triangleIndex = -1;
    const Block* block = nullptr;
    const char* label = "recovery";
};

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

// Traverse grid cells intersected by a ray, in near-to-far order.
// Tests triangles in each cell and returns the closest hit distance.
// Returns true if a hit was found, with hitDist set to the closest intersection.
// This is more efficient than querying the entire ray AABB because it only
// visits cells actually touched by the ray and stops at the first hit.
bool rayTraverseGridCells(
    const World& world,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    float maxDist,
    float& hitDist
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



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

#include "physics/physics-types.h"

class Player;
class World;

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
    std::vector<int>& out
);



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

// Resolve collision between two capsules (e.g., player vs NPC)
// - Mutates positions of both capsules
// - Returns true if collision was resolved
bool resolveCapsuleVsCapsule(
    Player& a,
    Player& b,
    bool& groundedA,
    bool& groundedB
);

// Authoritative body-part collision resolution.
// Runs once per frame (not per substep) to push body colliders back from
// world geometry. A fraction of the push is applied to the root capsule.
// Call after doCollisions() completes for the frame.
void resolveBodyPartCollisions(
    Player& p,
    const World& world,
    float dt
);

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

// purpose:
// declaration for solid world collision resolution
// implementation lives in physics-collision.cpp

#pragma once

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

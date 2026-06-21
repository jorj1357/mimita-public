// Unified horizontal movement: ground = instant snap, air = CS-style acceleration.
// Handles external impulse steering for knockback.
// Uses physics/config.h
// Only modifies p.vel.x / p.vel.y

#pragma once

#include <glm/vec2.hpp>

class Player;

// Handles horizontal walk / air steer
// - Uses physics/config.h
// - Debug heavy
// - No friction
// - No dash
// - No collision
// - No gravity
// - Only modifies p.vel.x / p.vel.y
void doWalk(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool jumpHeld,
    float dt
);

// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-walk.cpp
// feb 10 2026
/**
 * purpose
 * all logic for walking goes here
 * literallt just
 * wasd = forward back left right
 * exposes walk(direction, velocity) to other files 
 */

#pragma once

#include <glm/vec2.hpp>

class Player;

// Handles horizontal walk — ground only. No air acceleration.
// - Uses physics/config.h
// - No friction
// - No dash
// - No collision
// - No gravity
// - Only modifies p.vel.x / p.vel.y while grounded
void doWalk(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool onGround,
    float dt
);

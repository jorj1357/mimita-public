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
    float dt
);

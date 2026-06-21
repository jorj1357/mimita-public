// C:\important\quiet\n\mimita-priv-v7\src\physics\physics-mini.h
// feb 10 2026
/**
 * purpose
 * header so that
 * the rewrite twhere 
 * 1 big phsics file = 12 littel movement files
 * is better
 */

#pragma once

#include <glm/glm.hpp>

struct Player;
struct World;
struct InputState;

void physicsMainUpdate(
    Player& player,
    const World& world,
    const InputState& input,
    float dt
);

// Movement subsystem functions (called from physics-mini.cpp)
void doAirDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool triggerPressed,
    bool movementPressed,
    bool airborne,
    int movementTicks,
    float dt,
    const glm::vec3& camForward
);
void doDownDash(Player& p, bool triggerPressed, float dt);
void doJump(Player& p, bool jumpHeld, float dt);
void doWalk(Player& p, const glm::vec2& wishMoveXY, bool movementPressed, float dt);
void doGravity(Player& p, float dt);
void doGroundReturn(Player& p, bool triggerPressed, float dt);
void doFreeze(Player& p, bool freezeHeld, float dt, bool& wasFrozen);
void doFriction(Player& p, bool movementPressed, float dt);

// Collision
void doCollisions(Player& p, const struct World& world, bool& groundedThisFrame, float dt);

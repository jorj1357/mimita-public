// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-friction.h
// feb 10 2026
// purpose
// header for phsics friciton cpp

#pragma once
#include <glm/glm.hpp>

class Player;

// Handles ALL friction + drag
// - Ground friction
// - Air friction
// - Dash drag
// - Velocity cleanup
//
// No movement input
// No collision logic
// No audio
void doFriction(
    Player& p,
    bool onGround,
    float dt
);

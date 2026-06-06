// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-dash.h
// feb 10 2026
/**
 * piurpose
 * handle alll logic for plr dash
 * wasd, omni directional, etc
 * hold w + dash = dash forward, press anu other wasd keu = quit low air friction,
 * put high ground friciton 
 */

#pragma once
#include <glm/glm.hpp>

class Player;

// CHANGED: Dash is now a single-frame impulse applied directly to vel, jun 6 2026
// No persistent dashVel state, no cancelDashVelocity needed.
// Friction and walk naturally take over on subsequent frames.
void doDash(
    Player& p,
    const glm::vec2& wishMoveXY,
    bool dashPressed,
    const glm::vec3& camForward,
    float dt
);

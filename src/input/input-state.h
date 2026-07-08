// C:\important\quiet\n\mimita-priv-v7\src\input\input-state.h
// feb 10 2026
/**
 * purpose
 * Dumb data carrier
 * No GLFW, no math
 * headers and function declare for 
 * when the plauer is inputting keys
 * FINALLT WE JUST PUT GLFW_KEY_WASD IN HERE NOWHERE ELSE i tihnk.
 */

// input_state.h
#pragma once
#include <glm/glm.hpp>

struct InputState {
    glm::vec2 wishMoveXY{0};
    bool jumpHeld = false;
    bool jumpPressed = false;
    bool dashPressed = false;
    bool movementPressed = false;
    bool movementJustPressed = false;
    bool groundReturnPressed = false;
    bool downDashPressed = false;
    bool freezeHeld = false;
    float movementHeldDuration = 0.0f;
    glm::vec3 camForward{0,0,1};
};

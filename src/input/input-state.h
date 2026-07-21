// 07 21 2026, 16 30
/* purpose
* Defines the current local input state consumed by physics and simulation.
* Carries camera-relative movement, held controls, and edge-triggered gameplay actions.
* Keeps hardware polling output separate from movement formulas and Player state.
* Does NOT poll GLFW, send network packets, run physics, or own key bindings.
* Does NOT store UI focus, replay history, presentation flags, or authority decisions.
* Does NOT duplicate movement command serialization or shared movement state.
*/

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
    bool freezePressed = false;
    float movementHeldDuration = 0.0f;
    glm::vec3 camForward{0,0,1};
};

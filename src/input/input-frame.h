// 07 21 2026, 16 30
/* purpose
* Defines one local/replay input frame for fixed-step simulation.
* Carries movement axes, look angles, held inputs, and edge-triggered gameplay actions.
* Provides a compact data boundary between input polling, replay, terminal pulses, and simulation.
* Does NOT poll hardware, send packets, validate authority, or run movement physics.
* Does NOT own input bindings, buffering policy, or command serialization.
* Does NOT store presentation, UI, audio, or weapon runtime state.
*/

#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct InputFrame {
    float moveX = 0.0f;
    float moveY = 0.0f;
    bool jump = false;
    bool jumpPressed = false;
    bool dashPressed = false;
    bool movementPressed = false;
    bool movementJustPressed = false;
    bool reloadPressed = false;
    bool groundReturnPressed = false;
    bool downDashPressed = false;
    bool freezeHeld = false;
    bool freezePressed = false;
    float lookYaw = 0.0f;
    float lookPitch = 0.0f;
};

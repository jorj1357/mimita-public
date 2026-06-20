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
    float lookYaw = 0.0f;
    float lookPitch = 0.0f;
};

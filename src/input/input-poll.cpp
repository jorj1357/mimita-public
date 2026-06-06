// C:\important\quiet\n\mimita-priv-v7\src\input\input-state.h
// feb 10 2026
// /**
//  * ONLY translate GLFW -> InputState
//     Print edges, not spam   
// 
// #pragma message("COMPILING input-poll.cpp")

// input_poll.cpp
#include "input/input-state.h"
#include "input/input-commands.h"
#include "input/input-frame.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include "camera.h"

static bool gInputInitialized = false;

static void ensureInputInit(GLFWwindow* win) {
    if (!gInputInitialized) {
        InputCommandSystem::instance().init(win);
        gInputInitialized = true;
    }
}

InputState pollInput(GLFWwindow* win, const Camera& cam)
{
    ensureInputInit(win);
    InputCommandSystem::instance().update(0.016f);
    
    InputState in;
    in.camForward = cam.front;

    glm::vec3 f = cam.front;
    f.z = 0.0f;
    if (glm::length(f) > 0.0001f)
        f = glm::normalize(f);

    glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0,0,1)));

    glm::vec2 wish(0.0f);

    const auto& cmd = InputCommandSystem::instance();
    
    if (cmd.getState("move_forward").held) {
        wish.x += f.x;
        wish.y += f.y;
    }
    if (cmd.getState("move_back").held) {
        wish.x -= f.x;
        wish.y -= f.y;
    }
    if (cmd.getState("move_right").held) {
        wish.x += r.x;
        wish.y += r.y;
    }
    if (cmd.getState("move_left").held) {
        wish.x -= r.x;
        wish.y -= r.y;
    }

    in.wishMoveXY = wish;
    
    in.jumpHeld = cmd.isJumpHeld();
    in.dashPressed = cmd.isDashPressed();
    in.groundReturnPressed = cmd.isGroundReturnPressed();
    in.freezeHeld = cmd.isFreezeHeld();

    if (in.dashPressed)
        printf("[INPUT] Dash pressed\n");

    return in;
}

InputFrame buildInputFrame(GLFWwindow* win, const Camera& cam)
{
    ensureInputInit(win);
    InputCommandSystem::instance().update(0.016f);

    const auto& cmd = InputCommandSystem::instance();

    glm::vec3 f = cam.front;
    f.z = 0.0f;
    if (glm::length(f) > 0.0001f)
        f = glm::normalize(f);
    glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0,0,1)));

    InputFrame frame;
    if (cmd.getState("move_forward").held) { frame.moveX += f.x; frame.moveY += f.y; }
    if (cmd.getState("move_back").held)    { frame.moveX -= f.x; frame.moveY -= f.y; }
    if (cmd.getState("move_right").held)   { frame.moveX += r.x; frame.moveY += r.y; }
    if (cmd.getState("move_left").held)    { frame.moveX -= r.x; frame.moveY -= r.y; }

    frame.jump = cmd.isJumpHeld();
    frame.jumpPressed = cmd.getState("jump").pressed;
    frame.dashPressed = cmd.isDashPressed();
    frame.groundReturnPressed = cmd.isGroundReturnPressed();
    frame.freezeHeld = cmd.isFreezeHeld();

    frame.lookYaw = cam.yaw;
    frame.lookPitch = cam.pitch;

    return frame;
}
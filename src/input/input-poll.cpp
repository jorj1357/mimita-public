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
InputFrame gTerminalInputOverride;

void consumeTerminalInputOverride() {
    gTerminalInputOverride = InputFrame{};
}

static void ensureInputInit(GLFWwindow* win) {
    if (!gInputInitialized) {
        InputCommandSystem::instance().init(win);
        InputCommandSystem::instance().loadBinds("config/accounts/default.json");
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
    
    if (cmd.getState("walkforward").held) {
        wish.x += f.x;
        wish.y += f.y;
    }
    if (cmd.getState("walkback").held) {
        wish.x -= f.x;
        wish.y -= f.y;
    }
    if (cmd.getState("walkright").held) {
        wish.x += r.x;
        wish.y += r.y;
    }
    if (cmd.getState("walkleft").held) {
        wish.x -= r.x;
        wish.y -= r.y;
    }

    // 6 6 2026 also add this for normaliz 
    if (glm::length(wish) > 0.0001f)
        wish = glm::normalize(wish);

    in.wishMoveXY = wish;

    // 6 6 2026 for fixing cam moving with wads but not moving walk diection ? 
    in.movementPressed =
        cmd.getState("walkforward").held ||
        cmd.getState("walkback").held ||
        cmd.getState("walkleft").held ||
        cmd.getState("walkright").held;

    in.movementJustPressed =
        cmd.getState("walkforward").pressed ||
        cmd.getState("walkback").pressed ||
        cmd.getState("walkleft").pressed ||
        cmd.getState("walkright").pressed;
    
    in.movementHeldDuration = cmd.getMovementHoldDuration();
    in.jumpHeld = cmd.isJumpHeld();
    in.jumpPressed = cmd.getState("jump").pressed;
    in.dashPressed = cmd.isDashPressed();
    in.groundReturnPressed = cmd.isGroundReturnPressed();
    in.downDashPressed = cmd.isDownDashPressed();
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

    // 6 6 2026 normalize again 
    glm::vec2 move(frame.moveX, frame.moveY);

    if (glm::length(move) > 0.0001f)
    {
        move = glm::normalize(move);

        frame.moveX = move.x;
        frame.moveY = move.y;
    }

    if (cmd.getState("walkforward").held) { frame.moveX += f.x; frame.moveY += f.y; }
    if (cmd.getState("walkback").held)    { frame.moveX -= f.x; frame.moveY -= f.y; }
    if (cmd.getState("walkright").held)   { frame.moveX += r.x; frame.moveY += r.y; }
    if (cmd.getState("walkleft").held)    { frame.moveX -= r.x; frame.moveY -= r.y; }
    // frame.movementPressed =
    //     cmd.getState("walkforward").pressed || cmd.getState("walkback").pressed ||
    //     cmd.getState("walkleft").pressed || cmd.getState("walkright").pressed;

    frame.movementPressed =
        cmd.getState("walkforward").held ||
        cmd.getState("walkback").held ||
        cmd.getState("walkleft").held ||
        cmd.getState("walkright").held;

    frame.movementJustPressed =
        cmd.getState("walkforward").pressed ||
        cmd.getState("walkback").pressed ||
        cmd.getState("walkleft").pressed ||
        cmd.getState("walkright").pressed;

    frame.jump = cmd.isJumpHeld() || gTerminalInputOverride.jump;
    frame.jumpPressed = cmd.getState("jump").pressed || gTerminalInputOverride.jumpPressed;
    frame.dashPressed = cmd.isDashPressed() || gTerminalInputOverride.dashPressed;
    frame.reloadPressed = cmd.getState("reload").pressed || gTerminalInputOverride.reloadPressed;
    frame.groundReturnPressed = cmd.isGroundReturnPressed() || gTerminalInputOverride.groundReturnPressed;
    frame.downDashPressed = cmd.isDownDashPressed() || gTerminalInputOverride.downDashPressed;
    frame.freezeHeld = cmd.isFreezeHeld() || gTerminalInputOverride.freezeHeld;

    if (gTerminalInputOverride.moveX != 0.0f || gTerminalInputOverride.moveY != 0.0f) {
        frame.moveX = gTerminalInputOverride.moveX;
        frame.moveY = gTerminalInputOverride.moveY;
    }

    frame.lookYaw = (gTerminalInputOverride.lookYaw != 0.0f) ? gTerminalInputOverride.lookYaw : cam.yaw;
    frame.lookPitch = (gTerminalInputOverride.lookPitch != 0.0f) ? gTerminalInputOverride.lookPitch : cam.pitch;

    consumeTerminalInputOverride();
    return frame;
}

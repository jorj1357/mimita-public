// C:\important\quiet\n\mimita-priv-v7\src\input\input-state.h
// feb 10 2026
// /**
//  * ONLY translate GLFW → InputState
//     Print edges, not spam   

// #pragma message("COMPILING input-poll.cpp")

// input_poll.cpp
#include "input/input-state.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include "camera.h"

InputState pollInput(GLFWwindow* win, const Camera& cam)
{
    static bool prevDash = false;
    static bool prevGroundReturn = false;

    InputState in;
    in.camForward = cam.front;

    // camera relative movement

    glm::vec3 f = cam.front;

    // remove vertical component so looking up/down doesn't affect movement
    f.z = 0.0f;

    if (glm::length(f) > 0.0001f)
        f = glm::normalize(f);

    // right vector on ground plane
    glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0,0,1)));

    glm::vec2 wish(0.0f);

    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
    {
        wish.x += f.x;
        wish.y += f.y;
    }

    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
    {
        wish.x -= f.x;
        wish.y -= f.y;
    }

    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
    {
        wish.x += r.x;
        wish.y += r.y;
    }

    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
    {
        wish.x -= r.x;
        wish.y -= r.y;
    }

    in.wishMoveXY = wish;
    
    // movement keys handled elsewhere or later
    in.jumpHeld = glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;

    bool dashHeld = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    in.dashPressed = dashHeld && !prevDash;
    prevDash = dashHeld;

    bool grHeld = glfwGetKey(win, GLFW_KEY_B) == GLFW_PRESS;
    in.groundReturnPressed = grHeld && !prevGroundReturn;
    prevGroundReturn = grHeld;

    // freeze function key
    in.freezeHeld = glfwGetKey(win, GLFW_KEY_G) == GLFW_PRESS;

    if (in.dashPressed)
        printf("[INPUT] Dash pressed\n");

    // add printfs for dash and ground return and other stuff,
    // also their states e.g. velocity before during after
    // are sounds playing etc 
    // what sepcifcally am i colliding with etc etc 

    return in;
}

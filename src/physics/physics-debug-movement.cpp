// C:\important\quiet\n\mimita-public\mimita-public\src\physics\physics-debug-movement.cpp
// dec 17 2025
/**
 * purpose
 * has the debug movement so we can test things easier
 * tp up tp down tp forward etc
 * dont include in real game or do idk 
 */

#include "physics-debug-movement.h"
#include "entities/player.h"
#include <GLFW/glfw3.h>
#include "camera.h"
#include <cstdio>

void applyDebugMovement(Player& p, GLFWwindow* win, const Camera& cam, float dt)
{
    static bool debugEnabled = false;
    static bool keyHeldPrev = false;

    // --------------------------------------------------
    // TOGGLE WITH 9
    // --------------------------------------------------

    bool keyHeld = glfwGetKey(win, GLFW_KEY_9) == GLFW_PRESS;

    if (keyHeld && !keyHeldPrev)
    {
        debugEnabled = !debugEnabled;

        if (debugEnabled)
            printf("[DEBUG MOVEMENT] ENABLED\n");
        else
            printf("[DEBUG MOVEMENT] DISABLED\n");
    }

    keyHeldPrev = keyHeld;

    // --------------------------------------------------
    // DO NOTHING IF DEBUG MODE OFF
    // --------------------------------------------------

    if (!debugEnabled)
        return;

    const float vmult = 10.0f;
    const float speed = 5.0f * vmult;

    glm::vec3 forward = cam.front;
    forward.z = 0.0f;

    if (glm::length(forward) > 0.0001f)
        forward = glm::normalize(forward);

    // --------------------------------------------------
    // DEBUG MOVEMENT
    // --------------------------------------------------

    // up
    if (glfwGetKey(win, GLFW_KEY_T))
        p.vel.z = speed;

    // down
    if (glfwGetKey(win, GLFW_KEY_B))
        p.vel.z = -speed;

    // reset position
    if (glfwGetKey(win, GLFW_KEY_R))
        p.pos = glm::vec3(0.0f, 0.0f, 80.0f);

    // stop velocity
    if (glfwGetKey(win, GLFW_KEY_V))
        p.vel = glm::vec3(0.0f);

    // move forward
    if (glfwGetKey(win, GLFW_KEY_H))
        p.pos += forward * speed * dt;
}
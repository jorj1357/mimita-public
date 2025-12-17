// C:\important\quiet\n\mimita-public\mimita-public\src\physics\physics-debug-movement.cpp
// dec 17 2025
/**
 * purpose
 * has the debug movement so we can test things easier
 * tp up tp down tp forward etc
 * dont include in real game or do idk 
 */

// dec 17 2025

#include "physics-debug-movement.h"
#include "entities/player.h"
#include <GLFW/glfw3.h>
#include "camera.h"

void applyDebugMovement(Player& p, GLFWwindow* win, const Camera& cam, float dt)
{
    glm::vec3 forward = cam.front;
    forward.y = 0.0f;
    if (glm::length(forward) > 0.0001f)
        forward = glm::normalize(forward);

    if (glfwGetKey(win, GLFW_KEY_T))
        p.pos.y += 5.0f * dt;

    if (glfwGetKey(win, GLFW_KEY_B))
        p.pos.y -= 5.0f * dt;

    if (glfwGetKey(win, GLFW_KEY_G))
        p.pos += forward * (5.0f * dt);
}

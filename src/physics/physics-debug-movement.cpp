// C:\important\quiet\n\mimita-public\mimita-public\src\physics\physics-debug-movement.cpp
// dec 17 2025
/**
 * purpose
 * has the debug movement so we can test things easier
 * tp up tp down tp forward etc
 * dont include in real game or do idk 
 */

// DEC 18 2025
// WORKING DONT BREAK IT DONT BREAK 

#include "physics-debug-movement.h"
#include "entities/player.h"
#include <GLFW/glfw3.h>
#include "camera.h"

void applyDebugMovement(Player& p, GLFWwindow* win, const Camera& cam, float dt)
{
    const int vmult = 10.0f;

    glm::vec3 forward = cam.front;
    // z is up dec 19 2025 
    forward.z = 0.0f;
    if (glm::length(forward) > 0.0001f)
        forward = glm::normalize(forward);

    // dont mult with dt dec 18 2025 ? 
    if (glfwGetKey(win, GLFW_KEY_T))
        p.vel.z = 5.0f * vmult;

    if (glfwGetKey(win, GLFW_KEY_B))
        p.vel.z = -5.0f * vmult;

    // dec 18 2025 broken whatevr 
    // if (glfwGetKey(win, GLFW_KEY_G))
    //     p.vel += forward * (5.0f * vmult);
    
    // if (glfwGetKey(win, GLFW_KEY_G))
    //     p.vel.x = forward * (5.0f * vmult);

    // no plus? just eqwuals?
    // if (glfwGetKey(win, GLFW_KEY_G))
    //     p.vel = forward * (5.0f * vmult);

    // no forward = 0? just definte it as 1?
    // if (glfwGetKey(win, GLFW_KEY_G))
    //     p.vel += 1.0f * (5.0f * vmult);

    // idk how to fix this 
    if (glfwGetKey(win, GLFW_KEY_G))
        p.vel += forward * (5.0f * vmult);

    if (glfwGetKey(win, GLFW_KEY_R))
    // dec 19 2025 z is up reset 
        p.pos = glm::vec3(0.0f, 0.0f, 50.0f);
}

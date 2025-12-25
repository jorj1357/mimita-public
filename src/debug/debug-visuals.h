// C:\important\quiet\n\mimita-public\mimita-public\src\debug\debug-visuals.h
// dec 24 2025
/**
 * purpose
 * header for debug visals file 
 */

#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

struct DebugColors {
    glm::vec3 playerCapsule   = {1.0f, 0.0f, 0.0f}; // red
    glm::vec3 collisionBox    = {1.0f, 1.0f, 0.0f}; // yellow
    glm::vec3 worldChunks     = {0.0f, 1.0f, 0.0f}; // green
    glm::vec3 lookVector      = {0.0f, 0.5f, 1.0f}; // blue
};

namespace DebugVis {
    void init(GLFWwindow* win);
    void update();                 // handles key toggle
    bool enabled();

    const DebugColors& colors();
}
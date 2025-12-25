// C:\important\quiet\n\mimita-public\mimita-public\src\debug\archive\debug-visuals.cpp
// dec 24 2025
/**
 * purpose
 * so we CAN FINALLT SEE
 * WHATTHE HECKS GOING ON 
 * BC I DIDNT HAVE THESE AT ALL BEFORE
 */

 // C:\important\quiet\n\mimita-public\mimita-public\src\debug\archive\debug-visuals.cpp
// dec 24 2025

#include "debug-visuals.h"

namespace {
    bool gEnabled = false;
    bool last0 = false;
    GLFWwindow* gWindow = nullptr;
    DebugColors gColors;
}

void DebugVis::init(GLFWwindow* win) {
    gWindow = win;
}

void DebugVis::update() {
    if (!gWindow) return;

    bool key0 = glfwGetKey(gWindow, GLFW_KEY_0) == GLFW_PRESS;
    if (key0 && !last0) {
        gEnabled = !gEnabled;
    }
    last0 = key0;
}

bool DebugVis::enabled() {
    return gEnabled;
}

const DebugColors& DebugVis::colors() {
    return gColors;
}

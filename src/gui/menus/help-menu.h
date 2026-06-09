#pragma once
#include <GLFW/glfw3.h>

struct HelpMenuResult {
    bool goBack = false;
};

HelpMenuResult drawHelpMenu(GLFWwindow* win);

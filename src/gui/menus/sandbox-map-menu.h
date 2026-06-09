#pragma once

#include <GLFW/glfw3.h>
#include <string>

struct SandboxMapMenuResult
{
    bool startSandbox = false;
    bool goBack = false;
    std::string mapPath;
};

void sandboxMapMenuSetActive(bool active);
void sandboxMapMenuSetLoadResult(const std::string& message, bool success);
SandboxMapMenuResult drawSandboxMapMenu(GLFWwindow* win);

#pragma once
#include <GLFW/glfw3.h>

struct PracticeMenuResult
{
    bool goSandbox = false;
    bool goBack = false;
};

PracticeMenuResult drawPracticeMenu(GLFWwindow* win);

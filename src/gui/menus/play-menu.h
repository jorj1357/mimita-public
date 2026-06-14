#pragma once
#include <GLFW/glfw3.h>

struct PlayMenuResult
{
    bool goDuels = false;
    bool goOnline = false;
    bool goPractice = false;
    bool goBack = false;
};

PlayMenuResult drawPlayMenu(GLFWwindow* win);

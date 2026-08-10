#pragma once
#include <GLFW/glfw3.h>

struct PlayMenuResult
{
    bool goDuels = false;
    bool goQueueDuels = false;
    bool goBombTag = false;
    bool goOnline = false;
    bool goPractice = false;
    bool goBack = false;
    bool goCompetitive = false;
    bool goCompetitiveSignIn = false;
};

PlayMenuResult drawPlayMenu(GLFWwindow* win);

#pragma once
#include <GLFW/glfw3.h>
#include <string>

struct OnlineMenuResult
{
    bool startSandbox = false;
    bool startTimeTrials = false;
    bool startPractice = false;
    bool startDuel = false;
    bool startServer = false;
    bool stopServer = false;
    bool connectToServer = false;
    std::string connectAddress;
    bool goBack = false;
};

OnlineMenuResult drawOnlineMenu(GLFWwindow* win);
void onlineMenuSetActive(bool active);
void onlineMenuHandleChar(unsigned int codepoint);
void onlineMenuHandleKey(int key, int action);

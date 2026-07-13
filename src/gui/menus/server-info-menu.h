#pragma once
#include <GLFW/glfw3.h>

struct ServerInfoResult
{
    bool connect = false;
    bool startServer = false;
    bool goBack = false;
};

ServerInfoResult drawServerInfoMenu(GLFWwindow* win,
                                    char* serverAddress,
                                    bool serverRunning);
void serverInfoMenuSetActive(bool active);
void serverInfoMenuHandleChar(unsigned int codepoint);
void serverInfoMenuHandleKey(int key, int action, int mods);

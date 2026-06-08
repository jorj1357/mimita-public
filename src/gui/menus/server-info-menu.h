#pragma once
#include <GLFW/glfw3.h>

struct ServerInfoResult
{
    bool connect = false;
    bool startServer = false;
    bool goBack = false;
};

ServerInfoResult drawServerInfoMenu(GLFWwindow* win,
                                    const char* serverAddress,
                                    bool serverRunning);

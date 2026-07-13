#pragma once

#include <GLFW/glfw3.h>

struct LoginMenuResult
{
    bool signedIn = false;
    bool goBack = false;
    bool createAccount = false;
};

LoginMenuResult drawLoginMenu(GLFWwindow* window);
void loginMenuSetActive(bool active);

#pragma once

#include <GLFW/glfw3.h>

struct AccountPanelAction
{
    bool logIn = false;
    bool signUp = false;
    bool switchAccount = false;
    bool logOut = false;
};

AccountPanelAction drawAccountPanel(GLFWwindow* window);

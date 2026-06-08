#pragma once

#include <GLFW/glfw3.h>

struct SignInMenuResult
{
    bool signedIn = false;
    bool goBack = false;
};

SignInMenuResult drawSignInMenu(GLFWwindow* window);
void signInMenuHandleChar(unsigned int codepoint);
void signInMenuHandleKey(int key, int action);
void signInMenuSetActive(bool active);


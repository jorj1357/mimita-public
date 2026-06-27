#pragma once

#include <GLFW/glfw3.h>

enum class AuthPopupAction
{
    None,
    LogIn,
    CreateAccount,
    ContinueOffline,
};

AuthPopupAction drawAuthPopup(GLFWwindow* window);
void authPopupHandleChar(unsigned int codepoint);
void authPopupHandleKey(int key, int action);
bool authPopupIsInCodeInput();
void authPopupReset();

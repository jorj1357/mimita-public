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

#include "gui/menus/sign-in-menu.h"

#include "gui/gui-back.h"
#include "gui/gui-button.h"
#include "gui/ui-system.h"
#include "profile/local-profile-system.h"

#include <string>

namespace {

bool active = false;
bool passwordFocused = false;
std::string username;
std::string password;
std::string message;

void appendCharacter(std::string& target, unsigned int codepoint)
{
    if (codepoint >= 32 && codepoint <= 126 && target.size() < 31)
        target.push_back((char)codepoint);
}

}

void signInMenuSetActive(bool value)
{
    active = value;
    if (active)
        message.clear();
}

void signInMenuHandleChar(unsigned int codepoint)
{
    if (!active)
        return;
    appendCharacter(passwordFocused ? password : username, codepoint);
}

void signInMenuHandleKey(int key, int action)
{
    if (!active || (action != GLFW_PRESS && action != GLFW_REPEAT))
        return;
    if (key == GLFW_KEY_TAB)
        passwordFocused = !passwordFocused;
    else if (key == GLFW_KEY_BACKSPACE)
    {
        std::string& target = passwordFocused ? password : username;
        if (!target.empty())
            target.pop_back();
    }
    else if (key == GLFW_KEY_ENTER)
    {
        if (LocalProfileSystem::instance().signIn(username, password))
            message = "Signed in";
        else
            message = LocalProfileSystem::instance().lastError();
    }
}

SignInMenuResult drawSignInMenu(GLFWwindow* window)
{
    SignInMenuResult result{};
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    const float centerX = width * 0.5f;

    uiDrawRect({0, 0, (float)width, (float)height},
               {0.035f, 0.04f, 0.052f, 1.0f}, "sign-in-background");
    uiDrawText("Local Dev Sign In", centerX - 130.0f, 120.0f, 0.7f,
               {0.95f, 0.98f, 1.0f, 1.0f});
    uiDrawText("LocalProfileSystem: passwords are stored as plain text for development.",
               centerX - 300.0f, 180.0f, 0.3f, {1.0f, 0.72f, 0.3f, 1.0f});

    uiDrawText("Username", centerX - 210.0f, 245.0f, 0.34f, {0.8f,0.86f,0.95f,1});
    uiDrawRect({centerX - 210.0f, 275.0f, 420.0f, 48.0f},
               passwordFocused ? glm::vec4(0.12f,0.13f,0.16f,1)
                               : glm::vec4(0.18f,0.22f,0.28f,1),
               "sign-in-username");
    uiDrawText(username.empty() ? "_" : username.c_str(),
               centerX - 196.0f, 288.0f, 0.38f, {1,1,1,1});

    uiDrawText("Password", centerX - 210.0f, 350.0f, 0.34f, {0.8f,0.86f,0.95f,1});
    uiDrawRect({centerX - 210.0f, 380.0f, 420.0f, 48.0f},
               passwordFocused ? glm::vec4(0.18f,0.22f,0.28f,1)
                               : glm::vec4(0.12f,0.13f,0.16f,1),
               "sign-in-password");
    std::string masked(password.size(), '*');
    uiDrawText(masked.empty() ? "_" : masked.c_str(),
               centerX - 196.0f, 393.0f, 0.38f, {1,1,1,1});

    if (guiButton(window, "Sign In", centerX - 120.0f, 470.0f, 240.0f, 58.0f,
                  {0.2f,0.7f,1.0f,1.0f}))
    {
        result.signedIn = LocalProfileSystem::instance().signIn(username, password);
        message = result.signedIn
            ? "Signed in as " + LocalProfileSystem::instance().currentUsername()
            : LocalProfileSystem::instance().lastError();
    }

    if (!message.empty())
        uiDrawText(message.c_str(), centerX - 170.0f, 550.0f, 0.36f,
                   result.signedIn ? glm::vec4(0.3f,1,0.4f,1)
                                   : glm::vec4(1,0.3f,0.3f,1));

    if (guiBackButton(window))
        result.goBack = true;
    return result;
}


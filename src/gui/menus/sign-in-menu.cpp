#include "gui/menus/sign-in-menu.h"

#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/ui-system.h"
#include "gui/gui-coord.h"
#include "profile/local-profile-system.h"

#include <string>

namespace {

bool active = false;
bool passwordFocused = false;
std::string gUsername;
std::string gPassword;
std::string gMessage;

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
        gMessage.clear();
}

void signInMenuHandleChar(unsigned int codepoint)
{
    if (!active) return;
    appendCharacter(passwordFocused ? gPassword : gUsername, codepoint);
}

void signInMenuHandleKey(int key, int action)
{
    if (!active || (action != GLFW_PRESS && action != GLFW_REPEAT)) return;
    if (key == GLFW_KEY_TAB)
        passwordFocused = !passwordFocused;
    else if (key == GLFW_KEY_BACKSPACE)
    {
        std::string& target = passwordFocused ? gPassword : gUsername;
        if (!target.empty()) target.pop_back();
    }
    else if (key == GLFW_KEY_ENTER)
    {
        if (LocalProfileSystem::instance().signIn(gUsername, gPassword))
            gMessage = "Signed in";
        else
            gMessage = LocalProfileSystem::instance().lastError();
    }
}

SignInMenuResult drawSignInMenu(GLFWwindow* window)
{
    SignInMenuResult result{};
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/sign-in-menu.json");

    // Draw all static layout elements
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        // Skip dynamic elements drawn below
        if (id == "usernameLabel" || id == "passwordLabel")
        {
            // These are drawn as static text, just call drawGuiElement
            drawGuiElement(window, *elem);
            continue;
        }

        UIButtonState s = drawGuiElement(window, *elem);
        if (!s.clicked) continue;

        if (id == "signIn")
        {
            result.signedIn = LocalProfileSystem::instance().signIn(gUsername, gPassword);
            gMessage = result.signedIn
                ? "Signed in as " + LocalProfileSystem::instance().currentUsername()
                : LocalProfileSystem::instance().lastError();
        }
        else if (id == "backButton")
            result.goBack = true;
    }

    // Dynamic input fields (need runtime state)
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    {
        // Username input background
        glm::vec4 uBg = passwordFocused
            ? glm::vec4(0.12f, 0.13f, 0.16f, 1)
            : glm::vec4(0.18f, 0.22f, 0.28f, 1);
        UIRect uRect = cs.designToScreen({750, 275, 420, 48});
        uiDrawRect(uRect, uBg, "input-username-bg");

        // Username text
        const char* uText = gUsername.empty() ? "_" : gUsername.c_str();
        uiDrawText(uText, cs.designToScreenX(764), cs.designToScreenY(288), 0.38f, {1,1,1,1});

        // Password input background
        glm::vec4 pBg = passwordFocused
            ? glm::vec4(0.18f, 0.22f, 0.28f, 1)
            : glm::vec4(0.12f, 0.13f, 0.16f, 1);
        UIRect pRect = cs.designToScreen({750, 380, 420, 48});
        uiDrawRect(pRect, pBg, "input-password-bg");

        // Masked password text
        std::string masked(gPassword.size(), '*');
        const char* pText = masked.empty() ? "_" : masked.c_str();
        uiDrawText(pText, cs.designToScreenX(764), cs.designToScreenY(393), 0.38f, {1,1,1,1});
    }

    // Message text (dynamic)
    if (!gMessage.empty())
    {
        glm::vec4 msgColor = result.signedIn
            ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f)
            : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
        uiDrawText(gMessage.c_str(), cs.designToScreenX(790), cs.designToScreenY(550),
                   0.36f, msgColor);
    }

    return result;
}
